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

USTRUCT(BlueprintType)
struct FUpgradeRarityMagnitude
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Rarity")
	EUpgradeRarity Rarity = EUpgradeRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Rarity")
	float Magnitude = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Rarity", meta = (MultiLine = true))
	FText DescriptionOverride;
};

UENUM(BlueprintType)
enum class EUpgradeInvestmentOwner : uint8
{
	None,
	Samurai,
	Ninja
};

UENUM(BlueprintType)
enum class EUpgradeRole : uint8
{
	Stat,
	Starter,
	Support,
	Evolution,
	Mechanic,
	Special
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
	// Value 6 was retired. Explicit values preserve existing serialized upgrade assets.
	FanOfBlades = 7,
	BladeCascade = 8,
	SamuraiCleaver = 9,
	SamuraiDuelist = 10,
	SamuraiDeathblow = 11,
	HemotoxicReaction = 12,
	VirulentStrain = 13,
	AcceleratedVenom = 14,
	ShadowStep = 15,
	AfterimageFrenzy = 16
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

USTRUCT(BlueprintType)
struct FUpgradePrerequisiteRequirement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Prerequisites")
	FName UpgradeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Prerequisites", meta = (ClampMin = "1", UIMin = "1"))
	int32 MinimumLevel = 1;
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Taxonomy")
	EUpgradeInvestmentOwner InvestmentOwner = EUpgradeInvestmentOwner::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Taxonomy")
	EUpgradeRole Role = EUpgradeRole::Stat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Taxonomy")
	FName BuildFamilyId = NAME_None;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Rarity")
	bool bUsesRolledRarity = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Rarity", meta = (EditCondition = "bUsesRolledRarity", EditConditionHides))
	TArray<FUpgradeRarityMagnitude> RarityMagnitudes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Rarity", meta = (EditCondition = "bUsesRolledRarity", EditConditionHides, ToolTip = "Offer-specific description. Use {Magnitude} for the resolved numeric value and {Percent} for the value multiplied by 100."))
	FText RolledDescriptionFormat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Prerequisites")
	TArray<FName> PrerequisiteUpgradeIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Prerequisites")
	TArray<FUpgradePrerequisiteRequirement> PrerequisiteRequirements;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<FUpgradeStatModifierDefinition> StatModifiers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<EUpgradeSpecialEffect> SpecialEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Exclusivity", meta = (ToolTip = "Optional run-local exclusivity group. Once one upgrade in this group is acquired, other upgrades in the same group become ineligible."))
	FName ExclusivityGroup = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Blade Cascade", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Number of fired kunai required before Blade Cascade grants its bonus kunai."))
	int32 BladeCascadeKunaiThreshold = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Blade Cascade", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Number of bonus kunai granted when Blade Cascade triggers."))
	int32 BladeCascadeBonusKunai = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Handoff", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Temporary attack speed multiplier bonus applied by Handoff after swapping. 0.4 means +40%."))
	float HandoffAttackSpeedBonus = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Handoff", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Duration in seconds for the temporary Handoff attack speed bonus after swapping."))
	float HandoffDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Hemotoxic Reaction", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HemotoxicReactionMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Hemotoxic Reaction", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float HemotoxicReactionRadius = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Virulent Strain", meta = (ClampMin = "1", UIMin = "1"))
	int32 VirulentStrainThreshold = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Virulent Strain", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float VirulentStrainRadius = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Virulent Strain", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VirulentStrainDamageMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Accelerated Venom", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AcceleratedVenomTickRateMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|Afterimage Frenzy", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AfterimageFrenzyAttackSpeedBonus = 2.0f;
};
