// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyLightweightMovementComponent.generated.h"

UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API UEnemyLightweightMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyLightweightMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetMovementEnabled(bool bInMovementEnabled);
	bool IsMovementEnabled() const;

	void SetMoveSpeed(float NewMoveSpeed);
	void RequestMove(const FVector& WorldDirection);
	void StopMovement();
	void RefreshSpawnZ();

	FVector GetCurrentVelocity() const;
	bool WasLastMoveBlockedByWorldGeometry() const;

	// Uses the same world-blocker query as normal enemy movement, but stops at
	// the first lateral blocker instead of sliding. Intended for committed moves.
	bool MoveOwnerToNoSlide(const FVector& DesiredLocation, FHitResult& OutBlockingHit);

private:
	UPROPERTY(Transient)
	FVector RequestedMoveDirection = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector CurrentVelocity = FVector::ZeroVector;

	UPROPERTY(Transient)
	float SpawnZ = 0.0f;

	UPROPERTY(Transient)
	float MoveSpeed = 300.0f;

	UPROPERTY(Transient)
	bool bMovementEnabled = false;

	UPROPERTY(Transient)
	bool bHasRequestedMove = false;

	UPROPERTY(Transient)
	bool bLastMoveBlockedByWorldGeometry = false;
};
