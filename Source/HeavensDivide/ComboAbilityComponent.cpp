#include "ComboAbilityComponent.h"
#include "ComboAbilityDefinition.h"
#include "JointComboActor.h"
#include "NinjaCharacter.h"
#include "Animation/AnimInstance.h"
#include "AutoAttackComponent.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "InactiveCharacterAssistComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivorPlayerController.h"
#include "SwapPresentationComponent.h"
#include "PlayerUpgradeComponent.h"
#include "RunLoadoutSubsystem.h"
#include "Engine/GameInstance.h"

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
float UComboAbilityComponent::GetComboPercent() const { return IsComboEquipped() ? FMath::Clamp(CurrentCombo / GetMaxCombo(), 0.f, 1.f) : 0.f; }
bool UComboAbilityComponent::IsFull() const { return IsComboEquipped() && CurrentCombo >= GetMaxCombo(); }
bool UComboAbilityComponent::IsComboUnlocked() const
{
    const auto* PC=Cast<ASurvivorPlayerController>(GetOwner());
    const auto* Upgrades=PC?PC->GetPlayerUpgrades():nullptr;
    // The explicit BalanceLab full-meta profile supplies this bonus without editing the player's save.
    return Upgrades && Upgrades->GetMetaSkillBonus(TEXT("ComboUnlock"))>0.f;
}
bool UComboAbilityComponent::IsComboEquipped() const
{
    if (!IsComboUnlocked()) return false;
    const auto* PC = Cast<ASurvivorPlayerController>(GetOwner());
    const auto* Upgrades = PC ? PC->GetPlayerUpgrades() : nullptr;
    // BalanceLab's full-meta preview can exercise the ability without modifying menu equipment.
    if (Upgrades && Upgrades->BalancePreviewMetaProfile >= 0) return true;
    const auto* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const auto* Loadout = GI ? GI->GetSubsystem<URunLoadoutSubsystem>() : nullptr;
    return Loadout && (!Loadout->GetComboAbilityFor(EUpgradeStatTarget::Samurai).IsNone() || !Loadout->GetComboAbilityFor(EUpgradeStatTarget::Ninja).IsNone());
}
FName UComboAbilityComponent::GetActiveAbilityId() const
{
    const auto* PC = Cast<ASurvivorPlayerController>(GetOwner());
    const auto* Hero = PC && PC->GetCharacterManager() ? PC->GetCharacterManager()->GetActiveCharacter() : nullptr;
    const auto* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const auto* Loadout = GI ? GI->GetSubsystem<URunLoadoutSubsystem>() : nullptr;
    if (PC && PC->GetPlayerUpgrades() && PC->GetPlayerUpgrades()->BalancePreviewMetaProfile >= 0) return TEXT("BloodMoon");
    return Hero && Loadout ? Loadout->GetComboAbilityFor(Hero->IsA<ANinjaCharacter>() ? EUpgradeStatTarget::Ninja : EUpgradeStatTarget::Samurai) : NAME_None;
}
FText UComboAbilityComponent::GetActiveAbilityName() const
{
    if (const auto* Entry = ComboCatalog::Find(GetActiveAbilityId())) return FText::FromString(Entry->Name);
    return FText::FromString(TEXT("No combo"));
}
bool UComboAbilityComponent::CanRun() const
{
    const auto* PC = Cast<ASurvivorPlayerController>(GetOwner());
    return PC && PC->IsRunInProgress() && !PC->IsPlayerDead() && IsComboEquipped();
}
void UComboAbilityComponent::SetCombo(float Amount)
{
    const float Value = FMath::IsFinite(Amount) ? FMath::Clamp(Amount, 0.f, GetMaxCombo()) : 0.f;
    if (CurrentCombo == Value) return;
    CurrentCombo = Value;
    OnComboChanged.Broadcast(CurrentCombo, GetMaxCombo(), GetComboPercent());
}
void UComboAbilityComponent::RestoreCombo(float Amount) { SetCombo(IsComboEquipped()?Amount:0.f); }
void UComboAbilityComponent::AddCombo(float Amount)
{
    if (CanRun() && FMath::IsFinite(Amount) && Amount > 0.f)
    {
        const auto* Upgrades=CastChecked<ASurvivorPlayerController>(GetOwner())->GetPlayerUpgrades();
        const float Multiplier=1.f+FMath::Max(0.f,Upgrades->GetMetaSkillBonus(TEXT("ComboCharge")));
        SetCombo(CurrentCombo+FMath::Min(Amount*Multiplier,GetMaxCombo()));
    }
}
void UComboAbilityComponent::NotifySynergyAttack() { AddCombo(SynergyGain); }
void UComboAbilityComponent::HandleSwap(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter)
{
    if (OldCharacter && NewCharacter && OldCharacter != NewCharacter) AddCombo(SwapGain);
}
void UComboAbilityComponent::HandleAssist(ACharacterBase* Assistant, ACharacterBase* Active, AEnemyBase* Target)
{
    if (Assistant && Active && Target) AddCombo(AssistGain);
}
bool UComboAbilityComponent::CanActivateAbility() const
{
    const auto* PC = Cast<ASurvivorPlayerController>(GetOwner());
    const auto* Party = PC ? PC->GetCharacterManager() : nullptr;
    const auto* Character = Party ? Party->GetActiveCharacter() : nullptr;
    return CanRun() && IsFull() && !IsAbilityActive() && GetWorld() && !UGameplayStatics::IsGamePaused(this)
        && !PC->IsSelectingUpgrade() && IsValid(Character) && !GetActiveAbilityId().IsNone()
        && (ComboCatalog::Definition(GetActiveAbilityId()) && ComboCatalog::Definition(GetActiveAbilityId())->IsJoint())
        && Character->GetCharacterMode() == ECharacterMode::Active && !Character->IsDashing()
        && (!Character->SwapPresentation || !Character->SwapPresentation->IsBlockingAttacks());
}
bool UComboAbilityComponent::TryActivateAbility()
{
    if (!CanActivateAbility()) return false;
    auto* PC = CastChecked<ASurvivorPlayerController>(GetOwner());
    auto* Character = PC->GetCharacterManager()->GetActiveCharacter();
    ActiveTechnique = ComboCatalog::Definition(GetActiveAbilityId());
    AbilityCharacter = Character;
    Character->bComboAbilityActive = true;
    if (auto* Attack = Character->FindComponentByClass<UAutoAttackComponent>()) Attack->StopAutoAttack();
    bOwnsFacingOverride = FaceEnemyPack(Character);
    if (auto* Anim = Character->GetMesh()->GetAnimInstance()) Anim->Montage_Stop(.1f);
    const float Power = 1.f + FMath::Max(0.f, PC->GetPlayerUpgrades()->GetMetaSkillBonus(TEXT("ComboPower")));
    ExecutingJoint = AJointComboActor::Spawn(Character, ActiveTechnique, Power);
    if (!ExecutingJoint.IsValid()) { FinishAbility(false); return false; }
    AbilityDuration = ExecutingJoint->GetControlDuration();
    SetCombo(0.f);
    OnAbilityActivated.Broadcast(Character);
    return true;
}
void UComboAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!IsAbilityActive()) return;
    if (!CanRun() || !ExecutingJoint.IsValid()) { FinishAbility(true); return; }
    if (UGameplayStatics::IsGamePaused(this)) return;
    if (ExecutingJoint->GetAge() >= AbilityDuration) FinishAbility(false);
}
void UComboAbilityComponent::FinishAbility(bool bCancelled)
{
    auto Joint = ExecutingJoint;
    ExecutingJoint.Reset();
    if (Joint.IsValid()) {
        if (bCancelled) Joint->Destroy();
        else Joint->ReleasePerformers();
    }
    auto* Character = AbilityCharacter.Get();
    AbilityCharacter.Reset();
    ActiveTechnique = nullptr;
    if (!Character) return;
    Character->bComboAbilityActive = false;
    if (auto* Assist = GetOwner()->FindComponentByClass<UInactiveCharacterAssistComponent>()) Assist->RefreshAssistEffectState();
    if (bOwnsFacingOverride) Character->ClearFacingOverride();
    bOwnsFacingOverride = false;
    if (!bCancelled && CanRun() && Character->GetCharacterMode() == ECharacterMode::Active)
        if (auto* Attack = Character->FindComponentByClass<UAutoAttackComponent>()) Attack->StartAutoAttack();
}
