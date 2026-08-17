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
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "SurvivorPlayerController.h"

static TAutoConsoleVariable<int32> CVarLogSpawnDirector(
	TEXT("hd.LogSpawnDirector"),
	0,
	TEXT("When set to 1, logs periodic survivor spawn director status and spawn batches."));

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

	for (TWeakObjectPtr<AEnemyBase>& EnemyPtr : SpawnedEnemies)
	{
		if (AEnemyBase* Enemy = EnemyPtr.Get())
		{
			Enemy->OnDestroyed.RemoveDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDestroyed);
		}
	}

	SpawnedEnemies.Empty();

	Super::EndPlay(EndPlayReason);
}

void AEnemySpawner::StartSpawning()
{
	if (!bSpawningEnabled || GetCurrentSpawnInterval() <= 0.0f)
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
	if (!bSpawningEnabled)
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

	SpawnedEnemy->SpawnDefaultController();
	SpawnedEnemy->ApplySpawnDifficultyScaling(GetHealthMultiplier(), GetDamageMultiplier());
	SpawnedEnemy->OnDestroyed.AddDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDestroyed);
	SpawnedEnemies.Add(SpawnedEnemy);

	if (bDebugSpawning)
	{
		UE_LOG(LogTemp, Log, TEXT("EnemySpawner spawned %s at %s. Alive=%d Max=%d"),
			*GetNameSafe(SpawnEntry.EnemyClass.Get()),
			*SpawnLocation.ToString(),
			GetAliveEnemyCount(),
			CurrentMaxAliveEnemies);

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
		const bool bTimeAllowed = RunTimeSeconds >= Entry.MinimumRunTime
			&& (Entry.MaximumRunTime <= 0.0f || RunTimeSeconds <= Entry.MaximumRunTime);
		const bool bBudgetAllowed = Entry.SpawnCost <= FMath::Max(1, RemainingBudget);
		if (Entry.bEnabled && Entry.EnemyClass && Entry.SpawnWeight > 0.0f && bTimeAllowed && bBudgetAllowed)
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
		const bool bTimeAllowed = RunTimeSeconds >= Entry.MinimumRunTime
			&& (Entry.MaximumRunTime <= 0.0f || RunTimeSeconds <= Entry.MaximumRunTime);
		const bool bBudgetAllowed = Entry.SpawnCost <= FMath::Max(1, RemainingBudget);
		if (!Entry.bEnabled || !Entry.EnemyClass || Entry.SpawnWeight <= 0.0f || !bTimeAllowed || !bBudgetAllowed)
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
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector TraceStart = CandidateLocation + FVector(0.0f, 0.0f, GroundTraceHeight);
	const FVector TraceEnd = CandidateLocation - FVector(0.0f, 0.0f, GroundTraceDepth);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemySpawnerGroundTrace), false, this);
	const bool bHitGround = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

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
		OutProjectedLocation = HitResult.Location;
		return !bRequireNavMeshProjection;
	}

	FNavLocation NavLocation;
	if (!NavigationSystem->ProjectPointToNavigation(HitResult.Location, NavLocation, NavMeshProjectionExtent))
	{
		OutProjectedLocation = HitResult.Location;
		return !bRequireNavMeshProjection;
	}

	OutProjectedLocation = NavLocation.Location;
	return true;
}

bool AEnemySpawner::IsSpawnLocationInsideArena(const FVector& Location) const
{
	return !ActiveSpawnArea || ActiveSpawnArea->ContainsSpawnLocation(Location, SpawnEdgePadding);
}

bool AEnemySpawner::IsSpawnLocationCollisionFree(const FVector& Location, TSubclassOf<AEnemyBase> EnemyClass) const
{
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
	const int32 CompletedMinutes = FMath::FloorToInt(GetRunTimeMinutes());
	return 1.0f + CompletedMinutes * 0.5f;
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
	if (!bSpawningEnabled)
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

		AEnemyBase* SpawnedEnemy = SpawnEnemy();
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
	if (!bSpawningEnabled)
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
	SpawnedEnemies.RemoveAll([DestroyedActor](const TWeakObjectPtr<AEnemyBase>& EnemyPtr)
	{
		return !EnemyPtr.IsValid() || EnemyPtr.Get() == DestroyedActor;
	});
}
