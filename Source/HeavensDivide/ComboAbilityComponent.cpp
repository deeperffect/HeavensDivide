#include "ComboAbilityComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AutoAttackComponent.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "InactiveCharacterAssistComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Misc/App.h"
#include "SurvivorPlayerController.h"
#include "SwapPresentationComponent.h"
#include "SurvivorAbilityComponent.h"

UComboAbilityComponent::UComboAbilityComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UComboAbilityComponent::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Party = GetOwner()->FindComponentByClass<UCharacterManagerComponent>())
        Party->OnCharacterSwapped.AddUniqueDynamic(this, &UComboAbilityComponent::HandleSwap);
    if (auto* Assist = GetOwner()->FindComponentByClass<UInactiveCharacterAssistComponent>())
        Assist->OnAssistAttackExecuted.AddUniqueDynamic(this, &UComboAbilityComponent::HandleAssist);
}

void UComboAbilityComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    FinishAbility(true);
    if (auto* Party = GetOwner()->FindComponentByClass<UCharacterManagerComponent>())
        Party->OnCharacterSwapped.RemoveDynamic(this, &UComboAbilityComponent::HandleSwap);
    if (auto* Assist = GetOwner()->FindComponentByClass<UInactiveCharacterAssistComponent>())
        Assist->OnAssistAttackExecuted.RemoveDynamic(this, &UComboAbilityComponent::HandleAssist);
    Super::EndPlay(Reason);
}

