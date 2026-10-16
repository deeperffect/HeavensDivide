#include "EnemySpawner.h"
#include "EnemySpawnerDiagnostics.h"

#include "CharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "EnemyBase.h"
#include "EnemySpawnArea.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Stats/Stats.h"
#include "Kismet/GameplayStatics.h"
#include "CombatGround.h"
#include "EngineUtils.h"

TAutoConsoleVariable<int32> CVarDebugEnemySpawnGround(
	TEXT("hd.DebugEnemySpawnGround"),
	0,
	TEXT("When set to 1, logs rejected enemy spawn ground hits and suspicious spawn Z changes."));

static constexpr float EnemySpawnWalkableGroundNormalZ = 0.7f;
static constexpr ECollisionChannel EnemySpawnGroundTraceChannel = ECC_GameTraceChannel2;
static void TraceSpawnGround(UWorld* World, TArray<FHitResult>& Hits, const FVector& Start,
    const FVector& End, FCollisionQueryParams Params);

AEnemyBase* AEnemySpawner::SpawnEnemyAtValidatedLocation(TSubclassOf<AEnemyBase> EnemyClass, const FVector& Location, const FRotator& Rotation) const
{
    if (!GetWorld() || !EnemyClass) return nullptr;
    FActorSpawnParameters Params;
    Params.Owner=const_cast<AEnemySpawner*>(this);
    // DontSpawnIfColliding also checks the unregistered CDO, whose cached
    // geometry ignores authored root scale and rejects smaller enemies on clear
    // floor. Check the constructed actor, then forbid the adjustment it permits.
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
    auto* Enemy=GetWorld()->SpawnActor<AEnemyBase>(EnemyClass,Location,Rotation,Params);
    // BeginPlay seats lightweight enemies on the same floor, removing our 2 cm
    // contact allowance. Permit only that downward settle, never an XY shift
    // or an upward relocation onto scenery.
    const FVector Delta=Enemy ? Enemy->GetActorLocation()-Location : FVector::ZeroVector;
    if (Enemy && (Delta.SizeSquared2D()>.01f || Delta.Z < -2.1f || Delta.Z > .1f))
    {
        Enemy->Destroy();
        return nullptr;
    }
    return Enemy;
}

bool AEnemySpawner::ProjectEncounterGround(const FVector& Desired, FVector& OutGround) const
{
    return ProjectSpawnLocationToNavigation(Desired,OutGround)
        && FVector::DistSquared2D(Desired,OutGround)<=FMath::Square(35.f)
        && IsSpawnLocationInsideArena(OutGround);
}

bool AEnemySpawner::ValidateEncounterLocation(const FVector& Desired, TSubclassOf<AEnemyBase> EnemyClass, FVector& OutLocation) const
{
    FVector Ground;
    float Radius, HalfHeight;
    GetEnemyCapsuleDimensions(EnemyClass, Radius, HalfHeight);
    if (!ProjectSpawnLocationToNavigation(Desired, Ground)) return false;
    // A formation cannot tolerate navigation moving individual members sideways.
    if (FVector::DistSquared2D(Desired, Ground) > FMath::Square(35.f)) return false;
    OutLocation = Ground + FVector(0,0,HalfHeight);
    return IsSpawnLocationInsideArena(OutLocation) && CombatGround::IsInsideArena(GetWorld(),OutLocation,Radius)
        && HasGroundSupport(Ground,Radius) && IsSpawnLocationCollisionFree(OutLocation, EnemyClass);
}

bool AEnemySpawner::IsValidSpawnGround(const FHitResult& HitResult) const
{
	if (!HasAuthoredSpawnGround.IsSet())
	{
		HasAuthoredSpawnGround = false;
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			if (It->ActorHasTag(TEXT("SurvivalMapGround"))) { HasAuthoredSpawnGround = true; break; }
	}
	AActor* HitActor = HitResult.GetActor();
	return HitResult.bBlockingHit
		&& HitResult.GetComponent()
		&& (!HasAuthoredSpawnGround.GetValue() || (HitActor && HitActor->ActorHasTag(TEXT("SurvivalMapGround"))))
		&& (!HitActor || (!Cast<AEnemyBase>(HitActor) && !Cast<ACharacterBase>(HitActor)))
		&& HitResult.ImpactNormal.Z >= EnemySpawnWalkableGroundNormalZ;
}

