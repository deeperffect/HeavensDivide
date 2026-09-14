#include "EnemyBase.h"

#include "CharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "EnemyLightweightMovementComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Stats/Stats.h"
#include "TimerManager.h"

static TAutoConsoleVariable<int32> CVarDebugEnemySeparation(
	TEXT("hd.DebugEnemySeparation"),
	0,
	TEXT("When set to 1, draws lightweight enemy separation debug for a small sampled subset of enemies."));

static TAutoConsoleVariable<int32> CVarDebugEnemyPathFallback(
	TEXT("hd.DebugEnemyPathFallback"),
	0,
	TEXT("When set to 1, draws enemy lightweight obstacle path fallback debug."));

static TAutoConsoleVariable<int32> CVarDebugEnemyGroundSnap(
	TEXT("hd.DebugEnemyGroundSnap"),
	0,
	TEXT("When set to 1, logs suspicious one-time enemy ground snap corrections."));

static constexpr float EnemyWalkableGroundNormalZ = 0.7f;
static constexpr ECollisionChannel EnemyGroundSnapTraceChannel = ECC_GameTraceChannel2;

static bool IsBlockingWorldGeometryHit(const FHitResult& HitResult)
{
	AActor* HitActor = HitResult.GetActor();
	return HitResult.bBlockingHit
		&& HitActor
		&& !Cast<AEnemyBase>(HitActor)
		&& !Cast<ACharacterBase>(HitActor);
}

static bool IsValidEnemyGroundHit(const FHitResult& HitResult)
{
	AActor* HitActor = HitResult.GetActor();
	return HitResult.bBlockingHit
		&& HitResult.GetComponent()
		&& (!HitActor || (!Cast<AEnemyBase>(HitActor) && !Cast<ACharacterBase>(HitActor)))
		&& HitResult.ImpactNormal.Z >= EnemyWalkableGroundNormalZ;
}

static bool IsEnemyPathFallbackRequestAllowed(UWorld* World)
{
	if (!World)
	{
		return false;
	}

	constexpr uint64 MaxRequestsPerFrame = 10;
	static uint64 LastFrameNumber = 0;
	static uint64 RequestsThisFrame = 0;

	if (LastFrameNumber != GFrameCounter)
	{
		LastFrameNumber = GFrameCounter;
		RequestsThisFrame = 0;
	}

	if (RequestsThisFrame >= MaxRequestsPerFrame)
	{
		return false;
	}

	++RequestsThisFrame;
	return true;
}

void AEnemyBase::StartBehaviorUpdates()
{
	if (!GetWorld() || BehaviorUpdateInterval <= 0.0f || bIsDead)
	{
		return;
	}

	const float InitialDelay = FMath::FRandRange(0.0f, BehaviorUpdateInterval);
	GetWorld()->GetTimerManager().SetTimer(
		BehaviorUpdateTimerHandle,
		this,
		&AEnemyBase::HandleBehaviorUpdateTimer,
		BehaviorUpdateInterval,
		true,
		InitialDelay);
}

void AEnemyBase::StopBehaviorUpdates()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BehaviorUpdateTimerHandle);
	}
}

void AEnemyBase::HandleBehaviorUpdateTimer()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_EnemyBase_BehaviorUpdate);
	UpdateAnimationProfilingState();
	UpdateAnimationBudgetSignificance();

	UpdateEnemyBehavior(BehaviorUpdateInterval);
}

void AEnemyBase::StartSeparationUpdates()
{
	if (!GetWorld() || bIsDead || !bUseEnemySeparation || SeparationRadius <= 0.0f || SeparationUpdateInterval <= 0.0f)
	{
		CachedEnemySeparationVector = FVector::ZeroVector;
		return;
	}

	const float InitialDelay = FMath::FRandRange(0.0f, SeparationUpdateInterval);
	GetWorld()->GetTimerManager().SetTimer(
		SeparationUpdateTimerHandle,
		this,
		&AEnemyBase::HandleSeparationUpdateTimer,
		SeparationUpdateInterval,
		true,
		InitialDelay);
}

void AEnemyBase::StopSeparationUpdates()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SeparationUpdateTimerHandle);
	}

	CachedEnemySeparationVector = FVector::ZeroVector;
}

void AEnemyBase::HandleSeparationUpdateTimer()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_EnemyBase_SeparationUpdate);

	if (bIsDead || !bUseEnemySeparation)
	{
		CachedEnemySeparationVector = FVector::ZeroVector;
		return;
	}

	UpdateCachedEnemySeparation();
}

