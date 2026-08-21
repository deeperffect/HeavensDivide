// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemySpawner.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/CapsuleComponent.h"
#include "Curves/CurveFloat.h"
#include "DrawDebugHelpers.h"
#include "EnemyBase.h"
#include "EnemySpawnArea.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Stats/Stats.h"
#include "SurvivorPlayerController.h"
#include "AutoAttackComponent.h"

static TAutoConsoleVariable<int32> CVarLogSpawnDirector(
	TEXT("hd.LogSpawnDirector"),
	0,
	TEXT("When set to 1, logs periodic survivor spawn director status and spawn batches."));

static TAutoConsoleVariable<int32> CVarDebugEnemySpawnGround(
	TEXT("hd.DebugEnemySpawnGround"),
	0,
	TEXT("When set to 1, logs rejected enemy spawn ground hits and suspicious spawn Z changes."));

static constexpr float EnemySpawnWalkableGroundNormalZ = 0.7f;
static constexpr ECollisionChannel EnemySpawnGroundTraceChannel = ECC_GameTraceChannel2;

static bool IsValidEnemySpawnGroundHit(const FHitResult& HitResult)
{
	AActor* HitActor = HitResult.GetActor();
	return HitResult.bBlockingHit
		&& HitResult.GetComponent()
		&& (!HitActor || (!Cast<AEnemyBase>(HitActor) && !Cast<ACharacterBase>(HitActor)))
		&& HitResult.ImpactNormal.Z >= EnemySpawnWalkableGroundNormalZ;
}

#if !UE_BUILD_SHIPPING
static AEnemySpawner* FindEnemySpawnerForDebugCommand(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AEnemySpawner> SpawnerIt(World); SpawnerIt; ++SpawnerIt)
	{
		return *SpawnerIt;
	}

	return nullptr;
}

static void StressEnemiesCommand(const TArray<FString>& Args, UWorld* World)
{
	AEnemySpawner* Spawner = FindEnemySpawnerForDebugCommand(World);
	if (!Spawner)
	{
		UE_LOG(LogTemp, Warning, TEXT("hd.StressEnemies failed: no AEnemySpawner found in world."));
		return;
	}

	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: hd.StressEnemies <DesiredLivingStressEnemies>"));
		return;
	}

	Spawner->StressEnemies(FCString::Atoi(*Args[0]));
}

static void ClearStressEnemiesCommand(const TArray<FString>& Args, UWorld* World)
{
	if (AEnemySpawner* Spawner = FindEnemySpawnerForDebugCommand(World))
	{
		Spawner->ClearStressEnemies();
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("hd.ClearStressEnemies failed: no AEnemySpawner found in world."));
}

static FAutoConsoleCommandWithWorldAndArgs GStressEnemiesCommand(
	TEXT("hd.StressEnemies"),
	TEXT("Development only. Sets the desired living stress-test enemy count. Usage: hd.StressEnemies 100"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StressEnemiesCommand));

static FAutoConsoleCommandWithWorldAndArgs GClearStressEnemiesCommand(
	TEXT("hd.ClearStressEnemies"),
	TEXT("Development only. Destroys only enemies created by hd.StressEnemies."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ClearStressEnemiesCommand));
#endif

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (RunTimeUpdateInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(RunTimeTimerHandle, this, &AEnemySpawner::HandleRunTimeTimerElapsed, RunTimeUpdateInterval, true);
	}

	if (DistantEnemyCheckInterval > 0.0f && MaxEnemyDistanceFromPlayer > 0.0f)
	{
		GetWorldTimerManager().SetTimer(DistantEnemyCheckTimerHandle, this, &AEnemySpawner::HandleDistantEnemyCheckTimerElapsed, DistantEnemyCheckInterval, true);
	}

	if (bSpawningEnabled)
	{
		StartSpawning();
	}
}

void AEnemySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSpawning();
	GetWorldTimerManager().ClearTimer(RunTimeTimerHandle);
	GetWorldTimerManager().ClearTimer(DistantEnemyCheckTimerHandle);
#if !UE_BUILD_SHIPPING
	GetWorldTimerManager().ClearTimer(StressSpawnTimerHandle);
#endif

	for (TWeakObjectPtr<AEnemyBase>& EnemyPtr : SpawnedEnemies)
	{
		if (AEnemyBase* Enemy = EnemyPtr.Get())
		{
			Enemy->OnDestroyed.RemoveDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDestroyed);
		}
	}