bool AEnemySpawner::HasGroundSupport(const FVector& Ground, float Radius) const
{
    if (!GetWorld()) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SpawnFootprint), false, this);
    // Short traces never accept a prop roof high above the actual landing floor.
    for (const FVector& Direction : {FVector(1,0,0),FVector(-1,0,0),FVector(0,1,0),FVector(0,-1,0)})
    {
        const FVector Edge=Ground+Direction*FMath::Max(0.f,Radius)*.9f;
        TArray<FHitResult> Hits;
        TraceSpawnGround(GetWorld(),Hits,Edge+FVector(0,0,25),Edge-FVector(0,0,35),Query);
        if (!Hits.ContainsByPredicate([this](const FHitResult& Hit){return IsValidSpawnGround(Hit);})) return false;
    }
    return true;
}

bool AEnemySpawner::ValidateObjectiveLocation(const FVector& Desired, float Radius, float Height, FVector& OutGround) const
{
    Radius=FMath::Max(1.f,Radius); Height=FMath::Max(1.f,Height);
    if (!ProjectEncounterGround(Desired,OutGround) || !CombatGround::IsInsideArena(GetWorld(),OutGround,Radius)
        || !HasGroundSupport(OutGround,Radius)) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ObjectiveSpawnClearance),false,this);
    const FVector Center=OutGround+FVector(0,0,Height*.5f+8.f);
    const FCollisionShape Shape=FCollisionShape::MakeBox(FVector(Radius,Radius,Height*.5f));
    return !GetWorld()->OverlapBlockingTestByChannel(Center,FQuat::Identity,ECC_Pawn,Shape,Query)
        && !GetWorld()->OverlapBlockingTestByChannel(Center,FQuat::Identity,ECC_GameTraceChannel1,Shape,Query);
}

// Multi-channel traces stop at the first blocker, even when that hit is later rejected.
// Retry past transient characters so a crowd cannot hide valid floor from encounters.
static void TraceSpawnGround(UWorld* World, TArray<FHitResult>& Hits, const FVector& Start,
    const FVector& End, FCollisionQueryParams Params)
{
    for (int32 Attempt=0; Attempt<32; ++Attempt)
    {
        World->LineTraceMultiByChannel(Hits,Start,End,EnemySpawnGroundTraceChannel,Params);
        AActor* Blocker=nullptr;
        for (const FHitResult& Hit:Hits)
            if (Hit.bBlockingHit && (Cast<AEnemyBase>(Hit.GetActor()) || Cast<ACharacterBase>(Hit.GetActor())))
                Blocker=Hit.GetActor();
        if (!Blocker) return;
        Params.AddIgnoredActor(Blocker);
    }
    Hits.Reset(); // Fail safely if an unusually deep stack exceeds the bounded retry budget.
}