void AEnemyBase::ApplyDesiredMovementInput(float DeltaSeconds)
{
	if (!bIsDead && bHasDesiredMovementDirection)
	{
		SmoothFaceTarget(DeltaSeconds);
		RequestEnemyMovement(DesiredMovementDirection);
	}
}

void AEnemyBase::StopDesiredMovement()
{
	DesiredMovementDirection = FVector::ZeroVector;
	DesiredDirectMovementDirection = FVector::ZeroVector;
	bHasDesiredMovementDirection = false;
}

void AEnemyBase::InitializeEnemyMovementMode()
{
	if (LightweightMovementComponent)
	{
		LightweightMovementComponent->SetMoveSpeed(MoveSpeed);
		LightweightMovementComponent->SetMovementEnabled(true);
	}

	DisableNativeCharacterMovement();

	if (UCapsuleComponent* EnemyCapsule = GetCapsuleComponent())
	{
		ConfigureEnemyCapsuleCollisionDefaults();
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetGenerateOverlapEvents(false);
	}
}

void AEnemyBase::ConfigureEnemyCapsuleCollisionDefaults()
{
	if (UCapsuleComponent* EnemyCapsule = GetCapsuleComponent())
	{
		EnemyCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		EnemyCapsule->SetGenerateOverlapEvents(true);
		EnemyCapsule->SetCollisionObjectType(ECC_GameTraceChannel1);
		EnemyCapsule->SetCollisionResponseToAllChannels(ECR_Block);
		EnemyCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		EnemyCapsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	}
}

void AEnemyBase::SnapToGroundBeforeLightweightMovement()
{
	UWorld* World = GetWorld();
	UCapsuleComponent* EnemyCapsule = GetCapsuleComponent();
	if (!World || !EnemyCapsule)
	{
		if (LightweightMovementComponent)
		{
			LightweightMovementComponent->RefreshSpawnZ();
		}
		return;
	}

	const FVector OriginalLocation = GetActorLocation();
	const float CapsuleHalfHeight = EnemyCapsule->GetScaledCapsuleHalfHeight();
	const FVector TraceStart = OriginalLocation + FVector(0.0f, 0.0f, 150.0f);
	const FVector TraceEnd = OriginalLocation - FVector(0.0f, 0.0f, FMath::Max(500.0f, CapsuleHalfHeight + 1000.0f));

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyBeginPlayGroundSnap), false, this);
	QueryParams.AddIgnoredActor(this);

	TArray<FHitResult> HitResults;
	World->LineTraceMultiByChannel(HitResults, TraceStart, TraceEnd, EnemyGroundSnapTraceChannel, QueryParams);

	for (const FHitResult& HitResult : HitResults)
	{
		if (!IsValidEnemyGroundHit(HitResult))
		{
			continue;
		}

		const FVector CorrectedLocation(OriginalLocation.X, OriginalLocation.Y, HitResult.Location.Z + CapsuleHalfHeight);
		const float DeltaZ = CorrectedLocation.Z - OriginalLocation.Z;
		if (!FMath::IsNearlyZero(DeltaZ, 1.0f))
		{
			SetActorLocation(CorrectedLocation, false, nullptr, ETeleportType::TeleportPhysics);
			if (FMath::Abs(DeltaZ) > 25.0f && CVarDebugEnemyGroundSnap.GetValueOnGameThread() != 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("Enemy ground snap adjusted %s OriginalZ=%.2f CorrectedZ=%.2f DeltaZ=%.2f HitActor=%s HitComponent=%s ImpactNormal=%s"),
					*GetNameSafe(this),
					OriginalLocation.Z,
					CorrectedLocation.Z,
					DeltaZ,
					*GetNameSafe(HitResult.GetActor()),
					*GetNameSafe(HitResult.GetComponent()),
					*HitResult.ImpactNormal.ToString());
			}
		}
		break;
	}

	if (LightweightMovementComponent)
	{
		LightweightMovementComponent->RefreshSpawnZ();
	}
}

void AEnemyBase::InitializeCrowdSpread()
{
	const uint32 Seed = GetTypeHash(GetFName()) ^ GetUniqueID();
	const FRandomStream RandomStream(Seed);
	const float BiasSign = RandomStream.FRand() < 0.5f ? -1.0f : 1.0f;
	CrowdSpreadBias = BiasSign * RandomStream.FRandRange(0.35f, 1.0f);
}

