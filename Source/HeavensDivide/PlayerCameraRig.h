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

	void SetFollowTarget(ACharacterBase* NewFollowTarget);
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
};
