// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectKey.h"
#include "EnemySpawner.generated.h"

class ACharacterBase;
class AEnemyBase;
class AEnemySpawnArea;
class UAutoAttackComponent;
class UCurveFloat;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSpawnerEnemyKilled, AEnemyBase*, Enemy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSpawnerEnemyBecameBloodbound, AEnemyBase*, Enemy);

USTRUCT(BlueprintType)
struct FEnemySpawnModifierContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawning|Modifiers")
	bool bMakeBloodbound = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawning|Modifiers", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawning|Modifiers", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawning|Modifiers", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MovementSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawning|Modifiers")
	bool bDropsXP = true;
};

USTRUCT(BlueprintType)
struct FEnemySpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ToolTip = "Enemy Blueprint class this spawn entry creates. Per-type caps count enemies spawned from this exact configured class."))
	TSubclassOf<AEnemyBase> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Relative chance this entry is picked among currently eligible entries. Higher weight means more common."))
	float SpawnWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Earliest run time in seconds when this enemy type can spawn. 60 means starts after 1 minute."))
	float MinimumRunTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Latest run time in seconds when this enemy type can spawn. 0 means no maximum and it can keep spawning forever."))
	float MaximumRunTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Spawn budget cost for this enemy. Higher cost makes this enemy consume more of a spawn batch budget, reducing how many can spawn together."))
	int32 SpawnCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0", UIMin = "0", ToolTip = "Maximum number of living enemies of this type allowed at once. 0 = unlimited."))
	int32 MaxAliveOfThisType = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ToolTip = "Whether this spawn entry is allowed to be selected. Disable to temporarily remove this enemy type from spawning."))
	bool bEnabled = true;
};

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawner();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "Enemy Spawning")
	void StartSpawning();

	UFUNCTION(BlueprintCallable, Category = "Enemy Spawning")
	void StopSpawning();

	UFUNCTION(BlueprintCallable, Category = "Enemy Spawning")
	void SetSpawningEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Enemy Spawning")
	AEnemyBase* SpawnEnemy();

#if !UE_BUILD_SHIPPING
	void StressEnemies(int32 DesiredLivingEnemyCount);
	void ClearStressEnemies();