void AEnemyBase::UpdateObstaclePathFallback()
{
	UWorld* World = GetWorld();
	if (!World || bIsDead || !CurrentTarget || !LightweightMovementComponent)
	{
		ClearObstaclePath();
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	const bool bBlockedByWorldGeometry = LightweightMovementComponent->WasLastMoveBlockedByWorldGeometry();
	const float DistanceToTarget = FVector::Dist2D(GetActorLocation(), CurrentTarget->GetActorLocation());

	if (!bBlockedByWorldGeometry)
	{
		BlockedByWorldGeometryStartTime = -1.0f;
		return;
	}

	if (DistanceToTarget < FMath::Max(StopDistance, MinPathFallbackTargetDistance))
	{
		ClearObstaclePath();
		return;
	}

	if (BlockedByWorldGeometryStartTime < 0.0f)
	{
		BlockedByWorldGeometryStartTime = CurrentTime;
	}

	const bool bHasPath = ObstaclePathPoints.Num() > 1 && CurrentObstaclePathIndex != INDEX_NONE;
	const bool bTargetMovedEnoughForRepath = bHasPath
		&& PathTargetRepathDistance > 0.0f
		&& FVector::DistSquared2D(LastPathTargetLocation, CurrentTarget->GetActorLocation()) >= FMath::Square(PathTargetRepathDistance);
	const bool bBlockedLongEnough = CurrentTime - BlockedByWorldGeometryStartTime >= PathFallbackBlockedTime + PathFallbackRequestJitter;

	if (bBlockedLongEnough && (!bHasPath || bTargetMovedEnoughForRepath) && CurrentTime >= NextPathFallbackRequestTime)
	{
		if (!TryBuildObstaclePath() && CVarDebugEnemyPathFallback.GetValueOnGameThread() != 0)
		{
			const FVector DebugStart = GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
			DrawDebugSphere(World, DebugStart, 35.0f, 12, FColor::Yellow, false, BehaviorUpdateInterval, 0, 2.0f);
		}
	}
	else if (!bHasPath && CVarDebugEnemyPathFallback.GetValueOnGameThread() != 0)
	{
		const FVector DebugStart = GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
		DrawDebugLine(World, DebugStart, DebugStart + FVector::UpVector * 80.0f, FColor::Orange, false, BehaviorUpdateInterval, 0, 2.0f);
	}
}

bool AEnemyBase::TryBuildObstaclePath()
{
	UWorld* World = GetWorld();
	if (!World || !CurrentTarget)
	{
		ClearObstaclePath();
		return false;
	}

	NextPathFallbackRequestTime = World->GetTimeSeconds() + FMath::Max(0.1f, PathFallbackRepathInterval) + PathFallbackRequestJitter;

	if (!IsEnemyPathFallbackRequestAllowed(World))
	{
		if (CVarDebugEnemyPathFallback.GetValueOnGameThread() != 0)
		{
			DrawDebugSphere(World, GetActorLocation() + FVector(0.0f, 0.0f, 95.0f), 45.0f, 12, FColor::Orange, false, BehaviorUpdateInterval, 0, 2.0f);
		}
		return false;
	}

	const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavigationSystem)
	{
		ClearObstaclePath();
		return false;
	}

	FVector ProjectedStart;
	FVector ProjectedEnd;
	if (!ProjectPathFallbackLocation(GetActorLocation(), ProjectedStart) || !ProjectPathFallbackLocation(CurrentTarget->GetActorLocation(), ProjectedEnd))
	{
		ClearObstaclePath();
		return false;
	}

	UNavigationPath* NavigationPath = UNavigationSystemV1::FindPathToLocationSynchronously(this, ProjectedStart, ProjectedEnd, this);
	if (!NavigationPath || !NavigationPath->IsValid() || NavigationPath->PathPoints.Num() < 2)
	{
		ClearObstaclePath();
		if (CVarDebugEnemyPathFallback.GetValueOnGameThread() != 0)
		{
			DrawDebugSphere(World, ProjectedStart, 40.0f, 12, FColor::Red, false, PathFallbackRepathInterval, 0, 2.0f);
			DrawDebugSphere(World, ProjectedEnd, 40.0f, 12, FColor::Red, false, PathFallbackRepathInterval, 0, 2.0f);
		}
		return false;
	}

	ObstaclePathPoints = NavigationPath->PathPoints;
	CurrentObstaclePathIndex = ObstaclePathPoints.Num() > 1 ? 1 : INDEX_NONE;
	LastPathTargetLocation = CurrentTarget->GetActorLocation();
	NextDirectPathCheckTime = 0.0f;

	if (CVarDebugEnemyPathFallback.GetValueOnGameThread() != 0)
	{
		for (int32 Index = 1; Index < ObstaclePathPoints.Num(); ++Index)
		{
			DrawDebugLine(World, ObstaclePathPoints[Index - 1], ObstaclePathPoints[Index], FColor::Purple, false, PathFallbackRepathInterval, 0, 3.0f);
		}
		DrawDebugSphere(World, ObstaclePathPoints[CurrentObstaclePathIndex], 45.0f, 12, FColor::Cyan, false, PathFallbackRepathInterval, 0, 2.0f);
	}

	return true;
}

