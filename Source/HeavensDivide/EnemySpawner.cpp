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

static TAutoConsoleVariable<int32> CVarDebugPressureEvents(
	TEXT("hd.DebugPressureEvents"), 0,
	TEXT("Development only. Logs pressure-event starts/completions and draws their shared direction and arc."));

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarDebugSpatialPressure(
	TEXT("hd.DebugSpatialPressure"), 0,
	TEXT("Draws movement, sectors, stale Grunts, and replacement choices for spatial pressure."));
static TAutoConsoleVariable<int32> CVarLogSpatialPressure(
	TEXT("hd.LogSpatialPressurePasses"), 0,
	TEXT("Logs every automatic spatial-pressure evaluation when set to 1."));
#endif

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

static void LogSpawnDirectorCommand(const TArray<FString>& Args, UWorld* World)
{
	if (AEnemySpawner* Spawner = FindEnemySpawnerForDebugCommand(World))
	{
		Spawner->LogPressureDirectorStatus();
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("hd.LogPressureDirector failed: no AEnemySpawner found in world."));
}

static void TriggerPressureEventCommand(const TArray<FString>& Args, UWorld* World)
{
	AEnemySpawner* Spawner = FindEnemySpawnerForDebugCommand(World);
	if (!Spawner || Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: hd.TriggerPressureEvent <EventName>"));
		return;
	}
	Spawner->TriggerPressureEventByName(FName(*Args[0]));
}

static void LogSpatialPressureCommand(const TArray<FString>& Args, UWorld* World)
{
	if (AEnemySpawner* Spawner = FindEnemySpawnerForDebugCommand(World)) { Spawner->LogSpatialPressureStatus(); return; }
	UE_LOG(LogTemp, Warning, TEXT("hd.LogSpatialPressure failed: no AEnemySpawner found in world."));
}

static void ForceSpatialPressureCommand(const TArray<FString>& Args, UWorld* World)
{
	if (AEnemySpawner* Spawner = FindEnemySpawnerForDebugCommand(World)) { Spawner->ForceSpatialPressurePass(); return; }
	UE_LOG(LogTemp, Warning, TEXT("hd.ForceSpatialPressurePass failed: no AEnemySpawner found in world."));
}

static FAutoConsoleCommandWithWorldAndArgs GStressEnemiesCommand(
	TEXT("hd.StressEnemies"),
	TEXT("Development only. Sets the desired living stress-test enemy count. Usage: hd.StressEnemies 100"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StressEnemiesCommand));

static FAutoConsoleCommandWithWorldAndArgs GClearStressEnemiesCommand(
	TEXT("hd.ClearStressEnemies"),
	TEXT("Development only. Destroys only enemies created by hd.StressEnemies."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ClearStressEnemiesCommand));

static FAutoConsoleCommandWithWorldAndArgs GLogSpawnDirectorCommand(
	TEXT("hd.LogPressureDirector"),
	TEXT("Development only. Logs the active authored phase and per-enemy population deficits."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LogSpawnDirectorCommand));

static FAutoConsoleCommandWithWorldAndArgs GTriggerPressureEventCommand(
	TEXT("hd.TriggerPressureEvent"),
	TEXT("Development only. Forces a configured event while preserving class cooldowns, caps, and spawn validation."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TriggerPressureEventCommand));

static FAutoConsoleCommandWithWorldAndArgs GLogSpatialPressureCommand(
	TEXT("hd.LogSpatialPressure"), TEXT("Development only. Logs a dry-run spatial-pressure evaluation."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LogSpatialPressureCommand));
static FAutoConsoleCommandWithWorldAndArgs GForceSpatialPressureCommand(
	TEXT("hd.ForceSpatialPressurePass"), TEXT("Development only. Immediately runs one spatial-pressure recycle evaluation."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ForceSpatialPressureCommand));
#endif

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();
	NextEligibleSpawnTimeByClass.Empty();
	NextEligibleEventTimeByName.Empty();
	NextGlobalEventTime = 0.0f;
	ActiveEventIndex = INDEX_NONE;
	ActiveEventMembers.Empty();
	DirectionalBiasEndRunTime = 0.0f;

	if (RunTimeUpdateInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(RunTimeTimerHandle, this, &AEnemySpawner::HandleRunTimeTimerElapsed, RunTimeUpdateInterval, true);
	}

	if (DistantEnemyCheckInterval > 0.0f && MaxEnemyDistanceFromPlayer > 0.0f)
	{
		GetWorldTimerManager().SetTimer(DistantEnemyCheckTimerHandle, this, &AEnemySpawner::HandleDistantEnemyCheckTimerElapsed, DistantEnemyCheckInterval, true);
	}
	if (bEnableSpatialPressureRecycling && SpatialPressureEvaluationInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(SpatialPressureTimerHandle, this, &AEnemySpawner::HandleSpatialPressureTimerElapsed, SpatialPressureEvaluationInterval, true);
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
	GetWorldTimerManager().ClearTimer(SpatialPressureTimerHandle);
	GetWorldTimerManager().ClearTimer(EventMemberTimerHandle);
#if !UE_BUILD_SHIPPING
	GetWorldTimerManager().ClearTimer(StressSpawnTimerHandle);
#endif

	for (TWeakObjectPtr<AEnemyBase>& EnemyPtr : SpawnedEnemies)
	{
		if (AEnemyBase* Enemy = EnemyPtr.Get())
		{
			Enemy->OnDestroyed.RemoveDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDestroyed);
			Enemy->OnEnemyDied.RemoveDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDied);
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
	EnemySpawnRunTimeByEnemy.Empty();
	EventRecycleProtectionEndTimeByEnemy.Empty();
	NextEligibleSpawnTimeByClass.Empty();
	NextEligibleEventTimeByName.Empty();
	ActiveEventMembers.Empty();
	SpawnPressureModifiers.Empty();
	EnemySpawnModifierContexts.Empty();

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

	const FEnemyPressurePhase* Phase = ResolveActivePressurePhase();
	const FEnemyPopulationPhaseEntry* PopulationEntry = Phase ? ChooseDirectorEntry(*Phase) : nullptr;
	const FEnemySpawnEntry* SpawnEntry = PopulationEntry ? FindSpawnDefinition(PopulationEntry->EnemyClass) : nullptr;
	if (!Phase || !PopulationEntry || !SpawnEntry)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner %s has no positive population deficit to spawn."), *GetNameSafe(this));
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

void AEnemySpawner::FreezeRunTime()
{
	if (bRunTimeFrozen)
	{
		return;
	}

	bRunTimeFrozen = true;
	GetWorldTimerManager().ClearTimer(RunTimeTimerHandle);
	GetWorldTimerManager().ClearTimer(SpatialPressureTimerHandle);
	GetWorldTimerManager().ClearTimer(EventMemberTimerHandle);
	ActiveEventIndex = INDEX_NONE;
	ActiveEventMembers.Empty();
}

void AEnemySpawner::SetTrialSuspended(bool bSuspended)
{
	if (bTrialSuspended == bSuspended)
	{
		return;
	}

	bTrialSuspended = bSuspended;
	if (bTrialSuspended)
	{
		GetWorldTimerManager().PauseTimer(RunTimeTimerHandle);
		GetWorldTimerManager().PauseTimer(SpawnTimerHandle);
		GetWorldTimerManager().PauseTimer(DistantEnemyCheckTimerHandle);
		GetWorldTimerManager().PauseTimer(SpatialPressureTimerHandle);
		GetWorldTimerManager().PauseTimer(EventMemberTimerHandle);
	}
	else
	{
		if (!bRunTimeFrozen)
		{
			GetWorldTimerManager().UnPauseTimer(RunTimeTimerHandle);
		}
		GetWorldTimerManager().UnPauseTimer(SpawnTimerHandle);
		GetWorldTimerManager().UnPauseTimer(DistantEnemyCheckTimerHandle);
		GetWorldTimerManager().UnPauseTimer(SpatialPressureTimerHandle);
		GetWorldTimerManager().UnPauseTimer(EventMemberTimerHandle);
	}

	PruneTrackedEnemies();
	for (const TWeakObjectPtr<AEnemyBase>& EnemyPtr : SpawnedEnemies)
	{
		if (AEnemyBase* Enemy = EnemyPtr.Get())
		{
			Enemy->SetGameplaySuspended(bTrialSuspended);
		}
	}
}

void AEnemySpawner::SetSpawnPressureModifier(FName ModifierId, float Multiplier)
{
	if (ModifierId.IsNone())
	{
		return;
	}

	SpawnPressureModifiers.FindOrAdd(ModifierId) = FMath::Max(0.0f, Multiplier);
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	RescheduleSpawnTimer();
}

void AEnemySpawner::RemoveSpawnPressureModifier(FName ModifierId)
{
	if (!ModifierId.IsNone() && SpawnPressureModifiers.Remove(ModifierId) > 0)
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		RescheduleSpawnTimer();
	}
}

