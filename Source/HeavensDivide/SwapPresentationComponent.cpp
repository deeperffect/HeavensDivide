#include "SwapPresentationComponent.h"
#include "SwapAfterimage.h"
#include "AutoAttackComponent.h"
#include "NinjaCharacter.h"
#include "SurvivorAbilityComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Containers/Ticker.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/App.h"
#include "PlayerCameraRig.h"
#include "GameFramework/PlayerController.h"

USwapPresentationComponent::USwapPresentationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Ghost(TEXT("/Game/HeavensDivide/Materials/M_SwapGhost"));
    GhostMaterial = Ghost.Object;
    static ConstructorHelpers::FObjectFinder<USoundBase> Whoosh(TEXT("/Game/Assets/Sounds/Ninja/freesound_community-knife-swish-1-82559"));
    SwapWhoosh = Whoosh.Object;
    static ConstructorHelpers::FObjectFinder<USoundBase> SamuraiSound(TEXT("/Game/Assets/Sounds/Samurai/Swing1"));
    static ConstructorHelpers::FObjectFinder<USoundBase> NinjaSound(TEXT("/Game/Assets/Sounds/Ninja/freesound_community-knife-draw-48223"));
    DefaultSamuraiSound=SamuraiSound.Object; DefaultNinjaSound=NinjaSound.Object;
}
FLinearColor USwapPresentationComponent::GetPresentationColor() const
{
    if (!bUseCharacterColor) return CustomColor;
    return Cast<ANinjaCharacter>(GetOwner()) ? FLinearColor(.45f,.12f,1) : FLinearColor(1,.12f,.08f);
}
void USwapPresentationComponent::SpawnEffect(UNiagaraSystem* System)
{
    if (!System || !GetOwner()) return;
    auto* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System,
        GetOwner()->GetActorLocation()+VFXOffset, GetOwner()->GetActorRotation(), FVector(VFXScale), true, false);
    if(Effect)
    {
        Effect->SetVariableLinearColor(TEXT("User.SwapColor"), GetPresentationColor());
        if(bSwapFreezeActive)
        {
            Effect->SetForceSolo(true);
            // World dilation may have changed midway through this frame.
            Effect->SetCustomTimeDilation(1.f);
            Effect->AddTickPrerequisiteComponent(this);
            FreezeEffects.Add(Effect);
        }
        Effect->Activate();
    }
}
void USwapPresentationComponent::PlayDeparture()
{
    ClearReady(); StopEntrance();
    if (!bEnabled || GetNetMode()==NM_DedicatedServer) return;
    auto* Character = Cast<ACharacterBase>(GetOwner());
    if (!Character) return;
    if((DepartureMontage || bEnableAfterimage) && GhostMaterial)
    {
        FActorSpawnParameters Params; Params.Owner=Character;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if(auto* Ghost=GetWorld()->SpawnActor<ASwapAfterimage>(Character->GetActorLocation(),Character->GetActorRotation(),Params))
        {
            if(!DepartureMontage || !Ghost->InitializeDeparture(Character,GhostMaterial,GetPresentationColor(),DepartureMontage,DeparturePlayRate,AfterimageDuration))
            {
                if(bEnableAfterimage && !Ghost->IsActorBeingDestroyed()) Ghost->Initialize(Character,GhostMaterial,GetPresentationColor(),AfterimageDuration);
                else Ghost->Destroy();
            }
        }
    }
    SpawnEffect(DepartureVFX);
}
void USwapPresentationComponent::PlayArrival()
{
    bArrivalPending=false;
    if (!bEnabled || GetNetMode()==NM_DedicatedServer) return;
    auto* Character=Cast<ACharacterBase>(GetOwner());
    if(!Character) return;
    Attack=Character->FindComponentByClass<UAutoAttackComponent>();
    // Activation schedules an attack immediately. Keep its cooldown, but clear
    // stale attack state/montages before giving the entrance its own window.
    if(Attack.IsValid()) Attack->StopAutoAttack();
    if(auto* Anim=Character->GetMesh()->GetAnimInstance()) Anim->Montage_Stop(0.f);
    StartSwapFreeze();
    SpawnEffect(ArrivalVFX);
    if(!ArrivalVFX && bUseFallbackArrivalRing)
        if(auto* Ring=GetWorld()->SpawnActor<AAbilityAccent>(Character->GetActorLocation()+VFXOffset,FRotator::ZeroRotator))
            Ring->Initialize(FVector::ZeroVector,110*VFXScale,GetPresentationColor(),.25f,false);
    if(bEnableSound)
    {
        if(SwapWhoosh) UGameplayStatics::PlaySoundAtLocation(this,SwapWhoosh,Character->GetActorLocation(),SoundVolume);
        USoundBase* Accent=ArrivalSound;
        if(!Accent) Accent=Cast<ANinjaCharacter>(Character) ? DefaultNinjaSound.Get() : DefaultSamuraiSound.Get();
        if(Accent) UGameplayStatics::PlaySoundAtLocation(this,Accent,Character->GetActorLocation(),SoundVolume*.65f);
    }
    StartEntrance();
    StartArrivalMovement();
    if(bSwapFreezeActive)
        if(auto* Controller=Cast<APlayerController>(Character->GetController()))
            if(Controller->IsLocalController())
                if(auto* Rig=Cast<APlayerCameraRig>(Controller->GetViewTarget()))
                    if(Rig->GetFollowTarget()==Character) Rig->StartSwapFocus();
    if(IsBlockingAttacks())
    {
        // Input queued before the swap and residual velocity must not slide
        // the gameplay character underneath the cosmetic arrival animation.
        Character->ConsumeMovementInputVector();
        Character->GetCharacterMovement()->StopMovementImmediately();
    }
    if(Attack.IsValid()) Attack->StartAutoAttack();
    // Check readiness after the swap delegates have armed Grand Entrance.
    SetComponentTickEnabled(true);
}
void USwapPresentationComponent::ClearReady()
{
    if(ReadyEffect) { ReadyEffect->DeactivateImmediate(); ReadyEffect->DestroyComponent(); }
    ReadyEffect=nullptr; bReadyVisible=false;
}
void USwapPresentationComponent::HandleModeChanged(ECharacterMode Mode)
{
    if(Mode!=ECharacterMode::Active) { FinishSwapFreeze(); ClearReady(); StopEntrance(); SetComponentTickEnabled(false); }
}
void USwapPresentationComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Type,Function);
    auto* Character=Cast<ACharacterBase>(GetOwner());
    if(!bEnabled || !Character || Character->GetCharacterMode()!=ECharacterMode::Active)
    { FinishSwapFreeze(); ClearReady(); StopEntrance(); SetComponentTickEnabled(false); return; }
    if(bSwapFreezeActive) UpdateFreezeAnimationRate(Delta,static_cast<float>(FApp::GetDeltaTime()));
    UpdateArrivalMovement(static_cast<float>(FApp::GetDeltaTime()));
    UpdateEntrance(static_cast<float>(FApp::GetDeltaTime()));
    const bool Ready=bShowReadyGlow && Attack.IsValid() && Attack->GetReadyGrandEntranceUpgrade()!=nullptr;
    if(Ready && !bReadyVisible)
    {
        UMeshComponent* Weapon=nullptr;
        if(!WeaponComponentName.IsNone())
        {
            TInlineComponentArray<UMeshComponent*> Meshes(Character);
            for(auto* Mesh:Meshes) if(Mesh->GetFName()==WeaponComponentName) { Weapon=Mesh; break; }
        }
        else Weapon=Cast<UMeshComponent>(Attack->ResolveWeaponVisualComponent());
        // Some Ninja weapons are spawned projectiles; their hand mesh is a useful fallback.
        if(!Weapon) Weapon=Character->GetMesh();
        if(ReadyVFX) ReadyEffect=UNiagaraFunctionLibrary::SpawnSystemAttached(ReadyVFX,Weapon,NAME_None,FVector::ZeroVector,
            FRotator::ZeroRotator,EAttachLocation::KeepRelativeOffset,false,false);
        if(ReadyEffect) { ReadyEffect->SetVariableLinearColor(TEXT("User.SwapColor"),GetPresentationColor()); ReadyEffect->Activate(); }
        bReadyVisible=true;
    }
    else if(!Ready && bReadyVisible) ClearReady();
    if(!Ready && EntranceRemaining<=0 && !bSwapFreezeActive && !bArrivalMovementActive) SetComponentTickEnabled(false);
}
void USwapPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    FinishSwapFreeze(); ClearReady(); StopEntrance();
    Super::EndPlay(Reason);
}