bool AEnemySpawner::FindSpawnLocation(const FVector& ActivePlayerLocation, TSubclassOf<AEnemyBase> EnemyClass, FVector& OutSpawnLocation,
	bool bUseDirectionalArc, float DirectionAngleDegrees, float ArcDegrees, float DistanceMin, float DistanceMax) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_EnemySpawner_FindSpawnLocation);
	const int32 AttemptCount = FMath::Max(1, MaxSpawnLocationAttempts);
	float CapsuleRadius = 0.0f;
	float CapsuleHalfHeight = 0.0f;
	GetEnemyCapsuleDimensions(EnemyClass, CapsuleRadius, CapsuleHalfHeight);
	int32 CandidateFailures = 0;
	int32 NavigationFailures = 0;
	int32 ArenaFailures = 0;
	int32 CollisionFailures = 0;

	for (int32 Attempt = 0; Attempt < AttemptCount; ++Attempt)
	{
		FVector CandidateLocation;
		if (!GenerateCandidateSpawnLocation(ActivePlayerLocation, Attempt, AttemptCount, CandidateLocation,
			bUseDirectionalArc, DirectionAngleDegrees, ArcDegrees, DistanceMin, DistanceMax))
		{
			++CandidateFailures;
			continue;
		}

		FVector ProjectedLocation;
		if (!ProjectSpawnLocationToNavigation(CandidateLocation, ProjectedLocation))
		{
			++NavigationFailures;
			if (bDebugSpawnValidation)
			{
				DrawDebugSphere(GetWorld(), CandidateLocation, 45.0f, 12, FColor::Yellow, false, 2.0f, 0, 2.0f);
			}
			continue;
		}

		// Keep the capsule just above the floor contact tolerance so the engine's
		// final collision check does not need to move it off its validated point.
		const FVector ActorLocation = ProjectedLocation + FVector(0.0f, 0.0f, CapsuleHalfHeight + 2.f);
		if (!IsSpawnLocationInsideArena(ActorLocation) || !CombatGround::IsInsideArena(GetWorld(),ActorLocation,CapsuleRadius))
		{
			++ArenaFailures;
			if (bDebugSpawnValidation)
			{
				DrawDebugSphere(GetWorld(), ActorLocation, 45.0f, 12, FColor::Yellow, false, 2.0f, 0, 2.0f);
			}
			continue;
		}

		if (!HasGroundSupport(ProjectedLocation,CapsuleRadius) || !IsSpawnLocationCollisionFree(ActorLocation, EnemyClass))
		{
			++CollisionFailures;
			if (bDebugSpawnValidation)
			{
				DrawDebugCapsule(GetWorld(), ActorLocation, CapsuleHalfHeight, CapsuleRadius, FQuat::Identity, FColor::Red, false, 2.0f, 0, 2.0f);
			}
			continue;
		}

		OutSpawnLocation = ActorLocation;
		if (bDebugSpawnValidation)
		{
			DrawDebugCapsule(GetWorld(), OutSpawnLocation, CapsuleHalfHeight, CapsuleRadius, FQuat::Identity, FColor::Green, false, 2.0f, 0, 2.0f);
		}
		return true;
	}

	if (bDebugSpawning || bDebugSpawnValidation)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner %s failed spawn validation. Attempts=%d CandidateFailures=%d NavigationFailures=%d ArenaFailures=%d CollisionFailures=%d ActiveSpawnArea=%s RequireNavMesh=%s EdgePadding=%.1f CapsuleRadius=%.1f CapsuleHalfHeight=%.1f MinDistance=%.1f MaxDistance=%.1f"),
			*GetNameSafe(this),
			AttemptCount,
			CandidateFailures,
			NavigationFailures,
			ArenaFailures,
			CollisionFailures,
			*GetNameSafe(ActiveSpawnArea),
			bRequireNavMeshProjection ? TEXT("true") : TEXT("false"),
			SpawnEdgePadding,
			CapsuleRadius,
			CapsuleHalfHeight,
			MinSpawnDistance,
			MaxSpawnDistance);
	}

	return false;
}

bool AEnemySpawner::GenerateCandidateSpawnLocation(const FVector& ActivePlayerLocation, int32 AttemptIndex, int32 AttemptCount, FVector& OutCandidateLocation,
	bool bUseDirectionalArc, float DirectionAngleDegrees, float ArcDegrees, float DistanceMin, float DistanceMax) const
{
	const int32 DirectionalAttemptCount = bUseDirectionalArc ? FMath::Max(1, FMath::CeilToInt(AttemptCount * 0.7f)) : 0;
	if (bUseDirectionalArc && AttemptIndex < DirectionalAttemptCount)
	{
		const float SafeMinDistance = DistanceMin > 0.0f ? DistanceMin : MinSpawnDistance;
		const float SafeMaxDistance = DistanceMax > 0.0f ? DistanceMax : MaxSpawnDistance;
		const float AngleDegrees = DirectionAngleDegrees + FMath::FRandRange(-FMath::Abs(ArcDegrees) * 0.5f, FMath::Abs(ArcDegrees) * 0.5f);
		const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
		const float Distance = FMath::FRandRange(FMath::Min(SafeMinDistance, SafeMaxDistance), FMath::Max(SafeMinDistance, SafeMaxDistance));
		OutCandidateLocation = ActivePlayerLocation + FVector(FMath::Cos(AngleRadians) * Distance, FMath::Sin(AngleRadians) * Distance, 0.0f);
		return true;
	}

	const bool bUseAreaCandidate = ActiveSpawnArea
		&& bUseSpawnAreaFallbackCandidates
		&& AttemptIndex >= FMath::Max(1, AttemptCount / 2);

	if (bUseAreaCandidate)
	{
		OutCandidateLocation = ActiveSpawnArea->GetRandomLocationInside(SpawnEdgePadding);
		const float DistanceSquared = FVector::DistSquared2D(OutCandidateLocation, ActivePlayerLocation);
		if (DistanceSquared >= FMath::Square(FMath::Min(MinSpawnDistance, MaxSpawnDistance))
			&& DistanceSquared <= FMath::Square(FMath::Max(MinSpawnDistance, MaxSpawnDistance)))
		{
			return true;
		}
	}

	const float SafeMaxRadius = FMath::Max(MinSpawnDistance, MaxSpawnDistance);
	const float SafeMinRadius = FMath::Min(MinSpawnDistance, SafeMaxRadius);
	const float AngleRadians = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Distance = FMath::FRandRange(SafeMinRadius, SafeMaxRadius);
	const FVector SpawnOffset(FMath::Cos(AngleRadians) * Distance, FMath::Sin(AngleRadians) * Distance, 0.0f);
	OutCandidateLocation = ActivePlayerLocation + SpawnOffset;
	return true;
}