float AEnemySpawner::GetSpawnPressureModifierProduct() const
{
	float Product = 1.0f;
	for (const TPair<FName, float>& Pair : SpawnPressureModifiers)
	{
		Product *= FMath::Max(0.0f, Pair.Value);
	}
	return Product;
}

float AEnemySpawner::GetEffectiveSpawnPressure() const
{
	return GetSpawnPressure() * GetSpawnPressureModifierProduct();
}

void AEnemySpawner::SetEnemySpawnModifierContext(FName ModifierId, const FEnemySpawnModifierContext& ModifierContext)
{
	if (!ModifierId.IsNone())
	{
		EnemySpawnModifierContexts.FindOrAdd(ModifierId) = ModifierContext;
	}
}

void AEnemySpawner::RemoveEnemySpawnModifierContext(FName ModifierId)
{
	if (!ModifierId.IsNone())
	{
		EnemySpawnModifierContexts.Remove(ModifierId);
	}
}

int32 AEnemySpawner::ConvertRandomAliveEnemiesToBloodbound(const FEnemySpawnModifierContext& BloodboundContext, float ConversionPercent)
{
	const float SafeConversionPercent = FMath::IsFinite(ConversionPercent)
		? FMath::Clamp(ConversionPercent, 0.0f, 1.0f)
		: 0.0f;

	PruneTrackedEnemies();
	TArray<AEnemyBase*> EligibleEnemies;
	EligibleEnemies.Reserve(SpawnedEnemies.Num());
	for (const TWeakObjectPtr<AEnemyBase>& EnemyPtr : SpawnedEnemies)
	{
		AEnemyBase* Enemy = EnemyPtr.Get();
		if (!IsValid(Enemy)
			|| Enemy->IsActorBeingDestroyed()
			|| !CountedEnemyClassByEnemy.Contains(TObjectKey<AEnemyBase>(Enemy))
			|| Enemy->IsDead()
			|| Enemy->IsBloodbound())
		{
			continue;
		}

		EligibleEnemies.Add(Enemy);
	}

	const int32 ConversionCount = FMath::Clamp(
		FMath::RoundToInt(static_cast<float>(EligibleEnemies.Num()) * SafeConversionPercent),
		0,
		EligibleEnemies.Num());
	for (int32 Index = 0; Index < ConversionCount; ++Index)
	{
		const int32 SelectedIndex = FMath::RandRange(Index, EligibleEnemies.Num() - 1);
		EligibleEnemies.Swap(Index, SelectedIndex);
		AEnemyBase* SelectedEnemy = EligibleEnemies[Index];
		SelectedEnemy->MakeBloodbound(
			BloodboundContext.HealthMultiplier,
			BloodboundContext.DamageMultiplier,
			BloodboundContext.MovementSpeedMultiplier,
			BloodboundContext.bDropsXP);
		if (SelectedEnemy->IsBloodbound())
		{
			OnEnemyBecameBloodbound.Broadcast(SelectedEnemy);
		}
	}

	return ConversionCount;
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

	if (!IsSpawnDefinitionUnlocked(SpawnEntry))
	{
		return nullptr;
	}

	const FEnemyPressurePhase* ActivePhase = ResolveActivePressurePhase();
	const FEnemyPopulationPhaseEntry* ActivePopulationEntry = nullptr;
	if (ActivePhase)
	{
		ActivePopulationEntry = ActivePhase->EnemyPopulationEntries.FindByPredicate([&SpawnEntry](const FEnemyPopulationPhaseEntry& Entry)
		{
			return Entry.EnemyClass == SpawnEntry.EnemyClass;
		});
	}
	const bool bActiveEntryEligible = ActivePhase && ActivePopulationEntry
		&& (SpawnEntry.PressureSpawnMode == EEnemyPressureSpawnMode::TimedThreat
			? IsTimedThreatEntryEligible(*ActivePhase, *ActivePopulationEntry)
			: IsPopulationEntryEligible(*ActivePhase, *ActivePopulationEntry));
	if (!bActiveEntryEligible)
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
	const bool bUseBias = SpawnEntry.PressureSpawnMode == EEnemyPressureSpawnMode::MaintainPopulation
		&& bDirectionalBiasEnabled && RunTimeSeconds < DirectionalBiasEndRunTime
		&& FMath::FRand() < FMath::Clamp(DirectionalBiasChance, 0.0f, 1.0f);
	if (!FindSpawnLocation(ActivePlayer->GetActorLocation(), SpawnEntry.EnemyClass, SpawnLocation,
		bUseBias, DirectionalBiasAngleDegrees, DirectionalBiasArcDegrees, MinSpawnDistance, MaxSpawnDistance))
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
	SpawnedEnemy->ApplySpawnDifficultyScaling(GetHealthMultiplier(SpawnEntry), GetDamageMultiplier());
	ApplyEnemySpawnModifierContexts(SpawnedEnemy);
	if (SpawnedEnemy->IsBloodbound())
	{
		OnEnemyBecameBloodbound.Broadcast(SpawnedEnemy);
	}
	SpawnedEnemy->OnDestroyed.AddDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDestroyed);
	SpawnedEnemy->OnEnemyDied.AddUniqueDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDied);
	SpawnedEnemies.Add(SpawnedEnemy);
	IncrementAliveCountForSpawnedEnemy(SpawnedEnemy, SpawnEntry.EnemyClass);
	TrackEnemySpawnMetadata(SpawnedEnemy, false);

	if (bDebugSpawning)
	{
		const int32 AliveOfType = GetAliveCountForSpawnClass(SpawnEntry.EnemyClass);
		UE_LOG(LogTemp, Log, TEXT("EnemySpawner spawned %s at %s. Alive=%d Max=%d AliveOfType=%d/%d"),
			*GetNameSafe(SpawnEntry.EnemyClass.Get()),
			*SpawnLocation.ToString(),
			GetAliveEnemyCount(),
			CurrentMaxAliveEnemies,
			AliveOfType,
			0);

		DrawDebugSphere(GetWorld(), SpawnLocation, 40.0f, 16, FColor::Red, false, 2.0f, 0, 2.0f);
	}

	return SpawnedEnemy;
}

AEnemyBase* AEnemySpawner::SpawnEventEnemy(TSubclassOf<AEnemyBase> EnemyClass, int32 ClassOverflowAllowance)
{
	if (!EnemyClass || !PressureEvents.IsValidIndex(ActiveEventIndex) || GetAliveEnemyCount() >= ActiveEventOverflowCap)
	{
		return nullptr;
	}

	const FEnemyPressureEventDefinition& Event = PressureEvents[ActiveEventIndex];
	const FEnemyPressurePhase* Phase = ResolveActivePressurePhase();
	const FEnemySpawnEntry* Definition = FindSpawnDefinition(EnemyClass);
	const FEnemyPopulationPhaseEntry* Population = Phase ? Phase->EnemyPopulationEntries.FindByPredicate([EnemyClass](const FEnemyPopulationPhaseEntry& Entry)
	{
		return Entry.EnemyClass == EnemyClass;
	}) : nullptr;
	if (!Phase || !Definition || !Population || !IsSpawnDefinitionUnlocked(*Definition))
	{
		return nullptr;
	}
	if (!Event.bIgnoreThreatDeathCooldown && IsClassCooldownActive(EnemyClass))
	{
		return nullptr;
	}
	const int32 AllowedClassMax = Population->MaxPopulation + (Event.bAllowTemporaryPopulationOverflow ? FMath::Max(0, ClassOverflowAllowance) : 0);
	if (AllowedClassMax <= 0 || GetAliveCountForSpawnClass(EnemyClass) >= AllowedClassMax)
	{
		return nullptr;
	}

	ACharacterBase* ActivePlayer = GetActivePlayerCharacter();
	FVector SpawnLocation;
	if (!ActivePlayer || !FindSpawnLocation(ActivePlayer->GetActorLocation(), EnemyClass, SpawnLocation, true,
		ActiveEventDirectionAngle, Event.SpawnArcDegrees, Event.SpawnDistanceMin, Event.SpawnDistanceMax))
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	AEnemyBase* Enemy = GetWorld()->SpawnActor<AEnemyBase>(EnemyClass, SpawnLocation, FRotator::ZeroRotator, Parameters);
	if (!Enemy)
	{
		return nullptr;
	}
	Enemy->SpawnDefaultController();
	Enemy->ApplySpawnDifficultyScaling(GetHealthMultiplier(*Definition), GetDamageMultiplier());
	ApplyEnemySpawnModifierContexts(Enemy);
	if (Enemy->IsBloodbound())
	{
		OnEnemyBecameBloodbound.Broadcast(Enemy);
	}
	Enemy->OnDestroyed.AddDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDestroyed);
	Enemy->OnEnemyDied.AddUniqueDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDied);
	SpawnedEnemies.Add(Enemy);
	IncrementAliveCountForSpawnedEnemy(Enemy, EnemyClass);
	TrackEnemySpawnMetadata(Enemy, true);
	return Enemy;
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