#if !UE_BUILD_SHIPPING
	for (TWeakObjectPtr<AEnemyBase>& EnemyPtr : StressTestEnemies)
	{
		if (AEnemyBase* Enemy = EnemyPtr.Get())
		{
			Enemy->OnDestroyed.RemoveDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDestroyed);
		}
	}
	StressTestEnemies.Empty();
#endif

	SpawnedEnemies.Empty();
	AliveEnemyCountByClass.Empty();
	CountedEnemyClassByEnemy.Empty();

	Super::EndPlay(EndPlayReason);
}

void AEnemySpawner::StartSpawning()
{
	if (!bSpawningEnabled || GetCurrentSpawnInterval() <= 0.0f
#if !UE_BUILD_SHIPPING
		|| bStressPauseNormalSpawning
#endif
		)
	{
		return;
	}

	RescheduleSpawnTimer();
}

void AEnemySpawner::StopSpawning()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
}

void AEnemySpawner::SetSpawningEnabled(bool bEnabled)
{
	bSpawningEnabled = bEnabled;

	if (bSpawningEnabled)
	{
		StartSpawning();
	}
	else
	{
		StopSpawning();
	}
}

AEnemyBase* AEnemySpawner::SpawnEnemy()
{
	if (!bSpawningEnabled
#if !UE_BUILD_SHIPPING
		|| bStressPauseNormalSpawning
#endif
		)
	{
		if (bDebugSpawning)
		{
			UE_LOG(LogTemp, Log, TEXT("EnemySpawner %s skipped manual spawn: spawning disabled."), *GetNameSafe(this));
		}
		return nullptr;
	}

	const FEnemySpawnEntry* SpawnEntry = ChooseSpawnEntry(GetCurrentSpawnBudget());
	if (!SpawnEntry)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner %s has no valid enemy spawn entries."), *GetNameSafe(this));
		return nullptr;
	}

	return SpawnEnemyFromEntry(*SpawnEntry);
}

#if !UE_BUILD_SHIPPING
void AEnemySpawner::StressEnemies(int32 DesiredLivingEnemyCount)
{
	if (DesiredLivingEnemyCount > 0 && !IsStressTestEnemyClassConfigured())
	{
		UE_LOG(LogTemp, Warning, TEXT("StressTestEnemyClass is not configured on %s. Set BP_EnemySpawner -> Stress Test -> Stress Test Enemy Class before running hd.StressEnemies."), *GetNameSafe(this));
		return;
	}

	PruneStressTestEnemies();
	RequestedStressEnemyCount = FMath::Max(0, DesiredLivingEnemyCount);

	if (RequestedStressEnemyCount > 0)
	{
		SetStressPauseNormalSpawning(true);
		DisablePlayerAutoAttacksForStressTest();
	}

	const int32 CurrentCount = GetLivingStressEnemyCount();
	if (RequestedStressEnemyCount <= CurrentCount)
	{
		int32 EnemiesToRemove = CurrentCount - RequestedStressEnemyCount;
		for (int32 Index = StressTestEnemies.Num() - 1; Index >= 0 && EnemiesToRemove > 0; --Index)
		{
			if (AEnemyBase* Enemy = StressTestEnemies[Index].Get())
			{
				Enemy->Destroy();
				--EnemiesToRemove;
			}
			StressTestEnemies.RemoveAtSwap(Index);
		}

		GetWorldTimerManager().ClearTimer(StressSpawnTimerHandle);
		PruneStressTestEnemies();
		if (RequestedStressEnemyCount == 0)
		{
			RestorePlayerAutoAttacksAfterStressTest();
			SetStressPauseNormalSpawning(false);
		}
		return;
	}

	HandleStressSpawnTimerElapsed();
	if (GetLivingStressEnemyCount() < RequestedStressEnemyCount)
	{
		GetWorldTimerManager().SetTimer(
			StressSpawnTimerHandle,
			this,
			&AEnemySpawner::HandleStressSpawnTimerElapsed,
			FMath::Max(0.01f, StressSpawnBatchInterval),
			true);
	}
}

void AEnemySpawner::ClearStressEnemies()
{
	GetWorldTimerManager().ClearTimer(StressSpawnTimerHandle);
	RequestedStressEnemyCount = 0;
	RestorePlayerAutoAttacksAfterStressTest();

	int32 ClearedCount = 0;
	for (TWeakObjectPtr<AEnemyBase>& EnemyPtr : StressTestEnemies)
	{
		if (AEnemyBase* Enemy = EnemyPtr.Get())
		{
			Enemy->Destroy();
			++ClearedCount;
		}
	}

	StressTestEnemies.Empty();
	SetStressPauseNormalSpawning(false);
	UE_LOG(LogTemp, Log, TEXT("Cleared %d stress-test enemies."), ClearedCount);
}