float UComboAbilityComponent::GetMaxCombo() const { return FMath::IsFinite(MaxCombo) ? FMath::Max(1.f, MaxCombo) : 100.f; }
float UComboAbilityComponent::GetComboPercent() const { return FMath::Clamp(CurrentCombo / GetMaxCombo(), 0.f, 1.f); }
bool UComboAbilityComponent::IsFull() const { return CurrentCombo >= GetMaxCombo(); }
bool UComboAbilityComponent::CanRun() const
{
    const auto* PC = Cast<ASurvivorPlayerController>(GetOwner());
    return PC && PC->IsRunInProgress() && !PC->IsPlayerDead();
}
void UComboAbilityComponent::SetCombo(float Amount)
{
    const float Value = FMath::IsFinite(Amount) ? FMath::Clamp(Amount, 0.f, GetMaxCombo()) : 0.f;
    if (CurrentCombo == Value) return;
    CurrentCombo = Value;
    OnComboChanged.Broadcast(CurrentCombo, GetMaxCombo(), GetComboPercent());
}
void UComboAbilityComponent::RestoreCombo(float Amount) { SetCombo(Amount); }
void UComboAbilityComponent::AddCombo(float Amount)
{
    if (CanRun() && FMath::IsFinite(Amount) && Amount > 0.f) SetCombo(CurrentCombo + FMath::Min(Amount, GetMaxCombo()));
}
void UComboAbilityComponent::NotifySynergyAttack() { AddCombo(SynergyGain); }
void UComboAbilityComponent::HandleSwap(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter)
{
    if (OldCharacter && NewCharacter && OldCharacter != NewCharacter) AddCombo(SwapGain);
}
void UComboAbilityComponent::HandleAssist(ACharacterBase* Assistant, ACharacterBase* Active, AEnemyBase* Target)
{
    if (Assistant && Active && Target) NotifySynergyAttack();
}
bool UComboAbilityComponent::CanActivateAbility() const
{
    const auto* PC = Cast<ASurvivorPlayerController>(GetOwner());
    const auto* Party = PC ? PC->GetCharacterManager() : nullptr;
    const auto* Character = Party ? Party->GetActiveCharacter() : nullptr;
    return CanRun() && IsFull() && !IsAbilityActive() && GetWorld() && !UGameplayStatics::IsGamePaused(this)
        && !PC->IsSelectingUpgrade() && IsValid(Character) && Character->ComboAbility.bEnabled
        && Character->GetCharacterMode() == ECharacterMode::Active && !Character->IsDashing()
        && (!Character->SwapPresentation || !Character->SwapPresentation->IsBlockingAttacks());
}
bool UComboAbilityComponent::TryActivateAbility()
{
    if (!CanActivateAbility()) return false;
    auto* PC = CastChecked<ASurvivorPlayerController>(GetOwner());
    auto* Character = PC->GetCharacterManager()->GetActiveCharacter();
    ActiveSettings = Character->ComboAbility;
    ActiveSettings.EffectDelay = FMath::Max(0.f, ActiveSettings.EffectDelay);
    ActiveSettings.EffectDuration = FMath::Max(.01f, ActiveSettings.EffectDuration);
    ActiveSettings.PulseInterval = FMath::Max(.02f, ActiveSettings.PulseInterval);
    AbilityCharacter = Character;
    Character->bComboAbilityActive = true;
    AbilityElapsed = 0.f;
    bEffectExecuted = false;
    NextPulseTime = ActiveSettings.EffectDelay;
    AbilityDuration = FMath::Max(ActiveSettings.RecoveryDuration, ActiveSettings.EffectDelay + ActiveSettings.EffectDuration);
    if (auto* Attack = Character->FindComponentByClass<UAutoAttackComponent>()) Attack->StopAutoAttack();
    bOwnsFacingOverride = ActiveSettings.bFaceEnemyPack && FaceEnemyPack(Character);
    if (auto* Anim = Character->GetMesh()->GetAnimInstance())
    {
        Anim->Montage_Stop(.1f);
        if (ActiveSettings.Montage)
            AbilityDuration = FMath::Max(AbilityDuration, Anim->Montage_Play(ActiveSettings.Montage, FMath::Max(.01f, ActiveSettings.MontagePlayRate), EMontagePlayReturnType::Duration));
    }
    bOwnsActivationFreeze = ActiveSettings.bFreezeOnActivation && Character->SwapPresentation
        && Character->SwapPresentation->StartAbilityFreeze(ActiveSettings.FreezeDuration, ActiveSettings.FreezeEaseInDuration);
    if (bOwnsActivationFreeze) AbilityDuration = FMath::Max(AbilityDuration, ActiveSettings.FreezeDuration);
    // The same frame must update ability effects before their Niagara simulation.
    if (Character->SwapPresentation) AddTickPrerequisiteComponent(Character->SwapPresentation);
    SetCombo(0.f);
    OnAbilityActivated.Broadcast(Character);
    if (ActiveSettings.EffectDelay <= 0.f) { ExecuteEffect(); NextPulseTime += ActiveSettings.PulseInterval; }
    return true;
}
void UComboAbilityComponent::ExecuteEffect()
{
    auto* Character = AbilityCharacter.Get();
    if (!Character || !CanRun()) return;
    if (!bEffectExecuted && ActiveSettings.Sound)
        ActiveAudio = UGameplayStatics::SpawnSoundAttached(ActiveSettings.Sound, Character->GetRootComponent(), NAME_None,
            FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true, FMath::Max(0.f, ActiveSettings.SoundVolume));
    if (!bEffectExecuted && ActiveSettings.VFX)
    {
        ActiveVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(ActiveSettings.VFX, Character->GetVisualRoot(), NAME_None,
            ActiveSettings.VFXOffset, ActiveSettings.VFXRotation, EAttachLocation::KeepRelativeOffset, false, false);
        if (ActiveVFX)
        {
            if (Character->SwapPresentation) Character->SwapPresentation->RegisterFreezeEffect(ActiveVFX);
            ActiveVFX->SetRelativeScale3D(ActiveSettings.VFXScale);
            if (!ActiveSettings.VFXDurationParameter.IsNone()) ActiveVFX->SetVariableFloat(ActiveSettings.VFXDurationParameter, ActiveSettings.EffectDuration);
        }
    }
    const float Alpha = FMath::Clamp((NextPulseTime - ActiveSettings.EffectDelay) / ActiveSettings.EffectDuration, 0.f, 1.f);
    const float Radius = FMath::Max(1.f, FMath::Lerp(ActiveSettings.InitialRadius, ActiveSettings.FinalRadius, Alpha));
    if (ActiveVFX)
    {
        if (!ActiveSettings.VFXRadiusParameter.IsNone()) ActiveVFX->SetVariableFloat(ActiveSettings.VFXRadiusParameter, Radius);
        if (!bEffectExecuted) ActiveVFX->Activate();
    }
    bEffectExecuted = true;
    if (!ActiveSettings.VFX)
    {
        if (auto* Accent = GetWorld()->SpawnActor<AAbilityAccent>(Character->GetActorLocation(), FRotator::ZeroRotator))
            Accent->Initialize(Character->GetActorLocation(), Radius, ActiveSettings.FallbackColor, .2f, false);
    }
    Character->ExecuteComboAbility(Radius, FMath::Max(0.f, ActiveSettings.DamagePerPulse));
}
void UComboAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!CanRun()) { FinishAbility(true); SetCombo(0.f); return; }
    if (!IsAbilityActive()) return;
    if (AbilityCharacter->GetCharacterMode() != ECharacterMode::Active) { FinishAbility(true); return; }
    const bool bRealTimeAbility = bOwnsActivationFreeze && AbilityCharacter->SwapPresentation
        && AbilityCharacter->SwapPresentation->IsSwapFreezeActive();
    AbilityElapsed += bRealTimeAbility ? static_cast<float>(FApp::GetDeltaTime()) : DeltaTime;
    while (IsAbilityActive() && CanRun() && NextPulseTime <= AbilityElapsed && NextPulseTime <= ActiveSettings.EffectDelay + ActiveSettings.EffectDuration + KINDA_SMALL_NUMBER)
    {
        ExecuteEffect();
        NextPulseTime += ActiveSettings.PulseInterval;
    }
    if (AbilityElapsed >= AbilityDuration) FinishAbility(false);
}
void UComboAbilityComponent::FinishAbility(bool bCancelled)
{
    if (ActiveVFX) { ActiveVFX->DestroyComponent(); ActiveVFX = nullptr; }
    if (ActiveAudio) { ActiveAudio->Stop(); ActiveAudio = nullptr; }
    auto* Character = AbilityCharacter.Get();
    AbilityCharacter.Reset();
    if (!Character) return;
    if (Character->SwapPresentation)
    {
        RemoveTickPrerequisiteComponent(Character->SwapPresentation);
        if (bOwnsActivationFreeze) Character->SwapPresentation->FinishSwapFreeze(false);
    }
    bOwnsActivationFreeze = false;
    Character->bComboAbilityActive = false;
    if (bOwnsFacingOverride) Character->ClearFacingOverride();
    bOwnsFacingOverride = false;
    if (bCancelled && ActiveSettings.Montage)
        if (auto* Anim = Character->GetMesh()->GetAnimInstance()) Anim->Montage_Stop(.1f, ActiveSettings.Montage);
    if (!bCancelled && CanRun() && Character->GetCharacterMode() == ECharacterMode::Active)
        if (auto* Attack = Character->FindComponentByClass<UAutoAttackComponent>()) Attack->StartAutoAttack();
}