const FEnemyPressurePhase* AEnemySpawner::ResolveActivePressurePhase(int32* OutPhaseIndex, bool bLogWarnings) const
{
	if (OutPhaseIndex)
	{
		*OutPhaseIndex = INDEX_NONE;
	}

	const FEnemyPressurePhase* ResolvedPhase = nullptr;
	int32 ResolvedIndex = INDEX_NONE;
	int32 MatchCount = 0;
	for (int32 Index = 0; Index < PressurePhases.Num(); ++Index)
	{
		const FEnemyPressurePhase& Phase = PressurePhases[Index];
		const bool bValidRange = Phase.StartTimeSeconds >= 0.0f
			&& (Phase.EndTimeSeconds <= 0.0f || Phase.EndTimeSeconds > Phase.StartTimeSeconds);
		const bool bContainsTime = bValidRange
			&& RunTimeSeconds >= Phase.StartTimeSeconds
			&& (Phase.EndTimeSeconds <= 0.0f || RunTimeSeconds < Phase.EndTimeSeconds);
		if (bContainsTime)
		{
			++MatchCount;
			if (!ResolvedPhase)
			{
				ResolvedPhase = &Phase;
				ResolvedIndex = Index;
			}
		}
	}

	if (MatchCount == 1)
	{
		if (OutPhaseIndex)
		{
			*OutPhaseIndex = ResolvedIndex;
		}
		return ResolvedPhase;
	}

	const int32 CurrentSecond = FMath::FloorToInt(RunTimeSeconds);
	if (bLogWarnings && LastInvalidPhaseWarningSecond != CurrentSecond)
	{
		LastInvalidPhaseWarningSecond = CurrentSecond;
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner %s found %d pressure phases at RunTime=%.2f. Expected exactly one; normal spawning is paused safely."),
			*GetNameSafe(this), MatchCount, RunTimeSeconds);
	}
	return nullptr;
}

const FEnemySpawnEntry* AEnemySpawner::FindSpawnDefinition(TSubclassOf<AEnemyBase> EnemyClass) const
{
	for (const FEnemySpawnEntry& Entry : EnemySpawnEntries)
	{
		if (Entry.EnemyClass == EnemyClass)
		{
			return &Entry;
		}
	}
	return nullptr;
}

const FEnemyPopulationPhaseEntry* AEnemySpawner::ChoosePopulationDeficitEntry(const FEnemyPressurePhase& Phase) const
{
	float TotalPriority = 0.0f;
	for (const FEnemyPopulationPhaseEntry& Entry : Phase.EnemyPopulationEntries)
	{
		if (IsPopulationEntryEligible(Phase, Entry))
		{
			const int32 Deficit = Entry.DesiredPopulation - GetAliveCountForSpawnClass(Entry.EnemyClass);
			TotalPriority += static_cast<float>(Deficit) * Entry.RefillPriority;
		}
	}

	if (TotalPriority <= 0.0f)
	{
		return nullptr;
	}

	float Roll = FMath::FRandRange(0.0f, TotalPriority);
	for (const FEnemyPopulationPhaseEntry& Entry : Phase.EnemyPopulationEntries)
	{
		if (!IsPopulationEntryEligible(Phase, Entry))
		{
			continue;
		}
		Roll -= static_cast<float>(Entry.DesiredPopulation - GetAliveCountForSpawnClass(Entry.EnemyClass)) * Entry.RefillPriority;
		if (Roll <= 0.0f)
		{
			return &Entry;
		}
	}
	return nullptr;
}

const FEnemyPopulationPhaseEntry* AEnemySpawner::ChooseTimedThreatEntry(const FEnemyPressurePhase& Phase) const
{
	float TotalPriority = 0.0f;
	for (const FEnemyPopulationPhaseEntry& Entry : Phase.EnemyPopulationEntries)
	{
		if (IsTimedThreatEntryEligible(Phase, Entry))
		{
			TotalPriority += FMath::Max(0.0f, Entry.RefillPriority);
		}
	}

	if (TotalPriority <= 0.0f)
	{
		return nullptr;
	}

	float Roll = FMath::FRandRange(0.0f, TotalPriority);
	for (const FEnemyPopulationPhaseEntry& Entry : Phase.EnemyPopulationEntries)
	{
		if (!IsTimedThreatEntryEligible(Phase, Entry))
		{
			continue;
		}
		Roll -= FMath::Max(0.0f, Entry.RefillPriority);
		if (Roll <= 0.0f)
		{
			return &Entry;
		}
	}
	return nullptr;
}