void AEnemySpawner::SetStressPauseNormalSpawning(bool bPause)
{
	if (bStressPauseNormalSpawning == bPause)
	{
		return;
	}

	if (bPause)
	{
		bSavedSpawningEnabledBeforeStressPause = bSpawningEnabled;
		bStressPauseNormalSpawning = true;
		StopSpawning();
		return;
	}

	bStressPauseNormalSpawning = false;
	if (bSavedSpawningEnabledBeforeStressPause && bSpawningEnabled)
	{
		StartSpawning();
	}
}
#endif

float AEnemySpawner::GetRunTimeSeconds() const
{
	return RunTimeSeconds;
}

float AEnemySpawner::GetRunTimeMinutes() const
{
	return RunTimeSeconds / 60.0f;
}

AEnemyBase* AEnemySpawner::SpawnEnemyFromEntry(const FEnemySpawnEntry& SpawnEntry)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_EnemySpawner_SpawnEnemyFromEntry);
	PruneTrackedEnemies();

	const int32 AliveEnemyCount = GetAliveEnemyCount();
	const int32 CurrentMaxAliveEnemies = GetCurrentMaxAliveEnemies();
	if (AliveEnemyCount >= CurrentMaxAliveEnemies)
	{
		if (bDebugSpawning)
		{
			UE_LOG(LogTemp, Log, TEXT("EnemySpawner %s skipped spawn. Alive=%d Max=%d"), *GetNameSafe(this), AliveEnemyCount, CurrentMaxAliveEnemies);
		}
		return nullptr;
	}

	if (!IsSpawnEntryEligible(SpawnEntry, GetCurrentSpawnBudget(), true))
	{
		return nullptr;
	}

	ACharacterBase* ActivePlayer = GetActivePlayerCharacter();
	if (!ActivePlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner %s could not find an active player character."), *GetNameSafe(this));
		return nullptr;
	}

	FVector SpawnLocation;
	if (!FindSpawnLocation(ActivePlayer->GetActorLocation(), SpawnEntry.EnemyClass, SpawnLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner %s could not find a valid spawn location."), *GetNameSafe(this));
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	AEnemyBase* SpawnedEnemy = GetWorld()->SpawnActor<AEnemyBase>(SpawnEntry.EnemyClass, SpawnLocation, FRotator::ZeroRotator, SpawnParameters);
	if (!SpawnedEnemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner %s failed to spawn %s."), *GetNameSafe(this), *GetNameSafe(SpawnEntry.EnemyClass.Get()));
		return nullptr;
	}

	const float SpawnAdjustmentDeltaZ = SpawnedEnemy->GetActorLocation().Z - SpawnLocation.Z;
	if (FMath::Abs(SpawnAdjustmentDeltaZ) > 25.0f && CVarDebugEnemySpawnGround.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner spawn collision adjustment changed Z for %s RequestedZ=%.2f ActualZ=%.2f DeltaZ=%.2f"),
			*GetNameSafe(SpawnedEnemy),
			SpawnLocation.Z,
			SpawnedEnemy->GetActorLocation().Z,
			SpawnAdjustmentDeltaZ);
	}

	SpawnedEnemy->SpawnDefaultController();
	SpawnedEnemy->ApplySpawnDifficultyScaling(GetHealthMultiplier(), GetDamageMultiplier());
	SpawnedEnemy->OnDestroyed.AddDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDestroyed);
	SpawnedEnemies.Add(SpawnedEnemy);
	IncrementAliveCountForSpawnedEnemy(SpawnedEnemy, SpawnEntry.EnemyClass);

	if (bDebugSpawning)
	{
		const int32 AliveOfType = GetAliveCountForSpawnClass(SpawnEntry.EnemyClass);
		UE_LOG(LogTemp, Log, TEXT("EnemySpawner spawned %s at %s. Alive=%d Max=%d AliveOfType=%d/%d"),
			*GetNameSafe(SpawnEntry.EnemyClass.Get()),
			*SpawnLocation.ToString(),
			GetAliveEnemyCount(),
			CurrentMaxAliveEnemies,
			AliveOfType,
			SpawnEntry.MaxAliveOfThisType);

		DrawDebugSphere(GetWorld(), SpawnLocation, 40.0f, 16, FColor::Red, false, 2.0f, 0, 2.0f);
	}

	return SpawnedEnemy;
}

