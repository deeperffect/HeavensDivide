// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerCameraRig.generated.h"

class ACharacterBase;
class UCameraComponent;
class USpringArmComponent;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Swap Focus") bool bEnableSwapFocus = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Swap Focus", meta=(ClampMin="0", ClampMax="0.5")) float SwapZoomAmount = .18f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Swap Focus", meta=(ClampMin="0.01", Units="s")) float SwapZoomInDuration = .12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Swap Focus", meta=(ClampMin="0.01", Units="s")) float SwapZoomOutDuration = .65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Swap Focus", meta=(ClampMin="0", ClampMax="1")) float SwapVignetteStrength = .12f;

	void SetFollowTarget(ACharacterBase* NewFollowTarget);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Thousand Cuts") bool bEnableThousandCutsCamera = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Thousand Cuts", meta=(ClampMin="0", Units="cm")) float AbilityDeadZone = 160.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Thousand Cuts", meta=(ClampMin="0", Units="cm", ToolTip="Maximum distance between camera focus and character; overrides smoothing to keep long dashes framed.")) float AbilityMaxFocusDistance = 420.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Thousand Cuts", meta=(ClampMin="0.1")) float AbilityFollowSpeed = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Thousand Cuts", meta=(ClampMin="1", Units="cm/s")) float AbilityMaxCameraSpeed = 800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Thousand Cuts", meta=(ClampMin="1", ClampMax="2")) float AbilityZoomOutMultiplier = 1.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Thousand Cuts", meta=(ClampMin="0.01", Units="s")) float AbilityBlendInDuration = .25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Thousand Cuts", meta=(ClampMin="0.01", Units="s")) float AbilityBlendOutDuration = .65f;
	ACharacterBase* GetFollowTarget() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<ACharacterBase> FollowTarget;
private:
	friend class FAbilityCameraTest;
	bool UpdateAbilityCamera(float RealDelta);
	void StopAbilityCamera();
	bool bAbilityCameraActive = false, bAbilityCameraReturning = false;
	bool bAbilitySavedCameraLag = false;
	float AbilityCameraAge = 0, AbilityReturnAge = 0, AbilityZoomWeight = 0, AbilityReturnZoomWeight = 0;
	float AbilitySavedArmLength = 900, AbilitySavedOrthoWidth = 512;
	FVector AbilityReturnLocation = FVector::ZeroVector;
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