const FEnemyPopulationPhaseEntry* AEnemySpawner::ChooseDirectorEntry(const FEnemyPressurePhase& Phase) const
{
	const FEnemyPopulationPhaseEntry* MaintainedEntry = ChoosePopulationDeficitEntry(Phase);
	const FEnemyPopulationPhaseEntry* ThreatEntry = ChooseTimedThreatEntry(Phase);
	if (ThreatEntry && (!MaintainedEntry || FMath::FRand() < FMath::Clamp(TimedThreatSpawnSlotChance, 0.0f, 1.0f)))
	{
		return ThreatEntry;
	}
	return MaintainedEntry;
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

float AEnemySpawner::GetHealthMultiplier(const FEnemySpawnEntry& SpawnEntry) const
{
	return 1.0f + FMath::Max(0.0f, SpawnEntry.HealthScalingPerMinute) * FMath::Max(0.0f, GetRunTimeMinutes());
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
	const FEnemyPressurePhase* Phase = ResolveActivePressurePhase(nullptr, false);
	if (!Phase || GetTotalDesiredPopulation(*Phase) <= 0)
	{
		return 0.0f;
	}

	const float Ratio = GetPopulationRatio(*Phase);
	const float PhaseInterval = Ratio < 0.5f
		? Phase->EmergencySpawnInterval
		: (Ratio < 0.8f ? Phase->AcceleratedSpawnInterval : Phase->NormalSpawnInterval);
	return FMath::Max(0.01f, PhaseInterval / FMath::Max(0.01f, GetSpawnPressureModifierProduct()));
}

int32 AEnemySpawner::GetCurrentEnemiesPerSpawn() const
{
	const FEnemyPressurePhase* Phase = ResolveActivePressurePhase(nullptr, false);
	return Phase ? GetCurrentPopulationBatchSize(*Phase) : 0;
}

int32 AEnemySpawner::GetCurrentMaxAliveEnemies() const
{
	const FEnemyPressurePhase* Phase = ResolveActivePressurePhase(nullptr, false);
	return Phase ? FMath::Clamp(Phase->GlobalMaxAlive, 0, FMath::Max(1, AbsoluteHardAliveCap)) : 0;
}

int32 AEnemySpawner::GetCurrentSpawnBudget() const
{
	const float Pressure = GetEffectiveSpawnPressure();
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

bool AEnemySpawner::IsSpawnDefinitionUnlocked(const FEnemySpawnEntry& Entry) const
{
	const bool bTimeAllowed = RunTimeSeconds >= Entry.MinimumRunTime
		&& (Entry.MaximumRunTime <= 0.0f || RunTimeSeconds < Entry.MaximumRunTime);
	return Entry.bEnabled && Entry.EnemyClass && bTimeAllowed;
}

bool AEnemySpawner::IsClassCooldownActive(TSubclassOf<AEnemyBase> EnemyClass, float* OutRemainingSeconds) const
{
	const float* NextEligibleTime = EnemyClass ? NextEligibleSpawnTimeByClass.Find(EnemyClass.Get()) : nullptr;
	const float Remaining = NextEligibleTime ? FMath::Max(0.0f, *NextEligibleTime - RunTimeSeconds) : 0.0f;
	if (OutRemainingSeconds)
	{
		*OutRemainingSeconds = Remaining;
	}
	return Remaining > 0.0f;
}

bool AEnemySpawner::IsPopulationEntryEligible(const FEnemyPressurePhase& Phase, const FEnemyPopulationPhaseEntry& PopulationEntry) const
{
	if (!PopulationEntry.EnemyClass || PopulationEntry.DesiredPopulation <= 0
		|| PopulationEntry.MaxPopulation <= 0 || PopulationEntry.RefillPriority <= 0.0f)
	{
		return false;
	}

	const FEnemySpawnEntry* SpawnDefinition = FindSpawnDefinition(PopulationEntry.EnemyClass);
	if (!SpawnDefinition || SpawnDefinition->PressureSpawnMode != EEnemyPressureSpawnMode::MaintainPopulation
		|| !IsSpawnDefinitionUnlocked(*SpawnDefinition) || IsClassCooldownActive(PopulationEntry.EnemyClass))
	{
		return false;
	}

	const int32 AliveOfType = GetAliveCountForSpawnClass(PopulationEntry.EnemyClass);
	return AliveOfType < PopulationEntry.DesiredPopulation && AliveOfType < PopulationEntry.MaxPopulation;
}

bool AEnemySpawner::IsTimedThreatEntryEligible(const FEnemyPressurePhase& Phase, const FEnemyPopulationPhaseEntry& PopulationEntry) const
{
	if (!PopulationEntry.EnemyClass || PopulationEntry.MaxPopulation <= 0 || PopulationEntry.RefillPriority <= 0.0f)
	{
		return false;
	}

	const FEnemySpawnEntry* SpawnDefinition = FindSpawnDefinition(PopulationEntry.EnemyClass);
	if (!SpawnDefinition || SpawnDefinition->PressureSpawnMode != EEnemyPressureSpawnMode::TimedThreat
		|| !IsSpawnDefinitionUnlocked(*SpawnDefinition) || IsClassCooldownActive(PopulationEntry.EnemyClass))
	{
		return false;
	}

	return GetAliveCountForSpawnClass(PopulationEntry.EnemyClass) < PopulationEntry.MaxPopulation;
}

int32 AEnemySpawner::GetTotalDesiredPopulation(const FEnemyPressurePhase& Phase) const
{
	int32 TotalDesired = 0;
	for (const FEnemyPopulationPhaseEntry& Entry : Phase.EnemyPopulationEntries)
	{
		const FEnemySpawnEntry* SpawnDefinition = FindSpawnDefinition(Entry.EnemyClass);
		if (Entry.EnemyClass && Entry.DesiredPopulation > 0 && Entry.MaxPopulation > 0
			&& SpawnDefinition && SpawnDefinition->PressureSpawnMode == EEnemyPressureSpawnMode::MaintainPopulation
			&& IsSpawnDefinitionUnlocked(*SpawnDefinition))
		{
			TotalDesired += FMath::Min(Entry.DesiredPopulation, Entry.MaxPopulation);
		}
	}
	return TotalDesired;
}

float AEnemySpawner::GetPopulationRatio(const FEnemyPressurePhase& Phase) const
{
	const int32 TotalDesired = GetTotalDesiredPopulation(Phase);
	if (TotalDesired <= 0)
	{
		return 1.0f;
	}
	int32 LivingCount = 0;
	for (const FEnemyPopulationPhaseEntry& Entry : Phase.EnemyPopulationEntries)
	{
		const FEnemySpawnEntry* SpawnDefinition = FindSpawnDefinition(Entry.EnemyClass);
		if (SpawnDefinition && SpawnDefinition->PressureSpawnMode == EEnemyPressureSpawnMode::MaintainPopulation
			&& IsSpawnDefinitionUnlocked(*SpawnDefinition))
		{
			LivingCount += GetAliveCountForSpawnClass(Entry.EnemyClass);
		}
	}
	return static_cast<float>(LivingCount) / static_cast<float>(TotalDesired);
}

int32 AEnemySpawner::GetCurrentPopulationBatchSize(const FEnemyPressurePhase& Phase) const
{
	const float Ratio = GetPopulationRatio(Phase);
	return FMath::Max(1, Ratio < 0.5f ? EmergencyMaxBatchSize : (Ratio < 0.8f ? AcceleratedMaxBatchSize : NormalMaxBatchSize));
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

bool AEnemySpawner::UnregisterLivingEnemy(AEnemyBase* Enemy)
{
	if (!Enemy)
	{
		return false;
	}

	UClass* CountedClass = nullptr;
	if (!CountedEnemyClassByEnemy.RemoveAndCopyValue(TObjectKey<AEnemyBase>(Enemy), CountedClass) || !CountedClass)
	{
		return false;
	}
	EnemySpawnRunTimeByEnemy.Remove(TObjectKey<AEnemyBase>(Enemy));
	EventRecycleProtectionEndTimeByEnemy.Remove(TObjectKey<AEnemyBase>(Enemy));

	int32* AliveCount = AliveEnemyCountByClass.Find(CountedClass);
	if (!AliveCount)
	{
		return true;
	}

	*AliveCount = FMath::Max(0, *AliveCount - 1);
	if (*AliveCount <= 0)
	{
		AliveEnemyCountByClass.Remove(CountedClass);
	}
	return true;
}

void AEnemySpawner::PruneTrackedEnemies()
{
	for (auto It = CountedEnemyClassByEnemy.CreateIterator(); It; ++It)
	{
		AEnemyBase* Enemy = It.Key().ResolveObjectPtr();
		if (!IsValid(Enemy) || Enemy->IsDead() || Enemy->IsActorBeingDestroyed())
		{
			UClass* CountedClass = It.Value();
			It.RemoveCurrent();
			if (int32* AliveCount = AliveEnemyCountByClass.Find(CountedClass))
			{
				*AliveCount = FMath::Max(0, *AliveCount - 1);
				if (*AliveCount == 0)
				{
					AliveEnemyCountByClass.Remove(CountedClass);
				}
			}
		}
	}
	SpawnedEnemies.RemoveAll([](const TWeakObjectPtr<AEnemyBase>& EnemyPtr)
	{
		return !EnemyPtr.IsValid();
	});
	for (auto It = EnemySpawnRunTimeByEnemy.CreateIterator(); It; ++It)
	{
		if (!IsValid(It.Key().ResolveObjectPtr())) { EventRecycleProtectionEndTimeByEnemy.Remove(It.Key()); It.RemoveCurrent(); }
	}
}

void AEnemySpawner::TrackEnemySpawnMetadata(AEnemyBase* Enemy, bool bEventMember)
{
	if (!Enemy) return;
	const TObjectKey<AEnemyBase> Key(Enemy);
	EnemySpawnRunTimeByEnemy.FindOrAdd(Key) = RunTimeSeconds;
	if (bEventMember)
	{
		EventRecycleProtectionEndTimeByEnemy.FindOrAdd(Key) = RunTimeSeconds + FMath::Max(0.0f, EventGruntRecycleProtectionSeconds);
	}
}

bool AEnemySpawner::RecycleLivingEnemy(AEnemyBase* Enemy)
{
	if (!Enemy || Enemy->IsDead() || Enemy->IsActorBeingDestroyed() || !UnregisterLivingEnemy(Enemy)) return false;
	Enemy->OnEnemyDied.RemoveDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDied);
	Enemy->OnDestroyed.RemoveDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDestroyed);
	SpawnedEnemies.RemoveAll([Enemy](const TWeakObjectPtr<AEnemyBase>& Ptr) { return !Ptr.IsValid() || Ptr.Get() == Enemy; });
	Enemy->SetActorEnableCollision(false);
	Enemy->SetActorHiddenInGame(true);
	Enemy->Destroy();
	return true;
}

void AEnemySpawner::UpdateDirectionalBias()
{
	if (!bDirectionalBiasEnabled)
	{
		return;
	}
	if (RunTimeSeconds >= DirectionalBiasEndRunTime)
	{
		DirectionalBiasAngleDegrees = FMath::FRandRange(0.0f, 360.0f);
		const float MinDuration = FMath::Min(DirectionalBiasDurationMin, DirectionalBiasDurationMax);
		const float MaxDuration = FMath::Max(DirectionalBiasDurationMin, DirectionalBiasDurationMax);
		DirectionalBiasEndRunTime = RunTimeSeconds + FMath::FRandRange(FMath::Max(0.1f, MinDuration), FMath::Max(0.1f, MaxDuration));
	}
}

bool AEnemySpawner::IsEventSelectable(const FEnemyPressureEventDefinition& Event, const FEnemyPressurePhase& Phase, FString* OutReason, bool bIgnoreEventSchedule) const
{
	auto Fail = [OutReason](const TCHAR* Reason) { if (OutReason) { *OutReason = Reason; } return false; };
	if (!bIgnoreEventSchedule && !Phase.bEventsEnabled) return Fail(TEXT("phase events disabled"));
	if (!Event.bEnabled || Event.EventName.IsNone() || Event.Weight <= 0.0f) return Fail(TEXT("disabled/invalid"));
	if (!bIgnoreEventSchedule && (RunTimeSeconds < FMath::Max(120.0f, Event.MinimumRunTime)
		|| (Event.MaximumRunTime > 0.0f && RunTimeSeconds >= Event.MaximumRunTime))) return Fail(TEXT("outside time window"));
	if (!bIgnoreEventSchedule)
	{
		if (const float* EligibleTime = NextEligibleEventTimeByName.Find(Event.EventName); EligibleTime && RunTimeSeconds < *EligibleTime) return Fail(TEXT("event cooldown"));
	}
	if (Event.EnemyEntries.Num() == 0) return Fail(TEXT("no members"));

	int32 RequestedMembers = 0;
	for (const FEnemyPressureEventEnemyEntry& EventEntry : Event.EnemyEntries)
	{
		const FEnemySpawnEntry* Definition = FindSpawnDefinition(EventEntry.EnemyClass);
		const FEnemyPopulationPhaseEntry* Population = Phase.EnemyPopulationEntries.FindByPredicate([&EventEntry](const FEnemyPopulationPhaseEntry& Entry)
		{
			return Entry.EnemyClass == EventEntry.EnemyClass;
		});
		if (!Definition || !Population || !IsSpawnDefinitionUnlocked(*Definition)) return Fail(TEXT("required class unavailable"));
		if (!Event.bIgnoreThreatDeathCooldown && IsClassCooldownActive(EventEntry.EnemyClass)) return Fail(TEXT("required class cooldown"));
		const int32 Allowance = Event.bAllowTemporaryPopulationOverflow ? FMath::Max(0, EventEntry.ClassOverflowAllowance) : 0;
		if (Population->MaxPopulation <= 0 || GetAliveCountForSpawnClass(EventEntry.EnemyClass) + EventEntry.Count > Population->MaxPopulation + Allowance)
			return Fail(TEXT("class capacity"));
		RequestedMembers += FMath::Max(0, EventEntry.Count);
	}
	const int32 EventCap = FMath::Min(Phase.GlobalMaxAlive + (Event.bAllowTemporaryPopulationOverflow ? FMath::Max(0, Event.EventPopulationOverflowAllowance) : 0), AbsoluteHardAliveCap);
	int32 CurrentLiving = 0;
	for (const TPair<UClass*, int32>& Pair : AliveEnemyCountByClass) CurrentLiving += FMath::Max(0, Pair.Value);
	if (CurrentLiving + RequestedMembers > EventCap) return Fail(TEXT("global/event capacity"));
	if (OutReason) *OutReason = TEXT("selectable");
	return true;
}

void AEnemySpawner::UpdateEventScheduler(const FEnemyPressurePhase* ActivePhase)
{
	if (!ActivePhase || ActiveEventIndex != INDEX_NONE || !ActivePhase->bEventsEnabled || RunTimeSeconds < 120.0f)
	{
		return;
	}
	if (NextGlobalEventTime <= 0.0f)
	{
		const float MinInterval = FMath::Min(ActivePhase->EventIntervalMin, ActivePhase->EventIntervalMax);
		const float MaxInterval = FMath::Max(ActivePhase->EventIntervalMin, ActivePhase->EventIntervalMax);
		NextGlobalEventTime = RunTimeSeconds + FMath::FRandRange(MinInterval, MaxInterval);
		return;
	}
	if (RunTimeSeconds < NextGlobalEventTime)
	{
		return;
	}

	TArray<int32> EligibleIndices;
	float TotalWeight = 0.0f;
	for (int32 Index = 0; Index < PressureEvents.Num(); ++Index)
	{
		if (IsEventSelectable(PressureEvents[Index], *ActivePhase))
		{
			EligibleIndices.Add(Index);
		}
	}
	const bool bHasAlternative = EligibleIndices.ContainsByPredicate([this](int32 Index) { return PressureEvents[Index].EventName != LastCompletedEventName; });
	for (int32 Index : EligibleIndices)
	{
		if (!bHasAlternative || PressureEvents[Index].EventName != LastCompletedEventName) TotalWeight += PressureEvents[Index].Weight;
	}
	if (TotalWeight <= 0.0f)
	{
		NextGlobalEventTime = RunTimeSeconds + 1.0f;
		return;
	}
	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (int32 Index : EligibleIndices)
	{
		if (bHasAlternative && PressureEvents[Index].EventName == LastCompletedEventName) continue;
		Roll -= PressureEvents[Index].Weight;
		if (Roll <= 0.0f) { StartPressureEvent(Index, *ActivePhase); return; }
	}
}

#if !UE_BUILD_SHIPPING
void AEnemySpawner::TriggerPressureEventByName(FName EventName)
{
	if (ActiveEventIndex != INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PressureEvent] FORCE %s rejected: event %s is already active."),
			*EventName.ToString(), *PressureEvents[ActiveEventIndex].EventName.ToString());
		return;
	}
	const int32 EventIndex = PressureEvents.IndexOfByPredicate([EventName](const FEnemyPressureEventDefinition& Event) { return Event.EventName == EventName; });
	const FEnemyPressurePhase* Phase = ResolveActivePressurePhase();
	if (!PressureEvents.IsValidIndex(EventIndex) || !Phase)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PressureEvent] FORCE %s rejected: unknown event or invalid phase."), *EventName.ToString());
		return;
	}
	FString Reason;
	if (!IsEventSelectable(PressureEvents[EventIndex], *Phase, &Reason, true))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PressureEvent] FORCE %s rejected: %s."), *EventName.ToString(), *Reason);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[PressureEvent] FORCE %s accepted; event/global schedule bypassed."), *EventName.ToString());
	StartPressureEvent(EventIndex, *Phase);
}
#endif

