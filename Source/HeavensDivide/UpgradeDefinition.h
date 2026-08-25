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
	Synergy,
	Cursed UMETA(DisplayName = "Blood Pact"),
	NinjaTrial UMETA(DisplayName = "Ninja Technique"),
	SamuraiTrial UMETA(DisplayName = "Samurai Technique")
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
	QuickHandoff,
	Handoff,
	DoubleCut,
	Momentum,
	FanOfBlades,
	BladeCascade,
	SamuraiCleaver,
	SamuraiDuelist,
	SamuraiDeathblow
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Meta Progression", meta = (EditCondition = "Category == EUpgradeCategory::Synergy", EditConditionHides))
	FName MetaUnlockId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Meta Progression", meta = (EditCondition = "Category == EUpgradeCategory::Synergy", EditConditionHides))
	bool bRequiresMetaUnlock = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Meta Progression", meta = (EditCondition = "Category == EUpgradeCategory::Synergy && bRequiresMetaUnlock", EditConditionHides))
	bool bUnlockedByDefault = false;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Exclusivity", meta = (ToolTip = "Optional run-local exclusivity group. Once one upgrade in this group is acquired, other upgrades in the same group become ineligible."))
	FName ExclusivityGroup = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Momentum", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Kills required from a Samurai melee attack before the Momentum upgrade reduces the next attack cooldown."))
	int32 MomentumRequiredKills = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Momentum", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", ToolTip = "Fraction of remaining attack cooldown removed when Momentum triggers. 0.4 means reduce remaining cooldown by 40%."))
	float MomentumRemainingCooldownReduction = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Blade Cascade", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Number of fired kunai required before Blade Cascade grants its bonus kunai."))
	int32 BladeCascadeKunaiThreshold = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Blade Cascade", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Number of bonus kunai granted when Blade Cascade triggers."))
	int32 BladeCascadeBonusKunai = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Handoff", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Temporary attack speed multiplier bonus applied by Handoff after swapping. 0.4 means +40%."))
	float HandoffAttackSpeedBonus = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Handoff", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Duration in seconds for the temporary Handoff attack speed bonus after swapping."))
	float HandoffDuration = 3.0f;
};
