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

UENUM(BlueprintType)
enum class EEnemyPressureSpawnMode : uint8
{
	MaintainPopulation UMETA(DisplayName = "Maintain Population"),
	TimedThreat UMETA(DisplayName = "Timed Threat")
};

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawning|Scaling", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Continuous max-health growth per elapsed run minute for this enemy class. 0.05 means +5% base max health per minute."))
	float HealthScalingPerMinute = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawning|Pressure", meta = (ToolTip = "Maintained enemies refill population deficits. Timed threats use phase MaxPopulation and death-based replacement cooldowns."))
	EEnemyPressureSpawnMode PressureSpawnMode = EEnemyPressureSpawnMode::MaintainPopulation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawning|Pressure", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Minimum class-level replacement delay, measured from logical death using authoritative run time."))
	float MinRespawnDelayAfterDeath = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawning|Pressure", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Maximum class-level replacement delay. Values below the minimum are handled safely."))
	float MaxRespawnDelayAfterDeath = 0.0f;
};

USTRUCT(BlueprintType)
struct FEnemyPopulationPhaseEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure")
	TSubclassOf<AEnemyBase> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure", meta = (ClampMin = "0", UIMin = "0"))
	int32 DesiredPopulation = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure", meta = (ClampMin = "0", UIMin = "0", ToolTip = "Hard living cap for this class in this phase. Zero disables routine spawning for the entry."))
	int32 MaxPopulation = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Multiplied by the current population deficit when selecting what to refill."))
	float RefillPriority = 1.0f;
};

USTRUCT(BlueprintType)
struct FEnemyPressureEventEnemyEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event")
	TSubclassOf<AEnemyBase> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event", meta = (ClampMin = "0", UIMin = "0", ToolTip = "How far this event may exceed the active phase's class MaxPopulation."))
	int32 ClassOverflowAllowance = 0;
};

USTRUCT(BlueprintType)
struct FEnemyPressureEventDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event") FName EventName = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event") bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event", meta = (ClampMin = "0.0")) float MinimumRunTime = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event") float MaximumRunTime = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event", meta = (ClampMin = "0.0")) float Weight = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event", meta = (ClampMin = "0.0")) float EventCooldownMin = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event", meta = (ClampMin = "0.0")) float EventCooldownMax = 45.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event", meta = (ClampMin = "0.0", ClampMax = "360.0")) float SpawnArcDegrees = 60.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event", meta = (ClampMin = "0.0")) float SpawnDistanceMin = 1800.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event", meta = (ClampMin = "0.0")) float SpawnDistanceMax = 2800.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event", meta = (ClampMin = "0.01")) float DelayBetweenMembers = 0.10f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event", meta = (ClampMin = "0.0", ToolTip = "Optional delay after the first member before the rest of the event begins.")) float DelayAfterFirstMember = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event") bool bAllowTemporaryPopulationOverflow = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event", meta = (ClampMin = "0")) int32 EventPopulationOverflowAllowance = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event") bool bIgnoreThreatDeathCooldown = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure Event") TArray<FEnemyPressureEventEnemyEntry> EnemyEntries;
};

USTRUCT(BlueprintType)
struct FEnemyPressurePhase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure")
	FName PhaseName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StartTimeSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure", meta = (ToolTip = "Exclusive end time. A value <= 0 makes this the open-ended final phase."))
	float EndTimeSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure", meta = (ClampMin = "0", UIMin = "0"))
	int32 GlobalMaxAlive = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float NormalSpawnInterval = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float AcceleratedSpawnInterval = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float EmergencySpawnInterval = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure")
	TArray<FEnemyPopulationPhaseEntry> EnemyPopulationEntries;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Events") bool bEventsEnabled = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Events", meta = (ClampMin = "0.0")) float EventIntervalMin = 35.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Events", meta = (ClampMin = "0.0")) float EventIntervalMax = 45.0f;
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

	UFUNCTION(BlueprintCallable, Category = "Enemy Spawning|Debug")
	void LogPressureDirectorStatus();

#if !UE_BUILD_SHIPPING
	void TriggerPressureEventByName(FName EventName);
	void LogSpatialPressureStatus();
	void ForceSpatialPressurePass();