bool AEnemySpawner::ProjectSpawnLocationToNavigation(const FVector& CandidateLocation, FVector& OutProjectedLocation) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_EnemySpawner_ProjectSpawnLocation);
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector TraceStart = CandidateLocation + FVector(0.0f, 0.0f, GroundTraceHeight);
	const FVector TraceEnd = CandidateLocation - FVector(0.0f, 0.0f, GroundTraceDepth);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemySpawnerGroundTrace), false, this);
	QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this,0));

	TArray<FHitResult> HitResults;
	TraceSpawnGround(World, HitResults, TraceStart, TraceEnd, QueryParams);

	FHitResult GroundHit;
	bool bHitGround = false;
	for (const FHitResult& HitResult : HitResults)
	{
		if (IsValidSpawnGround(HitResult))
		{
			GroundHit = HitResult;
			bHitGround = true;
			break;
		}

		if (CVarDebugEnemySpawnGround.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("EnemySpawner rejected spawn ground hit Candidate=%s HitActor=%s HitComponent=%s HitZ=%.2f ImpactNormal=%s"),
				*CandidateLocation.ToString(),
				*GetNameSafe(HitResult.GetActor()),
				*GetNameSafe(HitResult.GetComponent()),
				HitResult.Location.Z,
				*HitResult.ImpactNormal.ToString());
		}
	}

	if (bDebugSpawning || bDebugSpawnValidation)
	{
		DrawDebugLine(World, TraceStart, TraceEnd, bHitGround ? FColor::Green : FColor::Yellow, false, 2.0f, 0, 1.5f);
	}

	if (!bHitGround)
	{
		return false;
	}

	const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavigationSystem)
	{
		OutProjectedLocation = GroundHit.Location;
		return !bRequireNavMeshProjection;
	}

	FNavLocation NavLocation;
	if (!NavigationSystem->ProjectPointToNavigation(GroundHit.Location, NavLocation, NavMeshProjectionExtent))
	{
		OutProjectedLocation = GroundHit.Location;
		return !bRequireNavMeshProjection;
	}

	const FVector NavGroundTraceStart = FVector(NavLocation.Location.X, NavLocation.Location.Y, CandidateLocation.Z + GroundTraceHeight);
	const FVector NavGroundTraceEnd = FVector(NavLocation.Location.X, NavLocation.Location.Y, CandidateLocation.Z - GroundTraceDepth);
	TArray<FHitResult> NavGroundHits;
	TraceSpawnGround(World, NavGroundHits, NavGroundTraceStart, NavGroundTraceEnd, QueryParams);

	FHitResult NavGroundHit;
	bool bHitNavGround = false;
	for (const FHitResult& HitResult : NavGroundHits)
	{
		if (IsValidSpawnGround(HitResult))
		{
			NavGroundHit = HitResult;
			bHitNavGround = true;
			break;
		}
	}

	if (!bHitNavGround)
	{
		if (CVarDebugEnemySpawnGround.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("EnemySpawner rejected nav-projected spawn because no valid ground was found beneath projected XY. Candidate=%s NavLocation=%s InitialGroundActor=%s InitialGroundZ=%.2f"),
				*CandidateLocation.ToString(),
				*NavLocation.Location.ToString(),
				*GetNameSafe(GroundHit.GetActor()),
				GroundHit.Location.Z);
		}
		return false;
	}

	OutProjectedLocation = FVector(NavLocation.Location.X, NavLocation.Location.Y, NavGroundHit.Location.Z);
    // A projection onto nearby floor must not be moved back onto a roof by the
    // second downward trace, or snap across a fence to an unrelated ledge.
    if (FMath::Abs(OutProjectedLocation.Z-NavLocation.Location.Z)>45.f
        || FVector::DistSquared2D(CandidateLocation,OutProjectedLocation)>FMath::Square(100.f)) return false;
	if (FMath::Abs(OutProjectedLocation.Z - CandidateLocation.Z) > 25.0f && CVarDebugEnemySpawnGround.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("EnemySpawner spawn ground Z adjusted CandidateZ=%.2f GroundZ=%.2f NavZ=%.2f HitActor=%s HitComponent=%s ImpactNormal=%s"),
			CandidateLocation.Z,
			OutProjectedLocation.Z,
			NavLocation.Location.Z,
			*GetNameSafe(NavGroundHit.GetActor()),
			*GetNameSafe(NavGroundHit.GetComponent()),
			*NavGroundHit.ImpactNormal.ToString());
	}
	return true;
}