ACharacterBase* AEnemySpawner::GetActivePlayerCharacter() const
{
	const ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (!SurvivorController)
	{
		return nullptr;
	}

	const UCharacterManagerComponent* CharacterManager = SurvivorController->GetCharacterManager();
	if (!CharacterManager)
	{
		return nullptr;
	}

	return CharacterManager->GetActiveCharacter();
}

const FEnemySpawnEntry* AEnemySpawner::ChooseSpawnEntry(int32 RemainingBudget) const
{
	float TotalWeight = 0.0f;
	for (const FEnemySpawnEntry& Entry : EnemySpawnEntries)
	{
		if (IsSpawnEntryEligible(Entry, RemainingBudget, true))
		{
			TotalWeight += Entry.SpawnWeight;
		}
	}

	if (TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const FEnemySpawnEntry& Entry : EnemySpawnEntries)
	{
		if (!IsSpawnEntryEligible(Entry, RemainingBudget, false))
		{
			continue;
		}

		Roll -= Entry.SpawnWeight;
		if (Roll <= 0.0f)
		{
			return &Entry;
		}
	}

	return nullptr;
}

bool AEnemySpawner::FindSpawnLocation(const FVector& ActivePlayerLocation, TSubclassOf<AEnemyBase> EnemyClass, FVector& OutSpawnLocation) const
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
		if (!GenerateCandidateSpawnLocation(ActivePlayerLocation, Attempt, AttemptCount, CandidateLocation))
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

		const FVector ActorLocation = ProjectedLocation + FVector(0.0f, 0.0f, CapsuleHalfHeight);
		if (!IsSpawnLocationInsideArena(ActorLocation))
		{
			++ArenaFailures;
			if (bDebugSpawnValidation)
			{
				DrawDebugSphere(GetWorld(), ActorLocation, 45.0f, 12, FColor::Yellow, false, 2.0f, 0, 2.0f);
			}
			continue;
		}

		if (!IsSpawnLocationCollisionFree(ActorLocation, EnemyClass))
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

