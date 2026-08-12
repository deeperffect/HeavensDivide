// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemySpawner.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/CapsuleComponent.h"
#include "Curves/CurveFloat.h"
#include "DrawDebugHelpers.h"
#include "EnemyBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
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

AEnemyBase* AEnemySpawner::SpawnEnemy()
{
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
	if (!FindSpawnLocation(ActivePlayer->GetActorLocation(), SpawnLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner %s could not find a valid spawn location."), *GetNameSafe(this));
		return nullptr;
	}

	if (const AEnemyBase* EnemyDefaultObject = SpawnEntry.EnemyClass->GetDefaultObject<AEnemyBase>())
	{
		const UCapsuleComponent* CapsuleComponent = EnemyDefaultObject->GetCapsuleComponent();
		if (CapsuleComponent)
		{
			SpawnLocation.Z += CapsuleComponent->GetScaledCapsuleHalfHeight();
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

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

bool AEnemySpawner::FindSpawnLocation(const FVector& ActivePlayerLocation, FVector& OutSpawnLocation) const
{
	const float SafeMaxRadius = FMath::Max(MinSpawnDistance, MaxSpawnDistance);
	const float SafeMinRadius = FMath::Min(MinSpawnDistance, SafeMaxRadius);
	constexpr int32 MaxAttempts = 6;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const float AngleRadians = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Distance = FMath::FRandRange(SafeMinRadius, SafeMaxRadius);
		const FVector SpawnOffset(FMath::Cos(AngleRadians) * Distance, FMath::Sin(AngleRadians) * Distance, 0.0f);
		const FVector CandidateLocation = ActivePlayerLocation + SpawnOffset;

		const FVector TraceStart = CandidateLocation + FVector(0.0f, 0.0f, GroundTraceHeight);
		const FVector TraceEnd = CandidateLocation - FVector(0.0f, 0.0f, GroundTraceDepth);

		FHitResult HitResult;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemySpawnerGroundTrace), false, this);
		const bool bHitGround = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

		if (bDebugSpawning)
		{
			DrawDebugLine(GetWorld(), TraceStart, TraceEnd, bHitGround ? FColor::Green : FColor::Red, false, 2.0f, 0, 1.5f);
		}

		if (bHitGround)
		{
			OutSpawnLocation = HitResult.Location;
			return true;
		}
	}

	return false;
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
	const float Minutes = GetRunTimeMinutes();
	return 1.0f + Minutes * 0.5f;
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
	int32 DespawnedCount = 0;
	for (TWeakObjectPtr<AEnemyBase>& EnemyPtr : SpawnedEnemies)
	{
		AEnemyBase* Enemy = EnemyPtr.Get();
		if (!Enemy || Enemy->IsDead())
		{
			continue;
		}

		if (FVector::DistSquared2D(Enemy->GetActorLocation(), ActivePlayer->GetActorLocation()) > MaxDistanceSquared)
		{
			Enemy->Destroy();
			++DespawnedCount;
			if (DespawnedCount >= MaxDistantDespawnsPerCheck)
			{
				break;
			}
		}
	}

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
