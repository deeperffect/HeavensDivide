// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyLightweightMovementComponent.h"

#include "CharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "EnemyBase.h"
#include "GameFramework/Character.h"

static bool FindBlockingWorldGeometryHit(const TArray<FHitResult>& HitResults, FHitResult& OutHit)
{
	const FHitResult* BestStartPenetratingHit = nullptr;

	for (const FHitResult& HitResult : HitResults)
	{
		AActor* HitActor = HitResult.GetActor();
		if (!HitResult.bBlockingHit || !HitActor)
		{
			continue;
		}

		if (Cast<AEnemyBase>(HitActor) || Cast<ACharacterBase>(HitActor))
		{
			continue;
		}

		if (HitResult.bStartPenetrating)
		{
			if (!BestStartPenetratingHit)
			{
				BestStartPenetratingHit = &HitResult;
			}
			continue;
		}

		OutHit = HitResult;
		return true;
	}

	if (BestStartPenetratingHit)
	{
		OutHit = *BestStartPenetratingHit;
		return true;
	}

	return false;
}

static FVector GetSafeMovementHitLocation(const FVector& FallbackLocation, const FHitResult& HitResult, float PullbackDistance)
{
	FVector SafeLocation = HitResult.Location.IsNearlyZero() ? FallbackLocation : HitResult.Location;
	if (!HitResult.Normal.IsNearlyZero())
	{
		SafeLocation += HitResult.Normal.GetSafeNormal2D() * PullbackDistance;
	}

	return SafeLocation;
}

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
		bLastMoveBlockedByWorldGeometry = false;
		bHasRequestedMove = false;
		return;
	}

	if (!bHasRequestedMove || RequestedMoveDirection.IsNearlyZero())
	{
		CurrentVelocity = FVector::ZeroVector;
		bLastMoveBlockedByWorldGeometry = false;
		bHasRequestedMove = false;
		return;
	}

	FVector MoveDirection = RequestedMoveDirection;
	MoveDirection.Z = 0.0f;
	if (!MoveDirection.Normalize())
	{
		CurrentVelocity = FVector::ZeroVector;
		bLastMoveBlockedByWorldGeometry = false;
		bHasRequestedMove = false;
		return;
	}

	const FVector StartLocation = Owner->GetActorLocation();
	const FVector DesiredDelta = MoveDirection * MoveSpeed * DeltaTime;
	FVector DesiredLocation = StartLocation + DesiredDelta;
	DesiredLocation.Z = SpawnZ;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyLightweightMovement), false, Owner);
	float CapsuleRadius = 34.0f;
	float CapsuleHalfHeight = 88.0f;
	if (const ACharacter* OwnerCharacter = Cast<ACharacter>(Owner))
	{
		if (const UCapsuleComponent* CapsuleComponent = OwnerCharacter->GetCapsuleComponent())
		{
			CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
			CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
		}
	}

	TArray<FHitResult> MoveHits;
	const bool bHasMoveHits = Owner->GetWorld()
		&& Owner->GetWorld()->SweepMultiByChannel(
			MoveHits,
			StartLocation,
			DesiredLocation,
			FQuat::Identity,
			ECC_Pawn,
			FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
			QueryParams);
	const bool bHitBlockingGeometry = bHasMoveHits && FindBlockingWorldGeometryHit(MoveHits, HitResult);
	bLastMoveBlockedByWorldGeometry = bHitBlockingGeometry;

	if (bHitBlockingGeometry && HitResult.bBlockingHit)
	{
		DesiredLocation = GetSafeMovementHitLocation(StartLocation, HitResult, 2.0f);
		DesiredLocation.Z = SpawnZ;
	}

	Owner->SetActorLocation(DesiredLocation, false);

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
			TArray<FHitResult> SlideHits;
			const bool bHasSlideHits = Owner->GetWorld()
				&& Owner->GetWorld()->SweepMultiByChannel(
					SlideHits,
					AfterFirstMove,
					SlideLocation,
					FQuat::Identity,
					ECC_Pawn,
					FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
					QueryParams);
			const bool bSlideHitBlockingGeometry = bHasSlideHits && FindBlockingWorldGeometryHit(SlideHits, SlideHit);

			if (bSlideHitBlockingGeometry && SlideHit.bBlockingHit)
			{
				SlideLocation = GetSafeMovementHitLocation(AfterFirstMove, SlideHit, 2.0f);
				SlideLocation.Z = SpawnZ;
			}

			Owner->SetActorLocation(SlideLocation, false);
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
	bLastMoveBlockedByWorldGeometry = false;
}

FVector UEnemyLightweightMovementComponent::GetCurrentVelocity() const
{
	return CurrentVelocity;
}

bool UEnemyLightweightMovementComponent::WasLastMoveBlockedByWorldGeometry() const
{
	return bLastMoveBlockedByWorldGeometry;
}
