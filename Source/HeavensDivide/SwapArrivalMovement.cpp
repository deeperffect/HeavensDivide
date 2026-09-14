#include "SwapPresentationComponent.h"
#include "Components/SceneComponent.h"
#include "SamuraiCharacter.h"

void USwapPresentationComponent::StartArrivalMovement()
{
    ResetArrivalMovement();
    auto* Character=Cast<ACharacterBase>(GetOwner());
    if(!Character || !Character->GetVisualRoot()) return;
    bArrivalWalking=Character->IsA<ASamuraiCharacter>();
    if(bArrivalWalking ? (!bEnableSamuraiWalkIn || ArrivalWalkDistance<=0) : (!bEnableNinjaArrivalDrop || ArrivalDropHeight<=0)) return;
    ArrivalBaseLocation=Character->GetVisualRoot()->GetRelativeLocation();
    if(bArrivalWalking)
    {
        FVector Direction=Character->GetVisualRoot()->GetForwardVector();
        Direction.Z=0;
        ArrivalStartOffset=-Direction.GetSafeNormal()*ArrivalWalkDistance;
        if(auto* Parent=Character->GetVisualRoot()->GetAttachParent())
            ArrivalStartOffset=Parent->GetComponentTransform().InverseTransformVector(ArrivalStartOffset);
        // Match the authored walking animation, even when it outlasts the freeze.
        ActiveMovementDuration=EntranceRemaining>0 ? EntranceRemaining : FMath::Max(.01f,ArrivalWalkDuration);
    }
    else
    {
        ArrivalStartOffset=FVector(0,0,ArrivalDropHeight);
        ActiveMovementDuration=FMath::Max(.01f,ArrivalDropDuration);
        if(bSwapFreezeActive) ActiveMovementDuration=FMath::Min(ActiveMovementDuration,FreezeRemaining);
        else if(EntranceRemaining>0) ActiveMovementDuration=FMath::Min(ActiveMovementDuration,EntranceRemaining);
    }
    ArrivalMovementElapsed=0;
    bArrivalMovementActive=true;
    Character->GetVisualRoot()->SetRelativeLocation(ArrivalBaseLocation+ArrivalStartOffset);
}

void USwapPresentationComponent::UpdateArrivalMovement(float Delta)
{
    if(!bArrivalMovementActive) return;
    ArrivalMovementElapsed+=FMath::Max(0.f,Delta);
    const float Alpha=FMath::Clamp(ArrivalMovementElapsed/ActiveMovementDuration,0.f,1.f);
    if(Alpha>=1.f) { ResetArrivalMovement(); return; }
    if(auto* Character=Cast<ACharacterBase>(GetOwner()))
        if(Character->GetVisualRoot())
            Character->GetVisualRoot()->SetRelativeLocation(ArrivalBaseLocation+ArrivalStartOffset*(1.f-(bArrivalWalking ? Alpha : Alpha*Alpha)));
}

void USwapPresentationComponent::ResetArrivalMovement()
{
    if(!bArrivalMovementActive) return;
    if(auto* Character=Cast<ACharacterBase>(GetOwner()))
        if(Character->GetVisualRoot()) Character->GetVisualRoot()->SetRelativeLocation(ArrivalBaseLocation);
    bArrivalMovementActive=false;
    ArrivalMovementElapsed=0;
}
