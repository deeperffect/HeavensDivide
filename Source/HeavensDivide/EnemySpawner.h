// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class ACharacterBase;
class AEnemyBase;
class AEnemySpawnArea;
class UCurveFloat;

USTRUCT(BlueprintType)
struct FEnemySpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning")
	TSubclassOf<AEnemyBase> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpawnWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinimumRunTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaximumRunTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "1", UIMin = "1"))
	int32 SpawnCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning")
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

	UFUNCTION(BlueprintPure, Category = "Run")
	float GetRunTimeSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Run")
	float GetRunTimeMinutes() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Pool")
	TArray<FEnemySpawnEntry> EnemySpawnEntries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float BaseSpawnInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float MinSpawnInterval = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1", UIMin = "1"))
	int32 BaseEnemiesPerSpawn = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxEnemiesPerSpawn = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1", UIMin = "1"))
	int32 BaseMaxAliveEnemies = 60;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaximumAliveEnemies = 200;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1", UIMin = "1"))
	int32 BaseSpawnBudget = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxSpawnBudget = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinSpawnDistance = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxSpawnDistance = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning")
	bool bSpawningEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Scaling")
	TObjectPtr<UCurveFloat> HealthMultiplierCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Scaling")
	TObjectPtr<UCurveFloat> DamageMultiplierCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Scaling")
	TObjectPtr<UCurveFloat> SpawnPressureCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Scaling", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float RunTimeUpdateInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Performance", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxEnemyDistanceFromPlayer = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Performance", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float DistantEnemyCheckInterval = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Performance", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxDistantDespawnsPerCheck = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Ground Trace", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float GroundTraceHeight = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Ground Trace", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float GroundTraceDepth = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation")
	TObjectPtr<AEnemySpawnArea> ActiveSpawnArea;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxSpawnLocationAttempts = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpawnEdgePadding = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation")
	FVector NavMeshProjectionExtent = FVector(250.0f, 250.0f, 500.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation")
	bool bRequireNavMeshProjection = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation")
	bool bUseSpawnAreaFallbackCandidates = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float FallbackCapsuleRadius = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float FallbackCapsuleHalfHeight = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Validation", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CollisionValidationGroundClearance = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Debug")
	bool bDebugSpawning = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning|Debug")
	bool bDebugSpawnValidation = false;

	UPROPERTY()
	TArray<TWeakObjectPtr<AEnemyBase>> SpawnedEnemies;

	FTimerHandle SpawnTimerHandle;
	FTimerHandle RunTimeTimerHandle;
	FTimerHandle DistantEnemyCheckTimerHandle;
	float RunTimeSeconds = 0.0f;

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
	void PruneTrackedEnemies();
	void HandleRunTimeTimerElapsed();
	void HandleSpawnTimerElapsed();
	void HandleDistantEnemyCheckTimerElapsed();
	void RescheduleSpawnTimer();
	void LogDirectorStatus() const;

	UFUNCTION()
	void HandleSpawnedEnemyDestroyed(AActor* DestroyedActor);
};