bool AEnemyBase::TryGetPathFallbackSteeringDirection(FVector& OutSteeringDirection)
{
	UWorld* World = GetWorld();
	if (!World || !CurrentTarget || ObstaclePathPoints.Num() < 2 || CurrentObstaclePathIndex == INDEX_NONE)
	{
		return false;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime >= NextDirectPathCheckTime)
	{
		NextDirectPathCheckTime = CurrentTime + FMath::Max(0.05f, DirectPathCheckInterval);
		if (IsDirectPathToTargetClear())
		{
			ClearObstaclePath();
			return false;
		}
	}

	const FVector CurrentLocation = GetActorLocation();
	while (ObstaclePathPoints.IsValidIndex(CurrentObstaclePathIndex)
		&& FVector::DistSquared2D(CurrentLocation, ObstaclePathPoints[CurrentObstaclePathIndex]) <= FMath::Square(PathWaypointAcceptanceRadius))
	{
		++CurrentObstaclePathIndex;
	}

	if (!ObstaclePathPoints.IsValidIndex(CurrentObstaclePathIndex))
	{
		ClearObstaclePath();
		return false;
	}

	int32 SteeringIndex = CurrentObstaclePathIndex;
	const int32 FurthestLookAheadIndex = FMath::Min(CurrentObstaclePathIndex + 3, ObstaclePathPoints.Num() - 1);
	for (int32 CandidateIndex = FurthestLookAheadIndex; CandidateIndex > CurrentObstaclePathIndex; --CandidateIndex)
	{
		if (IsPathFallbackRouteClearToLocation(ObstaclePathPoints[CandidateIndex]))
		{
			SteeringIndex = CandidateIndex;
			break;
		}
	}

	FVector ToWaypoint = ObstaclePathPoints[SteeringIndex] - CurrentLocation;
	ToWaypoint.Z = 0.0f;
	if (!ToWaypoint.Normalize())
	{
		return false;
	}

	OutSteeringDirection = ToWaypoint;

	if (CVarDebugEnemyPathFallback.GetValueOnGameThread() != 0)
	{
		const FVector DebugStart = CurrentLocation + FVector(0.0f, 0.0f, 45.0f);
		DrawDebugLine(World, DebugStart, DebugStart + OutSteeringDirection * 220.0f, FColor::Cyan, false, 0.15f, 0, 3.0f);
		DrawDebugSphere(World, ObstaclePathPoints[SteeringIndex], 35.0f, 12, FColor::Cyan, false, 0.15f, 0, 2.0f);
	}

	return true;
}

bool AEnemyBase::IsDirectPathToTargetClear() const
{
	return CurrentTarget && IsPathFallbackRouteClearToLocation(CurrentTarget->GetActorLocation());
}

bool AEnemyBase::IsPathFallbackRouteClearToLocation(const FVector& Location) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	float CapsuleRadius = 34.0f;
	float CapsuleHalfHeight = 88.0f;
	if (const UCapsuleComponent* EnemyCapsule = GetCapsuleComponent())
	{
		CapsuleRadius = EnemyCapsule->GetScaledCapsuleRadius();
		CapsuleHalfHeight = EnemyCapsule->GetScaledCapsuleHalfHeight();
	}

	TArray<FHitResult> Hits;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyPathFallbackDirectCheck), false, this);
	QueryParams.AddIgnoredActor(this);
	if (CurrentTarget)
	{
		QueryParams.AddIgnoredActor(CurrentTarget);
	}

	const FVector StartLocation = GetActorLocation();
	FVector EndLocation = Location;
	EndLocation.Z = StartLocation.Z;
	const bool bHasHits = World->SweepMultiByChannel(
		Hits,
		StartLocation,
		EndLocation,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
		QueryParams);

	if (!bHasHits)
	{
		return true;
	}

	for (const FHitResult& Hit : Hits)
	{
		if (IsBlockingWorldGeometryHit(Hit))
		{
			return false;
		}
	}

	return true;
}