bool AEnemySpawner::GenerateCandidateSpawnLocation(const FVector& ActivePlayerLocation, int32 AttemptIndex, int32 AttemptCount, FVector& OutCandidateLocation) const
{
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

	TArray<FHitResult> HitResults;
	const bool bHasGroundHits = World->LineTraceMultiByChannel(HitResults, TraceStart, TraceEnd, EnemySpawnGroundTraceChannel, QueryParams);

	FHitResult GroundHit;
	bool bHitGround = false;
	for (const FHitResult& HitResult : HitResults)
	{
		if (IsValidEnemySpawnGroundHit(HitResult))
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
	World->LineTraceMultiByChannel(NavGroundHits, NavGroundTraceStart, NavGroundTraceEnd, EnemySpawnGroundTraceChannel, QueryParams);

	FHitResult NavGroundHit;
	bool bHitNavGround = false;
	for (const FHitResult& HitResult : NavGroundHits)
	{
		if (IsValidEnemySpawnGroundHit(HitResult))
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
	return !ActiveSpawnArea || ActiveSpawnArea->ContainsSpawnLocation(Location, SpawnEdgePadding);
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
	return !World->OverlapBlockingTestByChannel(
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

	OutRadius = FMath::Max(1.0f, CapsuleComponent->GetUnscaledCapsuleRadius());
	OutHalfHeight = FMath::Max(OutRadius, CapsuleComponent->GetUnscaledCapsuleHalfHeight());
}

int32 AEnemySpawner::GetAliveEnemyCount()
{
	PruneTrackedEnemies();

	int32 AliveCount = 0;
	for (const TWeakObjectPtr<AEnemyBase>& EnemyPtr : SpawnedEnemies)
	{
		const AEnemyBase* Enemy = EnemyPtr.Get();
		if (Enemy && !Enemy->IsDead())
		{
			++AliveCount;
		}
	}

	return AliveCount;
}

float AEnemySpawner::GetHealthMultiplier() const
{
	return HealthMultiplierCurve ? FMath::Max(0.0f, HealthMultiplierCurve->GetFloatValue(RunTimeSeconds)) : EvaluateDefaultHealthMultiplier();
}

float AEnemySpawner::GetDamageMultiplier() const
{
	return DamageMultiplierCurve ? FMath::Max(0.0f, DamageMultiplierCurve->GetFloatValue(RunTimeSeconds)) : EvaluateDefaultDamageMultiplier();
}

float AEnemySpawner::GetSpawnPressure() const
{
	return SpawnPressureCurve ? FMath::Max(0.0f, SpawnPressureCurve->GetFloatValue(RunTimeSeconds)) : EvaluateDefaultSpawnPressure();
}

float AEnemySpawner::GetCurrentSpawnInterval() const
{
	const float Pressure = FMath::Max(0.1f, GetSpawnPressure());
	return FMath::Max(MinSpawnInterval, BaseSpawnInterval / Pressure);
}

int32 AEnemySpawner::GetCurrentEnemiesPerSpawn() const
{
	const float Pressure = GetSpawnPressure();
	return FMath::Clamp(FMath::RoundToInt(BaseEnemiesPerSpawn * FMath::Sqrt(FMath::Max(1.0f, Pressure))), 1, MaxEnemiesPerSpawn);
}

int32 AEnemySpawner::GetCurrentMaxAliveEnemies() const
{
	const float Pressure = GetSpawnPressure();
	return FMath::Clamp(FMath::RoundToInt(BaseMaxAliveEnemies * Pressure), 1, MaximumAliveEnemies);
}

int32 AEnemySpawner::GetCurrentSpawnBudget() const
{
	const float Pressure = GetSpawnPressure();
	return FMath::Clamp(FMath::RoundToInt(BaseSpawnBudget * FMath::Sqrt(FMath::Max(1.0f, Pressure))), 1, MaxSpawnBudget);
}

float AEnemySpawner::EvaluateDefaultHealthMultiplier() const
{
	const int32 CompletedMinutes = FMath::Max(0, FMath::FloorToInt(GetRunTimeMinutes()));
	if (CompletedMinutes <= 5)
	{
		return 1.0f + (0.2f * CompletedMinutes);
	}

	return FMath::Min(2.75f, 2.0f + (0.15f * (CompletedMinutes - 5)));
}

float AEnemySpawner::EvaluateDefaultDamageMultiplier() const
{
	const float Minutes = GetRunTimeMinutes();
	if (Minutes <= 5.0f)
	{
		return FMath::Lerp(1.0f, 1.25f, Minutes / 5.0f);
	}

	return FMath::Lerp(1.25f, 1.6f, FMath::Clamp((Minutes - 5.0f) / 5.0f, 0.0f, 1.0f));
}

float AEnemySpawner::EvaluateDefaultSpawnPressure() const
{
	const float Minutes = GetRunTimeMinutes();
	if (Minutes <= 5.0f)
	{
		return FMath::Lerp(1.0f, 2.15f, Minutes / 5.0f);
	}

	return FMath::Lerp(2.15f, 3.35f, FMath::Clamp((Minutes - 5.0f) / 5.0f, 0.0f, 1.0f));
}

bool AEnemySpawner::IsSpawnEntryEligible(const FEnemySpawnEntry& Entry, int32 RemainingBudget, bool bLogLimitFailures) const
{
	const bool bTimeAllowed = RunTimeSeconds >= Entry.MinimumRunTime
		&& (Entry.MaximumRunTime <= 0.0f || RunTimeSeconds <= Entry.MaximumRunTime);
	const bool bBudgetAllowed = Entry.SpawnCost <= FMath::Max(1, RemainingBudget);
	if (!Entry.bEnabled || !Entry.EnemyClass || Entry.SpawnWeight <= 0.0f || !bTimeAllowed || !bBudgetAllowed)
	{
		return false;
	}

	if (Entry.MaxAliveOfThisType > 0)
	{
		const int32 AliveOfType = GetAliveCountForSpawnClass(Entry.EnemyClass);
		if (AliveOfType >= Entry.MaxAliveOfThisType)
		{
			if (bLogLimitFailures && bDebugSpawning)
			{
				UE_LOG(LogTemp, Log, TEXT("EnemySpawner skipped %s: alive of type %d / max %d"),
					*GetNameSafe(Entry.EnemyClass.Get()),
					AliveOfType,
					Entry.MaxAliveOfThisType);
			}
			return false;
		}
	}

	return true;
}

int32 AEnemySpawner::GetAliveCountForSpawnClass(TSubclassOf<AEnemyBase> EnemyClass) const
{
	if (!EnemyClass)
	{
		return 0;
	}

	const int32* AliveCount = AliveEnemyCountByClass.Find(EnemyClass.Get());
	return AliveCount ? FMath::Max(0, *AliveCount) : 0;
}

void AEnemySpawner::IncrementAliveCountForSpawnedEnemy(AEnemyBase* SpawnedEnemy, TSubclassOf<AEnemyBase> SpawnClass)
{
	if (!SpawnedEnemy || !SpawnClass)
	{
		return;
	}

	UClass* CountedClass = SpawnClass.Get();
	int32& AliveCount = AliveEnemyCountByClass.FindOrAdd(CountedClass);
	++AliveCount;
	CountedEnemyClassByEnemy.FindOrAdd(TObjectKey<AEnemyBase>(SpawnedEnemy)) = CountedClass;
}

void AEnemySpawner::DecrementAliveCountForDestroyedEnemy(AActor* DestroyedActor)
{
	AEnemyBase* DestroyedEnemy = Cast<AEnemyBase>(DestroyedActor);
	if (!DestroyedEnemy)
	{
		return;
	}

	UClass* CountedClass = nullptr;
	if (!CountedEnemyClassByEnemy.RemoveAndCopyValue(TObjectKey<AEnemyBase>(DestroyedEnemy), CountedClass) || !CountedClass)
	{
		return;
	}

	int32* AliveCount = AliveEnemyCountByClass.Find(CountedClass);
	if (!AliveCount)
	{
		return;
	}

	*AliveCount = FMath::Max(0, *AliveCount - 1);
	if (*AliveCount <= 0)
	{
		AliveEnemyCountByClass.Remove(CountedClass);
	}
}

void AEnemySpawner::PruneTrackedEnemies()
{
	SpawnedEnemies.RemoveAll([](const TWeakObjectPtr<AEnemyBase>& EnemyPtr)
	{
		return !EnemyPtr.IsValid();
	});
}

void AEnemySpawner::HandleRunTimeTimerElapsed()
{
	RunTimeSeconds += RunTimeUpdateInterval;
	RescheduleSpawnTimer();
	LogDirectorStatus();
}

void AEnemySpawner::HandleSpawnTimerElapsed()
{
	if (!bSpawningEnabled
#if !UE_BUILD_SHIPPING
		|| bStressPauseNormalSpawning
#endif
		)
	{
		StopSpawning();
		return;
	}

	const int32 TargetSpawnCount = GetCurrentEnemiesPerSpawn();
	int32 RemainingBudget = GetCurrentSpawnBudget();
	int32 SpawnedCount = 0;

	for (int32 Index = 0; Index < TargetSpawnCount && RemainingBudget > 0; ++Index)
	{
		const FEnemySpawnEntry* Entry = ChooseSpawnEntry(RemainingBudget);
		if (!Entry)
		{
			break;
		}

		AEnemyBase* SpawnedEnemy = SpawnEnemyFromEntry(*Entry);
		if (!SpawnedEnemy)
		{
			break;
		}

		RemainingBudget -= FMath::Max(1, Entry->SpawnCost);
		++SpawnedCount;
	}

	if (SpawnedCount > 0 && CVarLogSpawnDirector.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Spawn Director spawned %d enemies."), SpawnedCount);
	}
}

void AEnemySpawner::HandleDistantEnemyCheckTimerElapsed()
{
	ACharacterBase* ActivePlayer = GetActivePlayerCharacter();
	if (!ActivePlayer || MaxEnemyDistanceFromPlayer <= 0.0f)
	{
		return;
	}

	PruneTrackedEnemies();

	const float MaxDistanceSquared = FMath::Square(MaxEnemyDistanceFromPlayer);
	TArray<TWeakObjectPtr<AEnemyBase>> EnemiesToDespawn;
	EnemiesToDespawn.Reserve(MaxDistantDespawnsPerCheck);

	for (const TWeakObjectPtr<AEnemyBase>& EnemyPtr : SpawnedEnemies)
	{
		AEnemyBase* Enemy = EnemyPtr.Get();
		if (!Enemy || Enemy->IsDead())
		{
			continue;
		}

		if (FVector::DistSquared2D(Enemy->GetActorLocation(), ActivePlayer->GetActorLocation()) > MaxDistanceSquared)
		{
			EnemiesToDespawn.Add(Enemy);
			if (EnemiesToDespawn.Num() >= MaxDistantDespawnsPerCheck)
			{
				break;
			}
		}
	}

	int32 DespawnedCount = 0;
	for (const TWeakObjectPtr<AEnemyBase>& EnemyPtr : EnemiesToDespawn)
	{
		if (AEnemyBase* Enemy = EnemyPtr.Get())
		{
			Enemy->Destroy();
			++DespawnedCount;
		}
	}

	PruneTrackedEnemies();

	if (DespawnedCount > 0 && CVarLogSpawnDirector.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Spawn Director despawned %d distant enemies."), DespawnedCount);
	}
}

void AEnemySpawner::RescheduleSpawnTimer()
{
	if (!bSpawningEnabled
#if !UE_BUILD_SHIPPING
		|| bStressPauseNormalSpawning
#endif
		)
	{
		return;
	}

	const float NewInterval = GetCurrentSpawnInterval();
	if (NewInterval <= 0.0f)
	{
		return;
	}

	const float RemainingTime = GetWorldTimerManager().GetTimerRemaining(SpawnTimerHandle);
	if (!GetWorldTimerManager().IsTimerActive(SpawnTimerHandle) || RemainingTime > NewInterval)
	{
		GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AEnemySpawner::HandleSpawnTimerElapsed, NewInterval, true, NewInterval);
	}
}

void AEnemySpawner::LogDirectorStatus() const
{
	if (CVarLogSpawnDirector.GetValueOnGameThread() == 0 || FMath::FloorToInt(RunTimeSeconds) % 10 != 0)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("=== SPAWN DIRECTOR ===\nRun Time: %.0f sec\nAlive: %d / %d\nSpawn Interval: %.2f\nEnemies Per Spawn: %d\nHealth Multiplier: %.2f\nDamage Multiplier: %.2f\nSpawn Pressure: %.2f\n======================"),
		RunTimeSeconds,
		const_cast<AEnemySpawner*>(this)->GetAliveEnemyCount(),
		GetCurrentMaxAliveEnemies(),
		GetCurrentSpawnInterval(),
		GetCurrentEnemiesPerSpawn(),
		GetHealthMultiplier(),
		GetDamageMultiplier(),
		GetSpawnPressure());
}

void AEnemySpawner::HandleSpawnedEnemyDestroyed(AActor* DestroyedActor)
{
	DecrementAliveCountForDestroyedEnemy(DestroyedActor);

	SpawnedEnemies.RemoveAll([DestroyedActor](const TWeakObjectPtr<AEnemyBase>& EnemyPtr)
	{
		return !EnemyPtr.IsValid() || EnemyPtr.Get() == DestroyedActor;
	});

#if !UE_BUILD_SHIPPING
	StressTestEnemies.RemoveAll([DestroyedActor](const TWeakObjectPtr<AEnemyBase>& EnemyPtr)
	{
		return !EnemyPtr.IsValid() || EnemyPtr.Get() == DestroyedActor;
	});
#endif
}

#if !UE_BUILD_SHIPPING
TSubclassOf<AEnemyBase> AEnemySpawner::GetStressTestEnemyClass() const
{
	return StressTestEnemyClass;
}

bool AEnemySpawner::IsStressTestEnemyClassConfigured() const
{
	return StressTestEnemyClass != nullptr;
}

AEnemyBase* AEnemySpawner::SpawnStressEnemy()
{
	TSubclassOf<AEnemyBase> EnemyClass = GetStressTestEnemyClass();
	if (!EnemyClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy stress spawn failed: no StressTestEnemyClass and no EnemySpawnEntries class available."));
		return nullptr;
	}

	ACharacterBase* ActivePlayer = GetActivePlayerCharacter();
	if (!ActivePlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy stress spawn failed: active player invalid."));
		return nullptr;
	}

	FVector SpawnLocation;
	if (!FindSpawnLocation(ActivePlayer->GetActorLocation(), EnemyClass, SpawnLocation))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	AEnemyBase* SpawnedEnemy = GetWorld()->SpawnActor<AEnemyBase>(EnemyClass, SpawnLocation, FRotator::ZeroRotator, SpawnParameters);
	if (!SpawnedEnemy)
	{
		return nullptr;
	}

	const float SpawnAdjustmentDeltaZ = SpawnedEnemy->GetActorLocation().Z - SpawnLocation.Z;
	if (FMath::Abs(SpawnAdjustmentDeltaZ) > 25.0f && CVarDebugEnemySpawnGround.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner stress spawn collision adjustment changed Z for %s RequestedZ=%.2f ActualZ=%.2f DeltaZ=%.2f"),
			*GetNameSafe(SpawnedEnemy),
			SpawnLocation.Z,
			SpawnedEnemy->GetActorLocation().Z,
			SpawnAdjustmentDeltaZ);
	}

	SpawnedEnemy->ConfigureForStressTest(true, true);
	SpawnedEnemy->SpawnDefaultController();
	SpawnedEnemy->OnDestroyed.AddDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDestroyed);
	StressTestEnemies.Add(SpawnedEnemy);
	return SpawnedEnemy;
}

