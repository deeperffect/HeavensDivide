#include "SwapPresentationComponent.h"
#include "SwapPortal.h"
#include "AutoAttackComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "PlayerCameraRig.h"
#include "GameFramework/PlayerController.h"

void USwapPresentationComponent::StartSwapFreeze()
{
    auto* Character=Cast<ACharacterBase>(GetOwner());
    UWorld* World=GetWorld();
    if(bSwapFreezeActive || !Character || !World || SwapFreezeDuration<=0 || UGameplayStatics::IsGamePaused(World)) return;
    auto* Settings=World->GetWorldSettings();
    PreviousTimeDilation=Settings->TimeDilation;
    // Do not take ownership of an existing level-up/death freeze.
    if(PreviousTimeDilation<=.001f) return;
    bSwapFreezeActive=true;
    FreezeRemaining=FMath::Clamp(SwapFreezeDuration,0.f,1.f);
    FreezeTotalDuration=FreezeRemaining;
    ActiveFreezeEaseInDuration=FMath::Clamp(FreezeEaseInDuration,0.f,FreezeTotalDuration);
    AppliedTimeDilation=Settings->SetTimeDilation(ActiveFreezeEaseInDuration>0 ? PreviousTimeDilation : .0001f);
    PreviousAnimationRate=Character->GetMesh()->GlobalAnimRateScale;
    // Keep the current rate until our next tick: this frame's delta may still
    // have been computed at the old world speed. Always update before the mesh.
    Character->GetMesh()->AddTickPrerequisiteComponent(this);
    FreezeTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this](float Delta) { return UpdateSwapFreeze(Delta); }));
}

void USwapPresentationComponent::UpdateFreezeAnimationRate(float GameDelta,float RealDelta)
{
    if(GameDelta<=0 || RealDelta<=0) return;
    if(auto* Character=Cast<ACharacterBase>(GetOwner()))
        Character->GetMesh()->GlobalAnimRateScale=PreviousAnimationRate*RealDelta/GameDelta;
    const float WorldDelta=GetWorld()->GetDeltaSeconds();
    if(WorldDelta>0)
        for(auto& Effect:FreezeEffects)
            if(Effect.IsValid()) Effect->SetCustomTimeDilation(RealDelta/WorldDelta);
}

bool USwapPresentationComponent::UpdateSwapFreeze(float RealDelta)
{
    if(!bSwapFreezeActive) return false;
    if(!bEnabled || !GetWorld()) { FinishSwapFreeze(); return false; }
    // Pausing the game should not spend the entrance window behind the menu.
    if(UGameplayStatics::IsGamePaused(GetWorld())) return true;
    auto* Settings=GetWorld()->GetWorldSettings();
    // Do not overwrite time changes made by another system during the ramp.
    if(!FMath::IsNearlyEqual(Settings->TimeDilation,AppliedTimeDilation,SMALL_NUMBER))
    { FinishSwapFreeze(false); return false; }
    FreezeRemaining-=RealDelta;
    if(FreezeRemaining<=0) { FinishSwapFreeze(false); return false; }
    const float Alpha=ActiveFreezeEaseInDuration>0 ? FMath::Clamp((FreezeTotalDuration-FreezeRemaining)/ActiveFreezeEaseInDuration,0.f,1.f) : 1.f;
    const float SmoothAlpha=Alpha*Alpha*(3.f-2.f*Alpha);
    AppliedTimeDilation=Settings->SetTimeDilation(FMath::Lerp(PreviousTimeDilation,.0001f,SmoothAlpha));
    return true;
}

void USwapPresentationComponent::FinishSwapFreeze(bool bCancelEntrance)
{
    if (bCancelEntrance && ArrivalPortalActor.IsValid()) ArrivalPortalActor->Destroy();
    bArrivalPending=false;
    if(bCancelEntrance) StopEntrance();
    if(bCancelEntrance)
        if(auto* Character=Cast<ACharacterBase>(GetOwner()))
            if(auto* Controller=Cast<APlayerController>(Character->GetController()))
                if(auto* Rig=Cast<APlayerCameraRig>(Controller->GetViewTarget()))
                    if(Rig->GetFollowTarget()==Character) Rig->StopSwapFocus();
    if(!bSwapFreezeActive) return;
    bSwapFreezeActive=false;
    FTSTicker::GetCoreTicker().RemoveTicker(FreezeTicker);
    FreezeTicker.Reset();
    if(auto* Character=Cast<ACharacterBase>(GetOwner()))
    {
        Character->GetMesh()->GlobalAnimRateScale=PreviousAnimationRate;
        Character->GetMesh()->RemoveTickPrerequisiteComponent(this);
    }
    if(UWorld* World=GetWorld())
    {
        auto* Settings=World->GetWorldSettings();
        if(FMath::IsNearlyEqual(Settings->TimeDilation,AppliedTimeDilation,SMALL_NUMBER))
            Settings->SetTimeDilation(PreviousTimeDilation);
    }
    for(auto& Effect:FreezeEffects)
        if(Effect.IsValid())
        {
            Effect->SetCustomTimeDilation(1.f);
            Effect->RemoveTickPrerequisiteComponent(this);
        }
    FreezeEffects.Reset();
    FreezeRemaining=0;
    if(Attack.IsValid() && !GetOwner()->IsActorBeingDestroyed()) Attack->StartAutoAttack();
}