#endif

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure", meta = (ToolTip = "Authored population phases. Exactly one valid phase should contain the authoritative run time."))
	TArray<FEnemyPressurePhase> PressurePhases;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Final safety cap applied after the active phase cap."))
	int32 AbsoluteHardAliveCap = 65;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Batch", meta = (ClampMin = "1", UIMin = "1"))
	int32 NormalMaxBatchSize = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Batch", meta = (ClampMin = "1", UIMin = "1"))
	int32 AcceleratedMaxBatchSize = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Batch", meta = (ClampMin = "1", UIMin = "1"))
	int32 EmergencyMaxBatchSize = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Threats", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", ToolTip = "Chance that an eligible timed threat takes a spawn slot while maintained population deficits also exist."))
	float TimedThreatSpawnSlotChance = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Events")
	TArray<FEnemyPressureEventDefinition> PressureEvents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Directional Bias") bool bDirectionalBiasEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Directional Bias", meta = (ClampMin = "0.0", ClampMax = "1.0")) float DirectionalBiasChance = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Directional Bias", meta = (ClampMin = "0.0", ClampMax = "360.0")) float DirectionalBiasArcDegrees = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Directional Bias", meta = (ClampMin = "0.1")) float DirectionalBiasDurationMin = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Directional Bias", meta = (ClampMin = "0.1")) float DirectionalBiasDurationMax = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Spatial Pressure") bool bEnableSpatialPressureRecycling = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Spatial Pressure", meta = (ToolTip = "Only this maintained fodder class may be recycled.")) TSubclassOf<AEnemyBase> SpatialPressureGruntClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Spatial Pressure", meta = (ClampMin = "0.1", Units = "s")) float SpatialPressureEvaluationInterval = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Spatial Pressure", meta = (ClampMin = "0.0", Units = "cm/s")) float MinimumPlayerSpeedForDirectionalRecycling = 150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Spatial Pressure", meta = (ClampMin = "0.0", ClampMax = "1.0")) float MinimumGruntPopulationRatioForRecycling = 0.85f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Spatial Pressure", meta = (ClampMin = "0.0", Units = "cm")) float StaleGruntMinimumDistance = 2800.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Spatial Pressure", meta = (ClampMin = "-1.0", ClampMax = "1.0")) float StaleGruntBehindDotThreshold = -0.50f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Spatial Pressure", meta = (ClampMin = "1")) int32 MaxGruntsRecycledPerEvaluation = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Spatial Pressure", meta = (ClampMin = "0.0", Units = "s")) float MinimumSecondsAliveBeforeRecyclable = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Spatial Pressure", meta = (ClampMin = "0.0", Units = "cm")) float ReplacementSpawnDistanceMin = 1200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Spatial Pressure", meta = (ClampMin = "0.0", Units = "cm")) float ReplacementSpawnDistanceMax = 2000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Spatial Pressure", meta = (ClampMin = "4", ClampMax = "16")) int32 SpatialSectorCount = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Spatial Pressure") bool bPreferUnderrepresentedSectors = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Spatial Pressure", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Blends inverse-density sector weights toward uniform randomness.")) float ReplacementSectorRandomness = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Pressure|Spatial Pressure", meta = (ClampMin = "0.0", Units = "s")) float EventGruntRecycleProtectionSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Normal survival spawning uses PressurePhases.", ClampMin = "0.01", UIMin = "0.01"))
	float BaseSpawnInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Normal survival spawning uses PressurePhases.", ClampMin = "0.01", UIMin = "0.01"))
	float MinSpawnInterval = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Normal survival spawning uses pressure-state batch sizes.", ClampMin = "1", UIMin = "1"))
	int32 BaseEnemiesPerSpawn = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Normal survival spawning uses pressure-state batch sizes.", ClampMin = "1", UIMin = "1"))
	int32 MaxEnemiesPerSpawn = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Normal survival spawning uses each phase's GlobalMaxAlive.", ClampMin = "1", UIMin = "1"))
	int32 BaseMaxAliveEnemies = 60;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Use AbsoluteHardAliveCap.", ClampMin = "1", UIMin = "1"))
	int32 MaximumAliveEnemies = 200;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "SpawnCost does not gate normal population maintenance.", ClampMin = "1", UIMin = "1"))
	int32 BaseSpawnBudget = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "SpawnCost does not gate normal population maintenance.", ClampMin = "1", UIMin = "1"))
	int32 MaxSpawnBudget = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Minimum 2D distance from the active player where enemies are allowed to spawn."))
	float MinSpawnDistance = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Maximum 2D distance from the active player where enemies are allowed to spawn."))
	float MaxSpawnDistance = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ToolTip = "Master switch for normal enemy spawning. Disable to stop this spawner from scheduling new normal spawn batches."))
	bool bSpawningEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Scaling|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Normal enemy health uses FEnemySpawnEntry.HealthScalingPerMinute."))
	TObjectPtr<UCurveFloat> HealthMultiplierCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Scaling", meta = (ToolTip = "Optional curve evaluated by run time in seconds to scale enemy damage. If unset, the C++ default damage scaling is used."))
	TObjectPtr<UCurveFloat> DamageMultiplierCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Scaling|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Normal survival spawning uses authored PressurePhases."))
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
	TMap<TObjectKey<AEnemyBase>, float> EnemySpawnRunTimeByEnemy;
	TMap<TObjectKey<AEnemyBase>, float> EventRecycleProtectionEndTimeByEnemy;
	TMap<UClass*, float> NextEligibleSpawnTimeByClass;
	TMap<FName, float> NextEligibleEventTimeByName;
	TMap<FName, float> SpawnPressureModifiers;
	TMap<FName, FEnemySpawnModifierContext> EnemySpawnModifierContexts;

	FTimerHandle SpawnTimerHandle;
	FTimerHandle RunTimeTimerHandle;
	FTimerHandle DistantEnemyCheckTimerHandle;
	FTimerHandle SpatialPressureTimerHandle;
	FTimerHandle EventMemberTimerHandle;
	float RunTimeSeconds = 0.0f;
	bool bRunTimeFrozen = false;
	bool bTrialSuspended = false;
	mutable int32 LastInvalidPhaseWarningSecond = INDEX_NONE;
	float NextGlobalEventTime = 0.0f;
	float DirectionalBiasAngleDegrees = 0.0f;
	float DirectionalBiasEndRunTime = 0.0f;
	int32 ActiveEventIndex = INDEX_NONE;
	int32 ActiveEventMemberIndex = 0;
	int32 ActiveEventSpawnedCount = 0;
	int32 ActiveEventFailedCount = 0;
	float ActiveEventDirectionAngle = 0.0f;
	float ActiveEventStartRunTime = 0.0f;
	int32 ActiveEventOverflowCap = 0;
	FName LastCompletedEventName = NAME_None;
	TArray<TSubclassOf<AEnemyBase>> ActiveEventMembers;

	ACharacterBase* GetActivePlayerCharacter() const;
	const FEnemyPressurePhase* ResolveActivePressurePhase(int32* OutPhaseIndex = nullptr, bool bLogWarnings = true) const;
	const FEnemySpawnEntry* FindSpawnDefinition(TSubclassOf<AEnemyBase> EnemyClass) const;
	const FEnemyPopulationPhaseEntry* ChoosePopulationDeficitEntry(const FEnemyPressurePhase& Phase) const;
	const FEnemyPopulationPhaseEntry* ChooseTimedThreatEntry(const FEnemyPressurePhase& Phase) const;
	const FEnemyPopulationPhaseEntry* ChooseDirectorEntry(const FEnemyPressurePhase& Phase) const;
	AEnemyBase* SpawnEnemyFromEntry(const FEnemySpawnEntry& SpawnEntry);
	AEnemyBase* SpawnEventEnemy(TSubclassOf<AEnemyBase> EnemyClass, int32 ClassOverflowAllowance);
	AEnemyBase* SpawnSpatialPressureReplacement(const TArray<int32>& SectorCounts, int32& OutSectorIndex);
	bool FindSpawnLocation(const FVector& ActivePlayerLocation, TSubclassOf<AEnemyBase> EnemyClass, FVector& OutSpawnLocation, bool bUseDirectionalArc = false, float DirectionAngleDegrees = 0.0f, float ArcDegrees = 360.0f, float DistanceMin = 0.0f, float DistanceMax = 0.0f) const;
	bool GenerateCandidateSpawnLocation(const FVector& ActivePlayerLocation, int32 AttemptIndex, int32 AttemptCount, FVector& OutCandidateLocation, bool bUseDirectionalArc = false, float DirectionAngleDegrees = 0.0f, float ArcDegrees = 360.0f, float DistanceMin = 0.0f, float DistanceMax = 0.0f) const;
	bool ProjectSpawnLocationToNavigation(const FVector& CandidateLocation, FVector& OutProjectedLocation) const;
	bool IsSpawnLocationInsideArena(const FVector& Location) const;
	bool IsSpawnLocationCollisionFree(const FVector& Location, TSubclassOf<AEnemyBase> EnemyClass) const;
	void GetEnemyCapsuleDimensions(TSubclassOf<AEnemyBase> EnemyClass, float& OutRadius, float& OutHalfHeight) const;
	int32 GetAliveEnemyCount();
	float GetHealthMultiplier(const FEnemySpawnEntry& SpawnEntry) const;
	float GetDamageMultiplier() const;
	float GetSpawnPressure() const;
	float GetCurrentSpawnInterval() const;
	int32 GetCurrentEnemiesPerSpawn() const;
	int32 GetCurrentMaxAliveEnemies() const;
	int32 GetCurrentSpawnBudget() const;
	float EvaluateDefaultHealthMultiplier() const;
	float EvaluateDefaultDamageMultiplier() const;
	float EvaluateDefaultSpawnPressure() const;
	bool IsSpawnDefinitionUnlocked(const FEnemySpawnEntry& Entry) const;
	bool IsClassCooldownActive(TSubclassOf<AEnemyBase> EnemyClass, float* OutRemainingSeconds = nullptr) const;
	bool IsPopulationEntryEligible(const FEnemyPressurePhase& Phase, const FEnemyPopulationPhaseEntry& PopulationEntry) const;
	bool IsTimedThreatEntryEligible(const FEnemyPressurePhase& Phase, const FEnemyPopulationPhaseEntry& PopulationEntry) const;
	int32 GetTotalDesiredPopulation(const FEnemyPressurePhase& Phase) const;
	float GetPopulationRatio(const FEnemyPressurePhase& Phase) const;
	int32 GetCurrentPopulationBatchSize(const FEnemyPressurePhase& Phase) const;
	int32 GetAliveCountForSpawnClass(TSubclassOf<AEnemyBase> EnemyClass) const;
	void IncrementAliveCountForSpawnedEnemy(AEnemyBase* SpawnedEnemy, TSubclassOf<AEnemyBase> SpawnClass);
	bool UnregisterLivingEnemy(AEnemyBase* Enemy);
	void PruneTrackedEnemies();
	void TrackEnemySpawnMetadata(AEnemyBase* Enemy, bool bEventMember);
	bool RecycleLivingEnemy(AEnemyBase* Enemy);
	void EvaluateSpatialPressure(bool bAllowRecycling, bool bForceLog);
	int32 ChooseReplacementSector(const TArray<int32>& SectorCounts) const;
	void DrawSpatialPressureDebug(const ACharacterBase* Player, const FVector& MovementDirection, const TArray<int32>& SectorCounts, const TArray<AEnemyBase*>& StaleEnemies) const;
	void ApplyEnemySpawnModifierContexts(AEnemyBase* SpawnedEnemy);
	void UpdateDirectionalBias();
	void UpdateEventScheduler(const FEnemyPressurePhase* ActivePhase);
	bool IsEventSelectable(const FEnemyPressureEventDefinition& Event, const FEnemyPressurePhase& Phase, FString* OutReason = nullptr, bool bIgnoreEventSchedule = false) const;
	void StartPressureEvent(int32 EventIndex, const FEnemyPressurePhase& Phase);
	void EmitNextEventMember();
	void CompleteActiveEvent();
	void HandleRunTimeTimerElapsed();
	void HandleSpawnTimerElapsed();
	void HandleDistantEnemyCheckTimerElapsed();
	void HandleSpatialPressureTimerElapsed();
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
