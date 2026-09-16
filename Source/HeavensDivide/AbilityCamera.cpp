#include "PlayerCameraRig.h"
#include "NinjaCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

bool APlayerCameraRig::UpdateAbilityCamera(float RealDelta)
{
    if (!IsValid(FollowTarget) || !Camera || !CameraBoom)
    {
        StopAbilityCamera();
        return false;
    }
    const bool bRequested = bEnableThousandCutsCamera && FollowTarget->IsA<ANinjaCharacter>()
        && FollowTarget->bComboAbilityActive;
    if (bRequested && !bAbilityCameraActive)
    {
        StopSwapFocus();
        AbilitySavedArmLength = CameraBoom->TargetArmLength;
        AbilitySavedOrthoWidth = Camera->OrthoWidth;
        bAbilitySavedCameraLag = CameraBoom->bEnableCameraLag;
        // Apply one layer of smoothing in real time, including during ability freeze.
        CameraBoom->bEnableCameraLag = false;
        bAbilityCameraActive = true;
        AbilityCameraAge = AbilityZoomWeight = 0;
    }
    if (!bAbilityCameraActive) return false;
    const float Dt = FMath::Max(0.f, RealDelta);
    const FVector Target = FollowTarget->GetActorLocation();
    if (bRequested)
    {
        if (bAbilityCameraReturning)
            AbilityCameraAge = AbilityZoomWeight * FMath::Max(.01f, AbilityBlendInDuration);
        bAbilityCameraReturning = false;
        AbilityCameraAge += Dt;
        AbilityZoomWeight = FMath::Min(1.f, AbilityCameraAge / FMath::Max(.01f, AbilityBlendInDuration));
        const float Limit = FMath::Max(0.f, AbilityMaxFocusDistance);
        const float DeadZone = FMath::Clamp(AbilityDeadZone, 0.f, Limit);
        FVector Focus = GetActorLocation();
        const FVector Offset = Target - Focus;
        const float Distance = Offset.Size();
        if (Distance > DeadZone)
        {
            const float Step = FMath::Min((Distance - DeadZone) * (1.f - FMath::Exp(-FMath::Max(.1f, AbilityFollowSpeed) * Dt)),
                FMath::Max(1.f, AbilityMaxCameraSpeed) * Dt);
            Focus += Offset.GetSafeNormal() * Step;
        }
        // Long root-motion dashes must not leave the character indefinitely behind the camera.
        Focus = Target + (Focus - Target).GetClampedToMaxSize(Limit);
        SetActorLocation(Focus);
    }
    else
    {
        if (!bAbilityCameraReturning)
        {
            bAbilityCameraReturning = true;
            AbilityReturnAge = 0;
            AbilityReturnLocation = GetActorLocation();
            AbilityReturnZoomWeight = AbilityZoomWeight;
        }
        AbilityReturnAge += Dt;
        const float Alpha = FMath::Clamp(AbilityReturnAge / FMath::Max(.01f, AbilityBlendOutDuration), 0.f, 1.f);
        const float Smooth = Alpha * Alpha * (3.f - 2.f * Alpha);
        SetActorLocation(FMath::Lerp(AbilityReturnLocation, Target, Smooth));
        AbilityZoomWeight = AbilityReturnZoomWeight * (1.f - Alpha);
        if (Alpha >= 1.f) { StopAbilityCamera(); return true; }
    }
    const float Weight = AbilityZoomWeight * AbilityZoomWeight * (3.f - 2.f * AbilityZoomWeight);
    const float Scale = FMath::Lerp(1.f, FMath::Clamp(AbilityZoomOutMultiplier, 1.f, 2.f), Weight);
    CameraBoom->TargetArmLength = AbilitySavedArmLength * Scale;
    Camera->SetOrthoWidth(AbilitySavedOrthoWidth * Scale);
    return true;
}

void APlayerCameraRig::StopAbilityCamera()
{
    if (!bAbilityCameraActive) return;
    if (CameraBoom)
    {
        CameraBoom->TargetArmLength = AbilitySavedArmLength;
        CameraBoom->bEnableCameraLag = bAbilitySavedCameraLag;
    }
    if (Camera) Camera->SetOrthoWidth(AbilitySavedOrthoWidth);
    bAbilityCameraActive = bAbilityCameraReturning = false;
    AbilityZoomWeight = 0;
}