#endif

	UFUNCTION(BlueprintPure, Category = "Run")
	float GetRunTimeSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Run")
	float GetRunTimeMinutes() const;

	UFUNCTION(BlueprintCallable, Category = "Run")
	void FreezeRunTime();

	UFUNCTION(BlueprintCallable, Category = "Run")
	void SetTrialSuspended(bool bSuspended);

	UFUNCTION(BlueprintPure, Category = "Run")
	bool IsTrialSuspended() const { return bTrialSuspended; }

	UFUNCTION(BlueprintCallable, Category = "Run Scaling|Modifiers")
	void SetSpawnPressureModifier(FName ModifierId, float Multiplier);

	UFUNCTION(BlueprintCallable, Category = "Run Scaling|Modifiers")
	void RemoveSpawnPressureModifier(FName ModifierId);

	UFUNCTION(BlueprintPure, Category = "Run Scaling|Modifiers")
	float GetSpawnPressureModifierProduct() const;

	UFUNCTION(BlueprintPure, Category = "Run Scaling|Modifiers")
	float GetEffectiveSpawnPressure() const;

	UFUNCTION(BlueprintCallable, Category = "Enemy Spawning|Modifiers")
	void SetEnemySpawnModifierContext(FName ModifierId, const FEnemySpawnModifierContext& ModifierContext);

	UFUNCTION(BlueprintCallable, Category = "Enemy Spawning|Modifiers")
	void RemoveEnemySpawnModifierContext(FName ModifierId);

	UFUNCTION(BlueprintCallable, Category = "Enemy Spawning|Modifiers", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	int32 ConvertRandomAliveEnemiesToBloodbound(const FEnemySpawnModifierContext& BloodboundContext, float ConversionPercent);

	UPROPERTY(BlueprintAssignable, Category = "Enemy Spawning|Events")
	FSpawnerEnemyKilled OnEnemyKilled;

	UPROPERTY(BlueprintAssignable, Category = "Enemy Spawning|Events")
	FSpawnerEnemyBecameBloodbound OnEnemyBecameBloodbound;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Pool", meta = (ToolTip = "List of enemy types this spawner can create. Each entry has its own weight, runtime window, cost, and optional per-type alive cap."))
	TArray<FEnemySpawnEntry> EnemySpawnEntries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0.01", UIMin = "0.01", ToolTip = "Base seconds between spawn batches before Spawn Pressure scaling is applied. Higher values spawn less often."))
	float BaseSpawnInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0.01", UIMin = "0.01", ToolTip = "Fastest allowed seconds between spawn batches after Spawn Pressure scaling. Prevents spawning from becoming too frequent."))
	float MinSpawnInterval = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Base number of enemies the spawner tries to create per spawn batch before Spawn Pressure scaling."))
	int32 BaseEnemiesPerSpawn = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Maximum enemies that can be spawned in a single batch after Spawn Pressure scaling."))
	int32 MaxEnemiesPerSpawn = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Base total living enemy cap before Spawn Pressure scaling. This is shared across all enemy types."))
	int32 BaseMaxAliveEnemies = 60;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Hard maximum total living enemy cap. Per-type caps cannot push the total above this value."))
	int32 MaximumAliveEnemies = 200;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Base spawn budget available per batch. Enemy entries spend this through Spawn Cost."))
	int32 BaseSpawnBudget = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Maximum spawn budget available per batch after Spawn Pressure scaling."))
	int32 MaxSpawnBudget = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Minimum 2D distance from the active player where enemies are allowed to spawn."))
	float MinSpawnDistance = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Maximum 2D distance from the active player where enemies are allowed to spawn."))
	float MaxSpawnDistance = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ToolTip = "Master switch for normal enemy spawning. Disable to stop this spawner from scheduling new normal spawn batches."))
	bool bSpawningEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Scaling", meta = (ToolTip = "Optional curve evaluated by run time in seconds to scale enemy max health. If unset, the C++ default health scaling is used."))
	TObjectPtr<UCurveFloat> HealthMultiplierCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Scaling", meta = (ToolTip = "Optional curve evaluated by run time in seconds to scale enemy damage. If unset, the C++ default damage scaling is used."))
	TObjectPtr<UCurveFloat> DamageMultiplierCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Scaling", meta = (ToolTip = "Optional curve evaluated by run time in seconds to scale spawn pressure. Spawn pressure affects interval, batch size, alive cap, and budget."))
	TObjectPtr<UCurveFloat> SpawnPressureCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Scaling", meta = (ClampMin = "0.01", UIMin = "0.01", ToolTip = "Seconds between internal run-time/scaling updates. Lower updates scaling more often; higher is cheaper."))
	float RunTimeUpdateInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Performance", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Enemies farther than this 2D distance from the active player can be despawned by distant cleanup. 0 disables distance cleanup."))
	float MaxEnemyDistanceFromPlayer = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Performance", meta = (ClampMin = "0.1", UIMin = "0.1", ToolTip = "Seconds between distant-enemy cleanup checks."))
	float DistantEnemyCheckInterval = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Performance", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Maximum number of distant enemies this spawner may despawn per cleanup check."))
	int32 MaxDistantDespawnsPerCheck = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Ground Trace", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "How far above a candidate spawn location the ground trace starts."))
	float GroundTraceHeight = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Ground Trace", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "How far below a candidate spawn location the ground trace searches for valid gameplay floor."))
	float GroundTraceDepth = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation", meta = (ToolTip = "Optional spawn area actor that constrains valid enemy spawn locations. If set, candidates outside this area are rejected."))
	TObjectPtr<AEnemySpawnArea> ActiveSpawnArea;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Number of candidate locations to try before giving up on a spawn attempt."))
	int32 MaxSpawnLocationAttempts = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Padding from the edge of the active spawn area. Higher values keep spawns farther inside the area bounds."))
	float SpawnEdgePadding = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation", meta = (ToolTip = "Extent used when projecting candidate spawn locations to the NavMesh if nav projection is enabled."))
	FVector NavMeshProjectionExtent = FVector(250.0f, 250.0f, 500.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation", meta = (ToolTip = "Requires spawn candidates to successfully project to the NavMesh before they can spawn."))
	bool bRequireNavMeshProjection = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation", meta = (ToolTip = "When normal ring candidates fail, allows fallback candidates inside the active spawn area."))
	bool bUseSpawnAreaFallbackCandidates = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation", meta = (ClampMin = "1.0", UIMin = "1.0", ToolTip = "Capsule radius used for spawn collision validation when the enemy class capsule cannot be read."))
	float FallbackCapsuleRadius = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation", meta = (ClampMin = "1.0", UIMin = "1.0", ToolTip = "Capsule half-height used for spawn collision validation when the enemy class capsule cannot be read."))
	float FallbackCapsuleHalfHeight = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Extra vertical clearance added during spawn collision validation above the ground."))
	float CollisionValidationGroundClearance = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Debug", meta = (ToolTip = "Logs high-level spawn decisions and draws successful spawn locations."))
	bool bDebugSpawning = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Debug", meta = (ToolTip = "Logs detailed spawn-location validation failures such as arena, ground, navigation, and collision rejection."))
	bool bDebugSpawnValidation = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stress Test", meta = (DevelopmentOnly, ToolTip = "Enemy class used by hd.StressEnemies for development-only stress testing."))
	TSubclassOf<AEnemyBase> StressTestEnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stress Test", meta = (ClampMin = "1", UIMin = "1", DevelopmentOnly, ToolTip = "Number of stress enemies spawned per stress-test batch."))
	int32 StressSpawnBatchSize = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stress Test", meta = (ClampMin = "0.01", UIMin = "0.01", DevelopmentOnly, ToolTip = "Seconds between stress-test spawn batches."))
	float StressSpawnBatchInterval = 0.05f;

	UPROPERTY()
	TArray<TWeakObjectPtr<AEnemyBase>> StressTestEnemies;

	FTimerHandle StressSpawnTimerHandle;
	int32 RequestedStressEnemyCount = 0;
	bool bStressPauseNormalSpawning = false;
	bool bSavedSpawningEnabledBeforeStressPause = false;
	bool bStressAutoAttacksDisabled = false;
	bool bSavedFirstAutoAttackEnabled = false;
	bool bSavedSecondAutoAttackEnabled = false;

	UPROPERTY()
	TArray<TWeakObjectPtr<AEnemyBase>> SpawnedEnemies;

	TMap<UClass*, int32> AliveEnemyCountByClass;
	TMap<TObjectKey<AEnemyBase>, UClass*> CountedEnemyClassByEnemy;
	TMap<FName, float> SpawnPressureModifiers;
	TMap<FName, FEnemySpawnModifierContext> EnemySpawnModifierContexts;

	FTimerHandle SpawnTimerHandle;
	FTimerHandle RunTimeTimerHandle;
	FTimerHandle DistantEnemyCheckTimerHandle;
	float RunTimeSeconds = 0.0f;
	bool bRunTimeFrozen = false;
	bool bTrialSuspended = false;

	ACharacterBase* GetActivePlayerCharacter() const;
	const FEnemySpawnEntry* ChooseSpawnEntry(int32 RemainingBudget) const;
	AEnemyBase* SpawnEnemyFromEntry(const FEnemySpawnEntry& SpawnEntry);
	bool FindSpawnLocation(const FVector& ActivePlayerLocation, TSubclassOf<AEnemyBase> EnemyClass, FVector& OutSpawnLocation) const;
	bool GenerateCandidateSpawnLocation(const FVector& ActivePlayerLocation, int32 AttemptIndex, int32 AttemptCount, FVector& OutCandidateLocation) const;
	bool ProjectSpawnLocationToNavigation(const FVector& CandidateLocation, FVector& OutProjectedLocation) const;
	bool IsSpawnLocationInsideArena(const FVector& Location) const;
	bool IsSpawnLocationCollisionFree(const FVector& Location, TSubclassOf<AEnemyBase> EnemyClass) const;
	void GetEnemyCapsuleDimensions(TSubclassOf<AEnemyBase> EnemyClass, float& OutRadius, float& OutHalfHeight) const;
	int32 GetAliveEnemyCount();
	float GetHealthMultiplier() const;
	float GetDamageMultiplier() const;
	float GetSpawnPressure() const;
	float GetCurrentSpawnInterval() const;
	int32 GetCurrentEnemiesPerSpawn() const;
	int32 GetCurrentMaxAliveEnemies() const;
	int32 GetCurrentSpawnBudget() const;
	float EvaluateDefaultHealthMultiplier() const;
	float EvaluateDefaultDamageMultiplier() const;
	float EvaluateDefaultSpawnPressure() const;
	bool IsSpawnEntryEligible(const FEnemySpawnEntry& Entry, int32 RemainingBudget, bool bLogLimitFailures) const;
	int32 GetAliveCountForSpawnClass(TSubclassOf<AEnemyBase> EnemyClass) const;
	void IncrementAliveCountForSpawnedEnemy(AEnemyBase* SpawnedEnemy, TSubclassOf<AEnemyBase> SpawnClass);
	void DecrementAliveCountForDestroyedEnemy(AActor* DestroyedActor);
	void PruneTrackedEnemies();
	void ApplyEnemySpawnModifierContexts(AEnemyBase* SpawnedEnemy);
	void HandleRunTimeTimerElapsed();
	void HandleSpawnTimerElapsed();
	void HandleDistantEnemyCheckTimerElapsed();
	void RescheduleSpawnTimer();
	void LogDirectorStatus() const;

	UFUNCTION()
	void HandleSpawnedEnemyDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleSpawnedEnemyDied(AEnemyBase* Enemy);

#if !UE_BUILD_SHIPPING
	TSubclassOf<AEnemyBase> GetStressTestEnemyClass() const;
	bool IsStressTestEnemyClassConfigured() const;
	AEnemyBase* SpawnStressEnemy();
	void HandleStressSpawnTimerElapsed();
	void PruneStressTestEnemies();
	int32 GetLivingStressEnemyCount() const;
	void SetStressPauseNormalSpawning(bool bPause);
	void DisablePlayerAutoAttacksForStressTest();
	void RestorePlayerAutoAttacksAfterStressTest();
	UAutoAttackComponent* FindAutoAttackComponent(ACharacterBase* Character) const;
#endif
};