bool AEnemyBase::ProjectPathFallbackLocation(const FVector& Location, FVector& OutProjectedLocation) const
{
	const UWorld* World = GetWorld();
	const UNavigationSystemV1* NavigationSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!NavigationSystem)
	{
		return false;
	}

	FNavLocation NavLocation;
	if (!NavigationSystem->ProjectPointToNavigation(Location, NavLocation, PathFallbackProjectionExtent))
	{
		return false;
	}

	OutProjectedLocation = NavLocation.Location;
	return true;
}

void AEnemyBase::ClearObstaclePath()
{
	ObstaclePathPoints.Reset();
	CurrentObstaclePathIndex = INDEX_NONE;
	LastPathTargetLocation = FVector::ZeroVector;
	NextDirectPathCheckTime = 0.0f;
	BlockedByWorldGeometryStartTime = -1.0f;
}

FVector AEnemyBase::ApplyCrowdSpreadToDirection(const FVector& DirectDirection) const
{
	if (!bUseCrowdSpread || CrowdSpreadStrength <= 0.0f)
	{
		return DirectDirection;
	}

	FVector SafeDirectDirection = DirectDirection;
	SafeDirectDirection.Z = 0.0f;
	if (!SafeDirectDirection.Normalize())
	{
		return FVector::ZeroVector;
	}

	const FVector SideDirection(-SafeDirectDirection.Y, SafeDirectDirection.X, 0.0f);
	FVector SpreadDirection = SafeDirectDirection + SideDirection * CrowdSpreadBias * CrowdSpreadStrength;
	SpreadDirection.Z = 0.0f;
	if (!SpreadDirection.Normalize())
	{
		return SafeDirectDirection;
	}

	return SpreadDirection;
}

FVector AEnemyBase::ApplyEnemySeparationToDirection(const FVector& MovementDirection) const
{
	if (!bUseEnemySeparation || SeparationStrength <= 0.0f || CachedEnemySeparationVector.IsNearlyZero())
	{
		return MovementDirection;
	}

	FVector SafeMovementDirection = MovementDirection;
	SafeMovementDirection.Z = 0.0f;
	if (!SafeMovementDirection.Normalize())
	{
		return MovementDirection;
	}

	FVector SeparationContribution = CachedEnemySeparationVector.GetClampedToMaxSize(MaxSeparationContribution) * SeparationStrength;
	SeparationContribution.Z = 0.0f;

	FVector FinalDirection = SafeMovementDirection + SeparationContribution;
	FinalDirection.Z = 0.0f;
	if (!FinalDirection.Normalize())
	{
		return SafeMovementDirection;
	}

	if (FVector::DotProduct(FinalDirection, SafeMovementDirection) < 0.35f)
	{
		FinalDirection = (SafeMovementDirection * 0.65f + FinalDirection * 0.35f).GetSafeNormal();
	}

	return FinalDirection;
}

void AEnemyBase::UpdateCachedEnemySeparation()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_EnemyBase_UpdateCachedEnemySeparation);
	CachedEnemySeparationVector = FVector::ZeroVector;

	if (!GetWorld() || SeparationRadius <= 0.0f)
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemySeparation), false, this);
	QueryParams.AddIgnoredActor(this);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);

	GetWorld()->OverlapMultiByObjectType(
		OverlapResults,
		GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(SeparationRadius),
		QueryParams);

	const FVector MyLocation = GetActorLocation();
	FVector Separation = FVector::ZeroVector;
	int32 NeighborCount = 0;

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AEnemyBase* OtherEnemy = Cast<AEnemyBase>(OverlapResult.GetActor());
		if (!OtherEnemy || OtherEnemy == this || OtherEnemy->IsDead())
		{
			continue;
		}

		FVector Away = MyLocation - OtherEnemy->GetActorLocation();
		Away.Z = 0.0f;
		const float Distance = Away.Size2D();
		if (Distance > SeparationRadius)
		{
			continue;
		}

		if (Distance <= KINDA_SMALL_NUMBER)
		{
			const uint32 Seed = GetUniqueID() ^ OtherEnemy->GetUniqueID();
			const float Angle = static_cast<float>(Seed % 360);
			Away = FVector(FMath::Cos(FMath::DegreesToRadians(Angle)), FMath::Sin(FMath::DegreesToRadians(Angle)), 0.0f);
		}
		else
		{
			Away /= Distance;
		}

		const float Weight = FMath::Clamp(1.0f - (Distance / SeparationRadius), 0.0f, 1.0f);
		Separation += Away * Weight;
		++NeighborCount;
	}

	if (NeighborCount > 0)
	{
		CachedEnemySeparationVector = Separation.GetClampedToMaxSize(MaxSeparationContribution);
	}

	if (CVarDebugEnemySeparation.GetValueOnGameThread() != 0 && GetWorld() && (GetUniqueID() % 24 == 0))
	{
		const FVector DebugStart = GetActorLocation() + FVector(0.0f, 0.0f, 35.0f);
		DrawDebugSphere(GetWorld(), GetActorLocation(), SeparationRadius, 16, FColor::Blue, false, SeparationUpdateInterval, 0, 1.0f);
		DrawDebugLine(GetWorld(), DebugStart, DebugStart + CachedEnemySeparationVector * 180.0f, FColor::Magenta, false, SeparationUpdateInterval, 0, 2.0f);
	}
}

