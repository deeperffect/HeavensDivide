// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyLightweightMovementComponent.h"

UEnemyLightweightMovementComponent::UEnemyLightweightMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UEnemyLightweightMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AActor* Owner = GetOwner())
	{
		SpawnZ = Owner->GetActorLocation().Z;
	}
}

void UEnemyLightweightMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!bMovementEnabled || !Owner || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		CurrentVelocity = FVector::ZeroVector;
		bHasRequestedMove = false;
		return;
	}

	if (!bHasRequestedMove || RequestedMoveDirection.IsNearlyZero())
	{
		CurrentVelocity = FVector::ZeroVector;
		bHasRequestedMove = false;
		return;
	}

	FVector MoveDirection = RequestedMoveDirection;
	MoveDirection.Z = 0.0f;
	if (!MoveDirection.Normalize())
	{
		CurrentVelocity = FVector::ZeroVector;
		bHasRequestedMove = false;
		return;
	}

	const FVector StartLocation = Owner->GetActorLocation();
	const FVector DesiredDelta = MoveDirection * MoveSpeed * DeltaTime;
	FVector DesiredLocation = StartLocation + DesiredDelta;
	DesiredLocation.Z = SpawnZ;

	FHitResult HitResult;
	Owner->SetActorLocation(DesiredLocation, true, &HitResult);

	if (HitResult.bBlockingHit && !DesiredDelta.IsNearlyZero())
	{
		const FVector AfterFirstMove = Owner->GetActorLocation();
		FVector SlideDelta = FVector::VectorPlaneProject(DesiredDelta, HitResult.Normal);
		SlideDelta.Z = 0.0f;

		if (!SlideDelta.IsNearlyZero())
		{
			FVector SlideLocation = AfterFirstMove + SlideDelta;
			SlideLocation.Z = SpawnZ;

			FHitResult SlideHit;
			Owner->SetActorLocation(SlideLocation, true, &SlideHit);
		}
	}

	const FVector ActualDelta = Owner->GetActorLocation() - StartLocation;
	CurrentVelocity = ActualDelta / DeltaTime;
	CurrentVelocity.Z = 0.0f;
	bHasRequestedMove = false;
}

void UEnemyLightweightMovementComponent::SetMovementEnabled(bool bInMovementEnabled)
{
	bMovementEnabled = bInMovementEnabled;
	SetComponentTickEnabled(bMovementEnabled);

	if (!bMovementEnabled)
	{
		StopMovement();
	}
}

bool UEnemyLightweightMovementComponent::IsMovementEnabled() const
{
	return bMovementEnabled;
}

void UEnemyLightweightMovementComponent::SetMoveSpeed(float NewMoveSpeed)
{
	MoveSpeed = FMath::Max(0.0f, NewMoveSpeed);
}

void UEnemyLightweightMovementComponent::RequestMove(const FVector& WorldDirection)
{
	if (!bMovementEnabled)
	{
		return;
	}

	RequestedMoveDirection = WorldDirection;
	RequestedMoveDirection.Z = 0.0f;
	bHasRequestedMove = !RequestedMoveDirection.IsNearlyZero();
}

void UEnemyLightweightMovementComponent::StopMovement()
{
	RequestedMoveDirection = FVector::ZeroVector;
	CurrentVelocity = FVector::ZeroVector;
	bHasRequestedMove = false;
}

FVector UEnemyLightweightMovementComponent::GetCurrentVelocity() const
{
	return CurrentVelocity;
}
