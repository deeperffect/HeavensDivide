// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawnArea.generated.h"

class UBoxComponent;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AEnemySpawnArea : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawnArea();

	UFUNCTION(BlueprintPure, Category = "Enemy Spawning")
	bool ContainsSpawnLocation(const FVector& WorldLocation, float EdgePadding = 0.0f) const;

	UFUNCTION(BlueprintPure, Category = "Enemy Spawning")
	FVector GetRandomLocationInside(float EdgePadding = 0.0f) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> SpawnBounds;
};