void AEnemySpawner::StartPressureEvent(int32 EventIndex, const FEnemyPressurePhase& Phase)
{
	if (!PressureEvents.IsValidIndex(EventIndex)) return;
	const FEnemyPressureEventDefinition& Event = PressureEvents[EventIndex];
	ActiveEventIndex = EventIndex;
	ActiveEventMemberIndex = ActiveEventSpawnedCount = ActiveEventFailedCount = 0;
	ActiveEventDirectionAngle = FMath::FRandRange(0.0f, 360.0f);
	ActiveEventStartRunTime = RunTimeSeconds;
	ActiveEventOverflowCap = FMath::Min(Phase.GlobalMaxAlive + (Event.bAllowTemporaryPopulationOverflow ? FMath::Max(0, Event.EventPopulationOverflowAllowance) : 0), AbsoluteHardAliveCap);
	ActiveEventMembers.Empty();
	for (const FEnemyPressureEventEnemyEntry& Entry : Event.EnemyEntries)
	{
		for (int32 Count = 0; Count < FMath::Max(0, Entry.Count); ++Count) ActiveEventMembers.Add(Entry.EnemyClass);
	}
	if (CVarDebugPressureEvents.GetValueOnGameThread() != 0 || CVarLogSpawnDirector.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[PressureEvent] START %s RunTime=%.1f Direction=%.1f RequestedMembers=%d CurrentAlive=%d EventCap=%d"),
			*Event.EventName.ToString(), RunTimeSeconds, ActiveEventDirectionAngle, ActiveEventMembers.Num(), GetAliveEnemyCount(), ActiveEventOverflowCap);
	}
#if !UE_BUILD_SHIPPING
	if (CVarDebugPressureEvents.GetValueOnGameThread() != 0)
	{
		if (ACharacterBase* Player = GetActivePlayerCharacter())
		{
			const FVector Origin = Player->GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
			for (float Offset : { -Event.SpawnArcDegrees * 0.5f, 0.0f, Event.SpawnArcDegrees * 0.5f })
			{
				const float Radians = FMath::DegreesToRadians(ActiveEventDirectionAngle + Offset);
				DrawDebugLine(GetWorld(), Origin, Origin + FVector(FMath::Cos(Radians), FMath::Sin(Radians), 0.0f) * 1400.0f,
					Offset == 0.0f ? FColor::Red : FColor::Orange, false, 3.0f, 0, 5.0f);
			}
		}
	}
#endif
	EmitNextEventMember();
}

void AEnemySpawner::EmitNextEventMember()
{
	if (!PressureEvents.IsValidIndex(ActiveEventIndex) || !ActiveEventMembers.IsValidIndex(ActiveEventMemberIndex))
	{
		CompleteActiveEvent();
		return;
	}
	const FEnemyPressureEventDefinition& Event = PressureEvents[ActiveEventIndex];
	const TSubclassOf<AEnemyBase> EnemyClass = ActiveEventMembers[ActiveEventMemberIndex++];
	const FEnemyPressureEventEnemyEntry* EventEntry = Event.EnemyEntries.FindByPredicate([EnemyClass](const FEnemyPressureEventEnemyEntry& Entry) { return Entry.EnemyClass == EnemyClass; });
	if (SpawnEventEnemy(EnemyClass, EventEntry ? EventEntry->ClassOverflowAllowance : 0)) ++ActiveEventSpawnedCount; else ++ActiveEventFailedCount;
	if (ActiveEventMemberIndex >= ActiveEventMembers.Num())
	{
		CompleteActiveEvent();
	}
	else
	{
		const float NextDelay = ActiveEventMemberIndex == 1 && Event.DelayAfterFirstMember > 0.0f
			? Event.DelayAfterFirstMember : Event.DelayBetweenMembers;
		GetWorldTimerManager().SetTimer(EventMemberTimerHandle, this, &AEnemySpawner::EmitNextEventMember, FMath::Max(0.01f, NextDelay), false);
	}
}

