// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterStatsComponent.generated.h"

UENUM(BlueprintType)
enum class ECharacterStatType : uint8
{
	DamageMultiplier,
	AttackSpeedMultiplier,
	AttackAreaMultiplier,
	ProjectileCountBonus,
	ProjectileSpeedMultiplier,
	HomingStrengthMultiplier,
	HPRegenPerSecond,
	DamageReduction,
	HealthOnKill,
	DodgeChance,
	ProjectilePierceBonus
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FName ModifierId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FName SourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	ECharacterStatType Stat = ECharacterStatType::DamageMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	EStatModifierOperation Operation = EStatModifierOperation::AddFlat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
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

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnCharacterStatsChanged OnStatsChanged;

private:
	float GetBaseStat(ECharacterStatType Stat) const;

	UPROPERTY()
	TArray<FCharacterStatModifier> Modifiers;
};
