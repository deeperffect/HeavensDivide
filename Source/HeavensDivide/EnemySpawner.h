// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class ACharacterBase;
class AEnemyBase;

USTRUCT(BlueprintType)
struct FEnemySpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning")
	TSubclassOf<AEnemyBase> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpawnWeight = 1.0f;

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
	AEnemyBase* SpawnEnemy();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning")
	TArray<FEnemySpawnEntry> EnemySpawnEntries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float SpawnInterval = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinSpawnRadius = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxSpawnRadius = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxAliveEnemies = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning")
	bool bSpawningEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning|Ground Trace", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float GroundTraceHeight = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning|Ground Trace", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float GroundTraceDepth = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawning|Debug")
	bool bDebugSpawning = false;

	UPROPERTY()
	TArray<TWeakObjectPtr<AEnemyBase>> SpawnedEnemies;

	FTimerHandle SpawnTimerHandle;

	ACharacterBase* GetActivePlayerCharacter() const;
	const FEnemySpawnEntry* ChooseSpawnEntry() const;
	bool FindSpawnLocation(const FVector& ActivePlayerLocation, FVector& OutSpawnLocation) const;
	int32 GetAliveEnemyCount();
	void PruneTrackedEnemies();
	void HandleSpawnTimerElapsed();

	UFUNCTION()
	void HandleSpawnedEnemyDestroyed(AActor* DestroyedActor);
};