void AEnemySpawner::CompleteActiveEvent()
{
	GetWorldTimerManager().ClearTimer(EventMemberTimerHandle);
	if (!PressureEvents.IsValidIndex(ActiveEventIndex)) { ActiveEventIndex = INDEX_NONE; ActiveEventMembers.Empty(); return; }
	const FEnemyPressureEventDefinition& Event = PressureEvents[ActiveEventIndex];
	const float MinCooldown = FMath::Min(Event.EventCooldownMin, Event.EventCooldownMax);
	const float MaxCooldown = FMath::Max(Event.EventCooldownMin, Event.EventCooldownMax);
	const float EventCooldown = FMath::FRandRange(MinCooldown, MaxCooldown);
	NextEligibleEventTimeByName.FindOrAdd(Event.EventName) = RunTimeSeconds + EventCooldown;
	LastCompletedEventName = Event.EventName;
	if (const FEnemyPressurePhase* Phase = ResolveActivePressurePhase(nullptr, false))
	{
		NextGlobalEventTime = RunTimeSeconds + FMath::FRandRange(FMath::Min(Phase->EventIntervalMin, Phase->EventIntervalMax), FMath::Max(Phase->EventIntervalMin, Phase->EventIntervalMax));
	}
	if (CVarDebugPressureEvents.GetValueOnGameThread() != 0 || CVarLogSpawnDirector.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[PressureEvent] COMPLETE %s Spawned=%d Failed=%d Duration=%.2f NextGlobalEventIn=%.2f EventCooldown=%.2f"),
			*Event.EventName.ToString(), ActiveEventSpawnedCount, ActiveEventFailedCount, RunTimeSeconds - ActiveEventStartRunTime,
			FMath::Max(0.0f, NextGlobalEventTime - RunTimeSeconds), EventCooldown);
	}
	ActiveEventIndex = INDEX_NONE;
	ActiveEventMembers.Empty();
}

void AEnemySpawner::HandleRunTimeTimerElapsed()
{
	if (bRunTimeFrozen)
	{
		return;
	}

	RunTimeSeconds += RunTimeUpdateInterval;
	UpdateDirectionalBias();
	UpdateEventScheduler(ResolveActivePressurePhase(nullptr, false));
	RescheduleSpawnTimer();
	LogDirectorStatus();
}

void AEnemySpawner::HandleSpawnTimerElapsed()
{
	if (bTrialSuspended || !bSpawningEnabled
#if !UE_BUILD_SHIPPING
		|| bStressPauseNormalSpawning
#endif
		)
	{
		StopSpawning();
		return;
	}

	PruneTrackedEnemies();
	const FEnemyPressurePhase* Phase = ResolveActivePressurePhase();
	if (!Phase || GetTotalDesiredPopulation(*Phase) <= 0)
	{
		RescheduleSpawnTimer();
		return;
	}

	// Event batches are short; pausing routine refill prevents event emission plus
	// Emergency batches from producing an uncontrolled one-frame actor burst.
	if (ActiveEventIndex != INDEX_NONE)
	{
		return;
	}

	const int32 TargetSpawnCount = GetCurrentPopulationBatchSize(*Phase);
	int32 SpawnedCount = 0;

	for (int32 Index = 0; Index < TargetSpawnCount; ++Index)
	{
		if (GetAliveEnemyCount() >= GetCurrentMaxAliveEnemies())
		{
			break;
		}

		const FEnemyPopulationPhaseEntry* PopulationEntry = ChooseDirectorEntry(*Phase);
		const FEnemySpawnEntry* Entry = PopulationEntry ? FindSpawnDefinition(PopulationEntry->EnemyClass) : nullptr;
		if (!PopulationEntry || !Entry)
		{
			break;
		}

		AEnemyBase* SpawnedEnemy = SpawnEnemyFromEntry(*Entry);
		if (!SpawnedEnemy)
		{
			// Each batch slot is independent. A blocked location must not cancel later attempts.
			continue;
		}

		++SpawnedCount;
	}

	if (SpawnedCount > 0 && CVarLogSpawnDirector.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Spawn Director spawned %d enemies."), SpawnedCount);
	}
}

void AEnemySpawner::HandleDistantEnemyCheckTimerElapsed()
{
	if (bTrialSuspended)
	{
		return;
	}

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
	const float CurrentRate = GetWorldTimerManager().GetTimerRate(SpawnTimerHandle);
	if (!GetWorldTimerManager().IsTimerActive(SpawnTimerHandle)
		|| RemainingTime > NewInterval
		|| !FMath::IsNearlyEqual(CurrentRate, NewInterval, 0.001f))
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

	const_cast<AEnemySpawner*>(this)->LogPressureDirectorStatus();
}

int32 AEnemySpawner::ChooseReplacementSector(const TArray<int32>& SectorCounts) const
{
	if (SectorCounts.IsEmpty()) return INDEX_NONE;
	if (!bPreferUnderrepresentedSectors) return FMath::RandRange(0, SectorCounts.Num() - 1);
	const float Randomness = FMath::Clamp(ReplacementSectorRandomness, 0.0f, 1.0f);
	float TotalWeight = 0.0f;
	TArray<float> Weights;
	Weights.Reserve(SectorCounts.Num());
	for (const int32 Count : SectorCounts)
	{
		const float Weight = FMath::Lerp(1.0f / (1.0f + FMath::Max(0, Count)), 1.0f, Randomness);
		Weights.Add(Weight);
		TotalWeight += Weight;
	}
	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (int32 Index = 0; Index < Weights.Num(); ++Index)
	{
		Roll -= Weights[Index];
		if (Roll <= 0.0f) return Index;
	}
	return Weights.Num() - 1;
}

AEnemyBase* AEnemySpawner::SpawnSpatialPressureReplacement(const TArray<int32>& SectorCounts, int32& OutSectorIndex)
{
	OutSectorIndex = INDEX_NONE;
	if (!SpatialPressureGruntClass || GetAliveEnemyCount() >= GetCurrentMaxAliveEnemies()) return nullptr;
	const FEnemyPressurePhase* Phase = ResolveActivePressurePhase();
	const FEnemySpawnEntry* Definition = FindSpawnDefinition(SpatialPressureGruntClass);
	const FEnemyPopulationPhaseEntry* Population = Phase ? Phase->EnemyPopulationEntries.FindByPredicate([this](const FEnemyPopulationPhaseEntry& Entry)
	{
		return Entry.EnemyClass == SpatialPressureGruntClass;
	}) : nullptr;
	if (!Phase || !Definition || Definition->PressureSpawnMode != EEnemyPressureSpawnMode::MaintainPopulation || !Population
		|| GetAliveCountForSpawnClass(SpatialPressureGruntClass) >= Population->MaxPopulation) return nullptr;

	ACharacterBase* Player = GetActivePlayerCharacter();
	OutSectorIndex = ChooseReplacementSector(SectorCounts);
	if (!Player || OutSectorIndex == INDEX_NONE) return nullptr;
	const int32 SectorCount = SectorCounts.Num();
	const float SectorWidth = 360.0f / static_cast<float>(SectorCount);
	const float SectorCenter = (static_cast<float>(OutSectorIndex) + 0.5f) * SectorWidth;
	FVector SpawnLocation;
	if (!FindSpawnLocation(Player->GetActorLocation(), SpatialPressureGruntClass, SpawnLocation, true, SectorCenter, SectorWidth,
		ReplacementSpawnDistanceMin, ReplacementSpawnDistanceMax)
		&& !FindSpawnLocation(Player->GetActorLocation(), SpatialPressureGruntClass, SpawnLocation, true, SectorCenter, SectorWidth,
			MinSpawnDistance, MaxSpawnDistance)) return nullptr;

	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	AEnemyBase* Enemy = GetWorld()->SpawnActor<AEnemyBase>(SpatialPressureGruntClass, SpawnLocation, FRotator::ZeroRotator, Parameters);
	if (!Enemy) return nullptr;
	Enemy->SpawnDefaultController();
	Enemy->ApplySpawnDifficultyScaling(GetHealthMultiplier(*Definition), GetDamageMultiplier());
	ApplyEnemySpawnModifierContexts(Enemy);
	if (Enemy->IsBloodbound()) OnEnemyBecameBloodbound.Broadcast(Enemy);
	Enemy->OnDestroyed.AddDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDestroyed);
	Enemy->OnEnemyDied.AddUniqueDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDied);
	SpawnedEnemies.Add(Enemy);
	IncrementAliveCountForSpawnedEnemy(Enemy, SpatialPressureGruntClass);
	TrackEnemySpawnMetadata(Enemy, false);
#if !UE_BUILD_SHIPPING
	if (CVarDebugSpatialPressure.GetValueOnGameThread() != 0)
	{
		DrawDebugSphere(GetWorld(), SpawnLocation, 65.0f, 16, FColor::Cyan, false, SpatialPressureEvaluationInterval, 0, 3.0f);
		DrawDebugLine(GetWorld(), Player->GetActorLocation() + FVector(0, 0, 60), SpawnLocation + FVector(0, 0, 60), FColor::Cyan, false, SpatialPressureEvaluationInterval, 0, 2.0f);
	}
#endif
	return Enemy;
}