bool AEnemySpawner::IsSpawnLocationInsideArena(const FVector& Location) const
{
	return ActiveSpawnArea ? ActiveSpawnArea->ContainsSpawnLocation(Location, SpawnEdgePadding)
        : CombatGround::IsInsideArena(GetWorld(),Location,SpawnEdgePadding);
}

bool AEnemySpawner::IsSpawnLocationCollisionFree(const FVector& Location, TSubclassOf<AEnemyBase> EnemyClass) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_EnemySpawner_CollisionValidation);
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	float CapsuleRadius = 0.0f;
	float CapsuleHalfHeight = 0.0f;
	GetEnemyCapsuleDimensions(EnemyClass, CapsuleRadius, CapsuleHalfHeight);
	const float GroundClearance = FMath::Clamp(CollisionValidationGroundClearance, 0.0f, CapsuleHalfHeight - CapsuleRadius);
	const FVector ValidationLocation = Location + FVector(0.0f, 0.0f, GroundClearance);
	const float ValidationHalfHeight = FMath::Max(CapsuleRadius, CapsuleHalfHeight - GroundClearance);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemySpawnerCapsuleValidation), false, this);
	FCollisionResponseParams ResponseParams;
	return !World->OverlapBlockingTestByChannel(ValidationLocation,FQuat::Identity,ECC_GameTraceChannel1,
        FCollisionShape::MakeCapsule(CapsuleRadius,ValidationHalfHeight),QueryParams,ResponseParams)
        && !World->OverlapBlockingTestByChannel(
		ValidationLocation,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeCapsule(CapsuleRadius, ValidationHalfHeight),
		QueryParams,
		ResponseParams);
}

void AEnemySpawner::GetEnemyCapsuleDimensions(TSubclassOf<AEnemyBase> EnemyClass, float& OutRadius, float& OutHalfHeight) const
{
	OutRadius = FMath::Max(1.0f, FallbackCapsuleRadius);
	OutHalfHeight = FMath::Max(OutRadius, FallbackCapsuleHalfHeight);

	const AEnemyBase* EnemyDefaultObject = EnemyClass ? EnemyClass->GetDefaultObject<AEnemyBase>() : nullptr;
	const UCapsuleComponent* CapsuleComponent = EnemyDefaultObject ? EnemyDefaultObject->GetCapsuleComponent() : nullptr;
	if (!CapsuleComponent)
	{
		return;
	}

	// CDO scene components have no registered world transform. Their cached
	// GetScaledCapsule* values therefore omit Blueprint root scale (e.g. the
	// grunt's 1.08 scale) and place the live capsule inside the floor.
	const float ShapeScale = CapsuleComponent->GetRelativeScale3D().GetAbsMin();
	OutRadius = FMath::Max(1.0f, CapsuleComponent->GetUnscaledCapsuleRadius() * ShapeScale);
	OutHalfHeight = FMath::Max(OutRadius, CapsuleComponent->GetUnscaledCapsuleHalfHeight() * ShapeScale);
}

