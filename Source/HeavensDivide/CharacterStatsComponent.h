// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterStatsComponent.generated.h"

UENUM(BlueprintType)
enum class ECharacterStatType : uint8
{
	DamageMultiplier UMETA(ToolTip = "Multiplies this character's outgoing damage. Add Flat 0.10 means +10%."),
	AttackSpeedMultiplier UMETA(ToolTip = "Multiplies this character's attack speed by reducing effective attack interval. Add Flat 0.10 means +10% attack speed."),
	AttackAreaMultiplier UMETA(ToolTip = "Multiplies this character's melee attack area. Add Flat 0.10 means +10% area scale."),
	ProjectileCountBonus UMETA(ToolTip = "Adds extra projectiles to this character's projectile attack."),
	ProjectileSpeedMultiplier UMETA(ToolTip = "Multiplies this character's projectile speed. Add Flat 0.10 means +10% speed."),
	HomingStrengthMultiplier UMETA(ToolTip = "Obsolete reserved stat from the removed homing projectile behavior. Do not use for current straight-line kunai."),
	HPRegenPerSecond UMETA(ToolTip = "Adds passive healing per second for this character while alive. Add Flat 0.5 means heal 0.5 HP per second."),
	DamageReduction UMETA(ToolTip = "Reduces incoming damage for this character. Add Flat 0.10 means 10% less damage taken."),
	HealthOnKill UMETA(ToolTip = "Heals this character when their attack kills enemies. Add Flat 1.0 means heal 1 HP per kill."),
	DodgeChance UMETA(ToolTip = "Chance for this character to avoid incoming damage. Add Flat 0.10 means 10% dodge chance."),
	ProjectilePierceBonus UMETA(ToolTip = "Adds extra enemies this character's projectiles can pierce before ending."),
	ProjectileBounceBonus UMETA(ToolTip = "Adds projectile retargets after a projectile would otherwise end."),
	ProjectileSplitBonus UMETA(ToolTip = "Enables one projectile split per original projectile.")
};

UENUM(BlueprintType)
enum class EStatModifierOperation : uint8
{
	AddFlat,
	AddPercent
};

USTRUCT(BlueprintType)
struct FCharacterStatModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ToolTip = "Unique id for this modifier instance. Modifiers with the same id replace each other."))
	FName ModifierId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ToolTip = "Source that granted this modifier, usually an upgrade id. Used when removing all modifiers from that source."))
	FName SourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ToolTip = "Character stat modified by this entry."))
	ECharacterStatType Stat = ECharacterStatType::DamageMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ToolTip = "How Value is applied. Add Flat adds the value directly; Add Percent adds a percentage of the stat's base value."))
	EStatModifierOperation Operation = EStatModifierOperation::AddFlat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ToolTip = "Amount applied by this modifier. For multiplier stats, 0.10 usually means +10%."))
	float Value = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterStatsChanged);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API UCharacterStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterStatsComponent();

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void AddModifier(const FCharacterStatModifier& Modifier);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	bool RemoveModifier(FName ModifierId);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ClearModifiersFromSource(FName SourceId);

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalStat(ECharacterStatType Stat) const;

	UFUNCTION(BlueprintPure, Category = "Stats|Debug")
	int32 GetModifierCount() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalDamageMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalAttackSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalAttackAreaMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	int32 GetFinalProjectileCount() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	int32 GetFinalProjectilePierceBonus() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	int32 GetFinalProjectileBounceBonus() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	int32 GetFinalProjectileSplitBonus() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalProjectileSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalHomingStrengthMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalHPRegenPerSecond() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalDamageReduction() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalHealthOnKill() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalDodgeChance() const;

	UPROPERTY(BlueprintAssignable, Category = "Stats", meta = (ToolTip = "Broadcast whenever this character's runtime stat modifiers change."))
	FOnCharacterStatsChanged OnStatsChanged;

private:
	float GetBaseStat(ECharacterStatType Stat) const;

	UPROPERTY()
	TArray<FCharacterStatModifier> Modifiers;
};
