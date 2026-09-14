#include "EnemySpawner.h"
#include "EnemySpawnerDiagnostics.h"

#include "AutoAttackComponent.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "EnemyBase.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivorPlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

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
