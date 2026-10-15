// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerCameraRig.generated.h"

class ACharacterBase;
class UCameraComponent;
class USpringArmComponent;
class UCameraOcclusionComponent;

UCLASS()
class HEAVENSDIVIDE_API APlayerCameraRig : public AActor
{
	GENERATED_BODY()

public:
	APlayerCameraRig();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void StartSwapFocus();
	void StopSwapFocus(bool bCancelArrivalKick = true);
	void StartArrivalKick(float Strength, float Duration);
	void StartJointCamera(AActor* Requester, FVector Focus, FVector Direction, float Windup, float Duration);
	void EndJointCamera(AActor* Requester, bool bImmediate = false);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Joint Combo") bool bEnableJointCamera = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Joint Combo", meta=(ClampMin="0", ClampMax="20")) float JointOrbitDegrees = 9.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Joint Combo", meta=(ClampMin="0", ClampMax="0.3")) float JointZoomAmount = .16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Swap Focus") bool bEnableSwapFocus = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Swap Focus", meta=(ClampMin="0", ClampMax="0.5")) float SwapZoomAmount = .18f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Swap Focus", meta=(ClampMin="0.01", Units="s")) float SwapZoomInDuration = .12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Swap Focus", meta=(ClampMin="0.01", Units="s")) float SwapZoomOutDuration = .65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Swap Focus", meta=(ClampMin="0", ClampMax="1")) float SwapVignetteStrength = .12f;

	void SetFollowTarget(ACharacterBase* NewFollowTarget);
	ACharacterBase* GetFollowTarget() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
	TObjectPtr<UCameraOcclusionComponent> OcclusionFade;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<ACharacterBase> FollowTarget;
private:
    friend class FJointComboTest;
    friend class AJointComboActor;
	bool UpdateJointCamera(float Delta);
	void RestoreJointCamera();
	TWeakObjectPtr<AActor> JointCameraOwner;
	bool bJointCameraActive = false;
	float JointAge = 0, JointWindup = .65f, JointDuration = 1.8f, JointReturnAge = 0;
	float JointSavedArm = 900, JointSavedWidth = 512, JointSavedFOV = 90, JointSavedVignette = .4f;
    float JointReturnArm=900, JointReturnWidth=512, JointReturnVignette=.4f;
	bool bJointSavedLag = false, bJointSavedVignetteOverride = false, bJointReturning = false;
	FVector JointFocus, JointStartLocation, JointReturnLocation;
	FRotator JointSavedRotation;
    FRotator JointReturnRotation;
	friend class FSwapCameraTest;
	void UpdateSwapFocus(float RealDelta);
	float ArrivalKickAge = 0, ArrivalKickDuration = 0, ArrivalKickStrength = 0;
	FVector ArrivalKickBaseLocation = FVector::ZeroVector;
	void UpdateArrivalKick(float RealDelta);
	void StopArrivalKick();
	bool bSwapFocusActive = false;
	float SwapFocusElapsed = 0;
	float OriginalFOV = 90;
	float OriginalOrthoWidth = 512;
	float OriginalVignette = .4f;
	bool bOriginalVignetteOverride = false;
	bool bOriginalCameraLag = false;
};
