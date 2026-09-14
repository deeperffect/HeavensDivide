// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemySpawner.h"
#include "EnemySpawnerDiagnostics.h"
#include "TimerManager.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "EnemyBase.h"
#include "EnemySpawnArea.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Stats/Stats.h"
#include "SurvivorPlayerController.h"

TAutoConsoleVariable<int32> CVarLogSpawnDirector(
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