void AEnemySpawner::HandleStressSpawnTimerElapsed()
{
	PruneStressTestEnemies();

	const int32 CurrentCount = GetLivingStressEnemyCount();
	if (CurrentCount >= RequestedStressEnemyCount)
	{
		GetWorldTimerManager().ClearTimer(StressSpawnTimerHandle);
		return;
	}

	const int32 DesiredThisBatch = FMath::Min(FMath::Max(1, StressSpawnBatchSize), RequestedStressEnemyCount - CurrentCount);
	int32 SpawnedThisBatch = 0;
	for (int32 Index = 0; Index < DesiredThisBatch; ++Index)
	{
		if (SpawnStressEnemy())
		{
			++SpawnedThisBatch;
		}
	}

	PruneStressTestEnemies();
	if (GetLivingStressEnemyCount() >= RequestedStressEnemyCount || SpawnedThisBatch <= 0)
	{
		GetWorldTimerManager().ClearTimer(StressSpawnTimerHandle);
	}

}

void AEnemySpawner::PruneStressTestEnemies()
{
	StressTestEnemies.RemoveAll([](const TWeakObjectPtr<AEnemyBase>& EnemyPtr)
	{
		const AEnemyBase* Enemy = EnemyPtr.Get();
		return !Enemy || Enemy->IsDead();
	});
}