void AEnemyBase::RequestEnemyMovement(const FVector& WorldDirection)
{
	if (!LightweightMovementComponent)
	{
		ensureMsgf(false, TEXT("Enemy %s is missing required LightweightMovementComponent."), *GetNameSafe(this));
		return;
	}

	LightweightMovementComponent->RequestMove(WorldDirection);
}

void AEnemyBase::StopEnemyMovement()
{
	StopDesiredMovement();

	if (LightweightMovementComponent)
	{
		LightweightMovementComponent->StopMovement();
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
}

FVector AEnemyBase::GetEnemyMovementVelocity() const
{
	if (LightweightMovementComponent)
	{
		return LightweightMovementComponent->GetCurrentVelocity();
	}

	return FVector::ZeroVector;
}

void AEnemyBase::DisableNativeCharacterMovement()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
		MovementComponent->SetComponentTickEnabled(false);
	}
}

void AEnemyBase::MoveTowardCurrentTarget()
{
	if (!CurrentTarget)
	{
		return;
	}

	FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.SizeSquared2D() <= FMath::Square(GetChaseStopDistance()))
	{
		StopEnemyMovement();
		return;
	}

	if (ToTarget.Normalize())
	{
		DesiredDirectMovementDirection = ToTarget;
		UpdateObstaclePathFallback();

		FVector SteeringDirection = ToTarget;
		FVector PathSteeringDirection;
		const bool bUsingPathFallback = TryGetPathFallbackSteeringDirection(PathSteeringDirection);
		if (bUsingPathFallback)
		{
			SteeringDirection = PathSteeringDirection;
		}

		DesiredMovementDirection = ApplyEnemySeparationToDirection(ApplyCrowdSpreadToDirection(SteeringDirection));
		if (bUsingPathFallback)
		{
			DesiredMovementDirection = (PathSteeringDirection * 0.8f + DesiredMovementDirection * 0.2f).GetSafeNormal2D();
		}
		bHasDesiredMovementDirection = true;

		if (bDebugCrowdSpread && GetWorld())
		{
			const FVector DebugStart = GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
			DrawDebugLine(GetWorld(), DebugStart, DebugStart + DesiredDirectMovementDirection * 160.0f, FColor::Green, false, 0.0f, 0, 2.0f);
			DrawDebugLine(GetWorld(), DebugStart, DebugStart + DesiredMovementDirection * 160.0f, FColor::Cyan, false, 0.0f, 0, 2.0f);
		}
	}
}

void AEnemyBase::FaceTarget()
{
	if (!CurrentTarget)
	{
		return;
	}

	FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.Normalize())
	{
		SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
	}
}

void AEnemyBase::SmoothFaceTarget(float DeltaSeconds)
{
	if (!CurrentTarget || DeltaSeconds <= 0.0f || EnemyRotationSpeed <= 0.0f)
	{
		return;
	}

	FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.0f;
	if (!ToTarget.Normalize())
	{
		return;
	}

	const FRotator CurrentRotation = GetActorRotation();
	const FRotator TargetRotation(0.0f, ToTarget.Rotation().Yaw, 0.0f);
	const FRotator NewRotation = FMath::RInterpConstantTo(CurrentRotation, TargetRotation, DeltaSeconds, EnemyRotationSpeed);
	SetActorRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));
}