void AEnemySpawner::DrawSpatialPressureDebug(const ACharacterBase* Player, const FVector& MovementDirection, const TArray<int32>& SectorCounts, const TArray<AEnemyBase*>& StaleEnemies) const
{
#if !UE_BUILD_SHIPPING
	if (!Player || CVarDebugSpatialPressure.GetValueOnGameThread() == 0) return;
	const FVector Origin = Player->GetActorLocation() + FVector(0, 0, 80);
	DrawDebugDirectionalArrow(GetWorld(), Origin, Origin + MovementDirection * 700.0f, 80.0f, FColor::Green, false, SpatialPressureEvaluationInterval, 0, 4.0f);
	const float Width = 2.0f * PI / static_cast<float>(FMath::Max(1, SectorCounts.Num()));
	for (int32 Index = 0; Index < SectorCounts.Num(); ++Index)
	{
		const float Angle = Index * Width;
		const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0);
		DrawDebugLine(GetWorld(), Origin, Origin + Direction * 1000.0f, FColor::Silver, false, SpatialPressureEvaluationInterval, 0, 1.0f);
		const float LabelAngle = Angle + Width * 0.5f;
		DrawDebugString(GetWorld(), Origin + FVector(FMath::Cos(LabelAngle), FMath::Sin(LabelAngle), 0) * 850.0f,
			FString::Printf(TEXT("%d: %d"), Index, SectorCounts[Index]), nullptr, FColor::White, SpatialPressureEvaluationInterval, false, 1.1f);
	}
	for (AEnemyBase* Enemy : StaleEnemies)
	{
		if (Enemy) DrawDebugSphere(GetWorld(), Enemy->GetActorLocation() + FVector(0, 0, 60), 75.0f, 12, FColor::Magenta, false, SpatialPressureEvaluationInterval, 0, 3.0f);
	}
#endif
}

void AEnemySpawner::EvaluateSpatialPressure(bool bAllowRecycling, bool bForceLog)
{
	if (bTrialSuspended || bRunTimeFrozen || !bSpawningEnabled || !bEnableSpatialPressureRecycling || !SpatialPressureGruntClass) return;
	ACharacterBase* Player = GetActivePlayerCharacter();
	const FEnemyPressurePhase* Phase = ResolveActivePressurePhase();
	if (!Player || !Phase) return;
	const FEnemyPopulationPhaseEntry* GruntPopulation = Phase->EnemyPopulationEntries.FindByPredicate([this](const FEnemyPopulationPhaseEntry& Entry)
	{
		return Entry.EnemyClass == SpatialPressureGruntClass;
	});
	if (!GruntPopulation || GruntPopulation->DesiredPopulation <= 0) return;
	PruneTrackedEnemies();
	const int32 GruntAlive = GetAliveCountForSpawnClass(SpatialPressureGruntClass);
	const bool bPopulationHealthy = static_cast<float>(GruntAlive) >= static_cast<float>(GruntPopulation->DesiredPopulation) * FMath::Clamp(MinimumGruntPopulationRatioForRecycling, 0.0f, 1.0f);
	FVector Velocity = Player->GetVelocity(); Velocity.Z = 0.0f;
	const float PlayerSpeed = Velocity.Size();
	const bool bMovingEnough = PlayerSpeed >= MinimumPlayerSpeedForDirectionalRecycling;
	const FVector MovementDirection = bMovingEnough ? Velocity / PlayerSpeed : FVector::ZeroVector;
	const FVector PlayerLocation = Player->GetActorLocation();
	const int32 SectorCount = FMath::Clamp(SpatialSectorCount, 4, 16);
	TArray<int32> SectorCounts; SectorCounts.Init(0, SectorCount);
	struct FStaleCandidate { AEnemyBase* Enemy; float Distance; float Dot; float Score; int32 Sector; };
	TArray<FStaleCandidate> Candidates;
	const float SectorWidth = 360.0f / static_cast<float>(SectorCount);
	for (const TPair<TObjectKey<AEnemyBase>, UClass*>& Pair : CountedEnemyClassByEnemy)
	{
		AEnemyBase* Enemy = Pair.Key.ResolveObjectPtr();
		if (!IsValid(Enemy) || Enemy->IsDead() || Pair.Value != SpatialPressureGruntClass.Get()) continue;
		FVector Offset = Enemy->GetActorLocation() - PlayerLocation; Offset.Z = 0.0f;
		const float Distance = Offset.Size();
		int32 EnemySector = INDEX_NONE;
		if (Distance > KINDA_SMALL_NUMBER)
		{
			const float Angle = FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X)) + 360.0f, 360.0f);
			EnemySector = FMath::Clamp(FMath::FloorToInt(Angle / SectorWidth), 0, SectorCount - 1);
			SectorCounts[EnemySector]++;
		}
		const float* SpawnTime = EnemySpawnRunTimeByEnemy.Find(Pair.Key);
		const float* ProtectedUntil = EventRecycleProtectionEndTimeByEnemy.Find(Pair.Key);
		const float Dot = Distance > KINDA_SMALL_NUMBER && bMovingEnough ? FVector::DotProduct(Offset / Distance, MovementDirection) : 1.0f;
		if (bMovingEnough && Distance >= StaleGruntMinimumDistance && Dot <= StaleGruntBehindDotThreshold
			&& SpawnTime && RunTimeSeconds - *SpawnTime >= MinimumSecondsAliveBeforeRecyclable
			&& (!ProtectedUntil || RunTimeSeconds >= *ProtectedUntil))
		{
			Candidates.Add({ Enemy, Distance, Dot, Distance / FMath::Max(1.0f, StaleGruntMinimumDistance) + (1.0f - Dot), EnemySector });
		}
	}
	Candidates.Sort([](const FStaleCandidate& A, const FStaleCandidate& B) { return A.Score > B.Score; });
	TArray<AEnemyBase*> DebugStale; for (const FStaleCandidate& Candidate : Candidates) DebugStale.Add(Candidate.Enemy);
	DrawSpatialPressureDebug(Player, MovementDirection, SectorCounts, DebugStale);
	const bool bRecycleAllowed = bAllowRecycling && bMovingEnough && bPopulationHealthy;
	const int32 RecycleTarget = bRecycleAllowed ? FMath::Min(FMath::Max(0, MaxGruntsRecycledPerEvaluation), Candidates.Num()) : 0;
	TArray<FString> RecycleLines;
	TArray<int32> ReplacementSectors;
	int32 Recycled = 0, Replaced = 0;
	for (int32 Index = 0; Index < RecycleTarget; ++Index)
	{
		const FStaleCandidate& Candidate = Candidates[Index];
		RecycleLines.Add(FString::Printf(TEXT("%s Distance=%.0f Dot=%.2f"), *GetNameSafe(Candidate.Enemy), Candidate.Distance, Candidate.Dot));
		if (!RecycleLivingEnemy(Candidate.Enemy)) continue;
		++Recycled;
		if (SectorCounts.IsValidIndex(Candidate.Sector)) SectorCounts[Candidate.Sector] = FMath::Max(0, SectorCounts[Candidate.Sector] - 1);
		int32 Sector = INDEX_NONE;
		if (SpawnSpatialPressureReplacement(SectorCounts, Sector))
		{
			++Replaced; ReplacementSectors.Add(Sector); if (SectorCounts.IsValidIndex(Sector)) ++SectorCounts[Sector];
		}
	}
#if !UE_BUILD_SHIPPING
	if (bForceLog || CVarLogSpatialPressure.GetValueOnGameThread() != 0)
	{
		FString Counts, Replacements;
		for (int32 Index = 0; Index < SectorCounts.Num(); ++Index) Counts += FString::Printf(TEXT("%s%d=%d"), Index ? TEXT(" ") : TEXT(""), Index, SectorCounts[Index]);
		for (int32 Index = 0; Index < ReplacementSectors.Num(); ++Index) Replacements += FString::Printf(TEXT("%s%d"), Index ? TEXT(",") : TEXT(""), ReplacementSectors[Index]);
		UE_LOG(LogTemp, Log, TEXT("SpatialPressure: Enabled=true PlayerSpeed=%.0f GruntAlive=%d GruntDesired=%d RecycleAllowed=%s SectorCounts=[%s] StaleCandidates=%d RecyclingThisPass=%d Replaced=%d ReplacementSectors=[%s]"),
			PlayerSpeed, GruntAlive, GruntPopulation->DesiredPopulation, bRecycleAllowed ? TEXT("true") : TEXT("false"), *Counts, Candidates.Num(), Recycled, Replaced, *Replacements);
		for (const FString& Line : RecycleLines) UE_LOG(LogTemp, Log, TEXT("  Recycle: %s"), *Line);
	}
#endif
}

void AEnemySpawner::HandleSpatialPressureTimerElapsed()
{
	EvaluateSpatialPressure(true, false);
}