int32 AEnemySpawner::GetLivingStressEnemyCount() const
{
	int32 LivingCount = 0;
	for (const TWeakObjectPtr<AEnemyBase>& EnemyPtr : StressTestEnemies)
	{
		const AEnemyBase* Enemy = EnemyPtr.Get();
		if (Enemy && !Enemy->IsDead())
		{
			++LivingCount;
		}
	}

	return LivingCount;
}

void AEnemySpawner::DisablePlayerAutoAttacksForStressTest()
{
	if (bStressAutoAttacksDisabled)
	{
		return;
	}

	const ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	const UCharacterManagerComponent* CharacterManager = SurvivorController ? SurvivorController->GetCharacterManager() : nullptr;
	if (!CharacterManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stress test could not disable player auto-attacks: CharacterManager invalid."));
		return;
	}

	UAutoAttackComponent* FirstAttack = FindAutoAttackComponent(CharacterManager->GetActiveCharacter());
	UAutoAttackComponent* SecondAttack = FindAutoAttackComponent(CharacterManager->GetInactiveCharacter());
	bSavedFirstAutoAttackEnabled = FirstAttack && FirstAttack->IsAutoAttackEnabled();
	bSavedSecondAutoAttackEnabled = SecondAttack && SecondAttack->IsAutoAttackEnabled();

	if (FirstAttack)
	{
		FirstAttack->SetAutoAttackEnabled(false);
	}
	if (SecondAttack)
	{
		SecondAttack->SetAutoAttackEnabled(false);
	}

	bStressAutoAttacksDisabled = true;
}

void AEnemySpawner::RestorePlayerAutoAttacksAfterStressTest()
{
	if (!bStressAutoAttacksDisabled)
	{
		return;
	}

	const ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	const UCharacterManagerComponent* CharacterManager = SurvivorController ? SurvivorController->GetCharacterManager() : nullptr;
	if (CharacterManager)
	{
		if (UAutoAttackComponent* FirstAttack = FindAutoAttackComponent(CharacterManager->GetActiveCharacter()))
		{
			FirstAttack->SetAutoAttackEnabled(bSavedFirstAutoAttackEnabled);
		}

		if (UAutoAttackComponent* SecondAttack = FindAutoAttackComponent(CharacterManager->GetInactiveCharacter()))
		{
			SecondAttack->SetAutoAttackEnabled(bSavedSecondAutoAttackEnabled);
		}
	}

	bStressAutoAttacksDisabled = false;
}

UAutoAttackComponent* AEnemySpawner::FindAutoAttackComponent(ACharacterBase* Character) const
{
	return Character ? Character->FindComponentByClass<UAutoAttackComponent>() : nullptr;
}
#endif
