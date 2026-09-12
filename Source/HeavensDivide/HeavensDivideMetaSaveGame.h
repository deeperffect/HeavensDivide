// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "HeavensDivideMetaSaveGame.generated.h"

UCLASS()
class HEAVENSDIVIDE_API UHeavensDivideMetaSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meta Progression")
	int32 SaveVersion = 3;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meta Progression")
	TArray<FName> UnlockedSynergyUpgradeIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meta Progression")
	int32 TwinSoulCompletionsTowardDiscovery = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meta Progression|Skill Tree")
	int32 SoulEmbers = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meta Progression|Skill Tree")
	TMap<FName, int32> SkillRanks;
	/** Actual paid cost, independent of future catalog price changes. */
	UPROPERTY() int32 SkillEmbersSpent = 0;
};
