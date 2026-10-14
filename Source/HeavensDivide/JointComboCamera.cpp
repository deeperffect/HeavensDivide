#include "PlayerCameraRig.h"
#include "CharacterBase.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

namespace { float Smooth(float A) { A=FMath::Clamp(A,0.f,1.f); return A*A*A*(A*(A*6-15)+10); } }
void APlayerCameraRig::StartJointCamera(AActor* Requester,FVector Focus,FVector Direction,float Windup,float Duration)
{
    if (!Requester || !FollowTarget || !Camera || !CameraBoom || !bEnableJointCamera) return;
    RestoreJointCamera(); StopSwapFocus();
    JointCameraOwner=Requester; bJointCameraActive=true; bJointReturning=false;
    JointAge=JointReturnAge=0; JointWindup=FMath::Max(.01f,Windup); JointDuration=FMath::Max(JointWindup+.2f,Duration);
    JointFocus=Focus+Direction*65; JointStartLocation=GetActorLocation();
    JointSavedArm=CameraBoom->TargetArmLength; JointSavedWidth=Camera->OrthoWidth; JointSavedFOV=Camera->FieldOfView;
    JointSavedRotation=CameraBoom->GetRelativeRotation(); bJointSavedLag=CameraBoom->bEnableCameraLag; CameraBoom->bEnableCameraLag=false;
    JointSavedVignette=Camera->PostProcessSettings.VignetteIntensity;
    bJointSavedVignetteOverride=Camera->PostProcessSettings.bOverride_VignetteIntensity;
}
void APlayerCameraRig::EndJointCamera(AActor* Requester,bool bImmediate)
{
    if (!bJointCameraActive || (JointCameraOwner.IsValid() && JointCameraOwner.Get()!=Requester)) return;
    if (!IsValid(FollowTarget)) { RestoreJointCamera(); return; }
    if (bImmediate) { RestoreJointCamera(); return; }
    if (!bJointReturning) {
        bJointReturning=true; JointReturnAge=0;
        // Cache the actual pose. Every channel makes one return, without a
        // release zoom, sinusoidal orbit reversal, or second impact kick.
        JointReturnLocation=GetActorLocation()-FollowTarget->GetActorLocation();
        JointReturnArm=CameraBoom->TargetArmLength; JointReturnWidth=Camera->OrthoWidth;
        JointReturnRotation=CameraBoom->GetRelativeRotation(); JointReturnVignette=Camera->PostProcessSettings.VignetteIntensity;
    }
}
bool APlayerCameraRig::UpdateJointCamera(float Delta)
{
    if (!bJointCameraActive) return false;
    if (!IsValid(FollowTarget) || !Camera || !CameraBoom) { RestoreJointCamera(); return false; }
    if (!JointCameraOwner.IsValid() || !bEnableJointCamera) EndJointCamera(nullptr);
    if (bJointReturning) {
        JointReturnAge+=FMath::Max(0.f,Delta); const float S=Smooth(JointReturnAge/.48f);
        SetActorLocation(FollowTarget->GetActorLocation()+JointReturnLocation*(1-S));
        CameraBoom->TargetArmLength=FMath::Lerp(JointReturnArm,JointSavedArm,S);
        Camera->SetOrthoWidth(FMath::Lerp(JointReturnWidth,JointSavedWidth,S));
        CameraBoom->SetRelativeRotation(FMath::Lerp(JointReturnRotation,JointSavedRotation,S));
        Camera->PostProcessSettings.VignetteIntensity=FMath::Lerp(JointReturnVignette,JointSavedVignette,S);
        if (JointReturnAge>=.48f) RestoreJointCamera();
    } else {
        JointAge+=FMath::Max(0.f,Delta);
        const float Charge=Smooth(JointAge/JointWindup);
        // A restrained push and orbit settle before the helper arrives, then hold
        // until control returns. Perspective FOV stays fixed throughout.
        SetActorLocation(FMath::Lerp(JointStartLocation,JointFocus,Charge));
        const float Scale=1-FMath::Clamp(JointZoomAmount,0.f,.3f)*.55f*Charge;
        CameraBoom->TargetArmLength=JointSavedArm*Scale; Camera->SetOrthoWidth(JointSavedWidth*Scale);
        CameraBoom->SetRelativeRotation(JointSavedRotation+FRotator(-1.5f*Charge,FMath::Clamp(JointOrbitDegrees,0.f,20.f)*.35f*Charge,0));
        Camera->PostProcessSettings.bOverride_VignetteIntensity=true;
        Camera->PostProcessSettings.VignetteIntensity=FMath::Clamp(JointSavedVignette+.08f*Charge,0.f,1.f);
        if (JointAge>=JointDuration) EndJointCamera(JointCameraOwner.Get());
    }
    return true;
}
void APlayerCameraRig::RestoreJointCamera()
{
    if (!bJointCameraActive) return;
    if (CameraBoom) { CameraBoom->TargetArmLength=JointSavedArm; CameraBoom->SetRelativeRotation(JointSavedRotation); CameraBoom->bEnableCameraLag=bJointSavedLag; }
    if (Camera) { Camera->SetOrthoWidth(JointSavedWidth); Camera->SetFieldOfView(JointSavedFOV); Camera->PostProcessSettings.VignetteIntensity=JointSavedVignette; Camera->PostProcessSettings.bOverride_VignetteIntensity=bJointSavedVignetteOverride; }
    bJointCameraActive=bJointReturning=false; JointCameraOwner.Reset();
    if (IsValid(FollowTarget)) SetActorLocation(FollowTarget->GetActorLocation());
}
