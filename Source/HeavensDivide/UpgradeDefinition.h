// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CharacterStatsComponent.h"
#include "Engine/DataAsset.h"
#include "SharedPlayerStatsComponent.h"
#include "UpgradeDefinition.generated.h"

UENUM(BlueprintType)
enum class EUpgradeCategory : uint8
{
	Samurai,
	Ninja,
	Global,
	Synergy
};

UENUM(BlueprintType)
enum class EUpgradeRarity : uint8
{
	Common,
	Rare,
	Epic
};

UENUM(BlueprintType)
enum class EUpgradeStatTarget : uint8
{
	Samurai,
	Ninja,
	SharedPlayer
};

UENUM(BlueprintType)
enum class EUpgradeSpecialEffect : uint8
{
	None,
	SwapRestoresDashCharge,
	InactiveCharacterAssist,
	DoubleCut,
	Momentum,
	FanOfBlades
};

USTRUCT(BlueprintType)
struct FUpgradeStatModifierDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade")
	EUpgradeStatTarget Target = EUpgradeStatTarget::Samurai;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade", meta = (EditCondition = "Target != EUpgradeStatTarget::SharedPlayer", EditConditionHides))
	ECharacterStatType CharacterStat = ECharacterStatType::DamageMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade", meta = (EditCondition = "Target == EUpgradeStatTarget::SharedPlayer", EditConditionHides))
	ESharedPlayerStatType SharedPlayerStat = ESharedPlayerStatType::MoveSpeedMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade")
	EStatModifierOperation Operation = EStatModifierOperation::AddPercent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade")
	float ValuePerLevel = 0.0f;
};

UCLASS(BlueprintType)
class HEAVENSDIVIDE_API UUpgradeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	FName UpgradeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	EUpgradeCategory Category = EUpgradeCategory::Global;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	EUpgradeRarity Rarity = EUpgradeRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Prerequisites")
	TArray<FName> PrerequisiteUpgradeIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<FUpgradeStatModifierDefinition> StatModifiers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<EUpgradeSpecialEffect> SpecialEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Momentum", meta = (ClampMin = "1", UIMin = "1"))
	int32 MomentumRequiredKills = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Momentum", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MomentumRemainingCooldownReduction = 0.4f;
};