#if !UE_BUILD_SHIPPING
void AEnemySpawner::LogSpatialPressureStatus() { EvaluateSpatialPressure(false, true); }
void AEnemySpawner::ForceSpatialPressurePass() { EvaluateSpatialPressure(true, true); }
#endif

void AEnemySpawner::LogPressureDirectorStatus()
{
	PruneTrackedEnemies();
	int32 PhaseIndex = INDEX_NONE;
	const FEnemyPressurePhase* Phase = ResolveActivePressurePhase(&PhaseIndex);
	if (!Phase)
	{
		UE_LOG(LogTemp, Warning, TEXT("=== PRESSURE DIRECTOR === RunTime=%.2f No uniquely valid phase."), RunTimeSeconds);
		return;
	}

	const int32 Alive = GetAliveEnemyCount();
	const int32 Desired = GetTotalDesiredPopulation(*Phase);
	const float Ratio = Desired > 0 ? static_cast<float>(Alive) / static_cast<float>(Desired) : 1.0f;
	const TCHAR* Mode = Ratio < 0.5f ? TEXT("Emergency") : (Ratio < 0.8f ? TEXT("Accelerated") : TEXT("Normal"));
	FString Detail;
	for (const FEnemyPopulationPhaseEntry& Entry : Phase->EnemyPopulationEntries)
	{
		const int32 ClassAlive = GetAliveCountForSpawnClass(Entry.EnemyClass);
		const FEnemySpawnEntry* Definition = FindSpawnDefinition(Entry.EnemyClass);
		const bool bTimedThreat = Definition && Definition->PressureSpawnMode == EEnemyPressureSpawnMode::TimedThreat;
		float CooldownRemaining = 0.0f;
		const bool bCooldownActive = IsClassCooldownActive(Entry.EnemyClass, &CooldownRemaining);
		const bool bUnlocked = Definition && IsSpawnDefinitionUnlocked(*Definition);
		const bool bEligible = bTimedThreat ? IsTimedThreatEntryEligible(*Phase, Entry) : IsPopulationEntryEligible(*Phase, Entry);
		Detail += FString::Printf(TEXT("\n  %s Mode=%s Alive=%d Desired=%d Max=%d Deficit=%d Unlocked=%s Eligible=%s CooldownActive=%s CooldownRemaining=%.2f"),
			*GetNameSafe(Entry.EnemyClass.Get()), bTimedThreat ? TEXT("TimedThreat") : TEXT("MaintainPopulation"),
			ClassAlive, Entry.DesiredPopulation, Entry.MaxPopulation,
			bTimedThreat ? 0 : FMath::Max(0, Entry.DesiredPopulation - ClassAlive),
			bUnlocked ? TEXT("true") : TEXT("false"), bEligible ? TEXT("true") : TEXT("false"),
			bCooldownActive ? TEXT("true") : TEXT("false"), CooldownRemaining);
	}
	Detail += FString::Printf(TEXT("\nEventsEnabled=%s ActiveEvent=%s NextGlobalEventIn=%.2f LastEvent=%s BiasAngle=%.1f BiasRemaining=%.2f"),
		Phase->bEventsEnabled ? TEXT("true") : TEXT("false"),
		PressureEvents.IsValidIndex(ActiveEventIndex) ? *PressureEvents[ActiveEventIndex].EventName.ToString() : TEXT("None"),
		FMath::Max(0.0f, NextGlobalEventTime - RunTimeSeconds), *LastCompletedEventName.ToString(),
		DirectionalBiasAngleDegrees, FMath::Max(0.0f, DirectionalBiasEndRunTime - RunTimeSeconds));
	for (const FEnemyPressureEventDefinition& Event : PressureEvents)
	{
		FString Reason;
		const bool bSelectable = ActiveEventIndex == INDEX_NONE && RunTimeSeconds >= NextGlobalEventTime && IsEventSelectable(Event, *Phase, &Reason);
		const float* EventEligibleTime = NextEligibleEventTimeByName.Find(Event.EventName);
		Detail += FString::Printf(TEXT("\n  Event=%s Enabled=%s TimeValid=%s ClassValid=%s CooldownRemaining=%.2f Weight=%.1f Selectable=%s Reason=%s"),
			*Event.EventName.ToString(), Event.bEnabled ? TEXT("true") : TEXT("false"),
			(RunTimeSeconds >= FMath::Max(120.0f, Event.MinimumRunTime) && (Event.MaximumRunTime <= 0.0f || RunTimeSeconds < Event.MaximumRunTime)) ? TEXT("true") : TEXT("false"),
			Reason.Contains(TEXT("class")) ? TEXT("false") : TEXT("true"),
			EventEligibleTime ? FMath::Max(0.0f, *EventEligibleTime - RunTimeSeconds) : 0.0f,
			Event.Weight, bSelectable ? TEXT("true") : TEXT("false"), *Reason);
	}

	UE_LOG(LogTemp, Log, TEXT("=== PRESSURE DIRECTOR ===\nRunTime=%.2f Phase[%d]=%s\nTotalAlive=%d MaintainedDesired=%d GlobalMax=%d MaintainedRatio=%.3f RefillMode=%s Interval=%.2f BatchMax=%d DamageMultiplier=%.2f%s\n========================="),
		RunTimeSeconds, PhaseIndex, *Phase->PhaseName.ToString(), Alive, Desired, GetCurrentMaxAliveEnemies(), Ratio, Mode,
		GetCurrentSpawnInterval(), GetCurrentPopulationBatchSize(*Phase), GetDamageMultiplier(), *Detail);
}

void AEnemySpawner::HandleSpawnedEnemyDestroyed(AActor* DestroyedActor)
{
	UnregisterLivingEnemy(Cast<AEnemyBase>(DestroyedActor));

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

void AEnemySpawner::ApplyEnemySpawnModifierContexts(AEnemyBase* SpawnedEnemy)
{
	if (!SpawnedEnemy || EnemySpawnModifierContexts.Num() == 0)
	{
		return;
	}

	bool bMakeBloodbound = false;
	bool bDropsXP = true;
	float HealthMultiplier = 1.0f;
	float DamageMultiplier = 1.0f;
	float MovementSpeedMultiplier = 1.0f;
	for (const TPair<FName, FEnemySpawnModifierContext>& Pair : EnemySpawnModifierContexts)
	{
		const FEnemySpawnModifierContext& Context = Pair.Value;
		bMakeBloodbound |= Context.bMakeBloodbound;
		bDropsXP &= Context.bDropsXP;
		HealthMultiplier *= FMath::Max(0.0f, Context.HealthMultiplier);
		DamageMultiplier *= FMath::Max(0.0f, Context.DamageMultiplier);
		MovementSpeedMultiplier *= FMath::Max(0.0f, Context.MovementSpeedMultiplier);
	}

	if (bMakeBloodbound)
	{
		SpawnedEnemy->MakeBloodbound(HealthMultiplier, DamageMultiplier, MovementSpeedMultiplier, bDropsXP);
	}
	else
	{
		SpawnedEnemy->ApplySpawnInstanceModifiers(HealthMultiplier, DamageMultiplier, MovementSpeedMultiplier);
	}
}

void AEnemySpawner::HandleSpawnedEnemyDied(AEnemyBase* Enemy)
{
	if (Enemy)
	{
		UClass* CountedClass = nullptr;
		if (UClass* const* RegisteredClass = CountedEnemyClassByEnemy.Find(TObjectKey<AEnemyBase>(Enemy)))
		{
			CountedClass = *RegisteredClass;
		}
		UnregisterLivingEnemy(Enemy);

		const FEnemySpawnEntry* SpawnDefinition = CountedClass ? FindSpawnDefinition(CountedClass) : nullptr;
		if (SpawnDefinition)
		{
			const float MinimumDelay = FMath::Max(0.0f, FMath::Min(SpawnDefinition->MinRespawnDelayAfterDeath, SpawnDefinition->MaxRespawnDelayAfterDeath));
			const float MaximumDelay = FMath::Max(0.0f, FMath::Max(SpawnDefinition->MinRespawnDelayAfterDeath, SpawnDefinition->MaxRespawnDelayAfterDeath));
			if (MaximumDelay > 0.0f)
			{
				const float NextEligibleTime = RunTimeSeconds + FMath::FRandRange(MinimumDelay, MaximumDelay);
				float& StoredEligibleTime = NextEligibleSpawnTimeByClass.FindOrAdd(CountedClass);
				StoredEligibleTime = FMath::Max(StoredEligibleTime, NextEligibleTime);
				if (bDebugSpawning || CVarLogSpawnDirector.GetValueOnGameThread() != 0)
				{
					UE_LOG(LogTemp, Log, TEXT("Spawn Director started %s replacement cooldown: %.2fs remaining (eligible at run time %.2f)."),
						*GetNameSafe(CountedClass), StoredEligibleTime - RunTimeSeconds, StoredEligibleTime);
				}
			}
		}
		OnEnemyKilled.Broadcast(Enemy);
	}
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
