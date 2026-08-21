// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CharacterStatsComponent.h"
#include "Components/ActorComponent.h"
#include "SharedPlayerStatsComponent.generated.h"

UENUM(BlueprintType)
enum class ESharedPlayerStatType : uint8
{
	MoveSpeedMultiplier UMETA(ToolTip = "Shared movement speed multiplier applied to both player characters. Add Flat 0.10 means +10% move speed."),
	MaxHealthMultiplier UMETA(ToolTip = "Shared max health multiplier applied to the player health pool. Add Flat 0.10 means +10% max health."),
	PickupRadiusMultiplier UMETA(ToolTip = "Shared XP pickup radius multiplier. Add Flat 0.10 means +10% pickup radius."),
	DamageMultiplier UMETA(ToolTip = "Shared outgoing damage multiplier for both player characters. Add Flat 0.10 means +10% damage."),
	AttackSpeedMultiplier UMETA(ToolTip = "Shared attack speed multiplier for both player characters. Add Flat 0.10 means +10% attack speed."),
	MaxDashCharges UMETA(ToolTip = "Adds shared maximum dash charges. Add Flat 1 means +1 max dash charge.")
};

USTRUCT(BlueprintType)
struct FSharedPlayerStatModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ToolTip = "Unique id for this modifier instance. Modifiers with the same id replace each other."))
	FName ModifierId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ToolTip = "Source that granted this modifier, usually an upgrade id. Used when removing all modifiers from that source."))
	FName SourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ToolTip = "Shared player stat modified by this entry."))
	ESharedPlayerStatType Stat = ESharedPlayerStatType::MoveSpeedMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ToolTip = "How Value is applied. Add Flat adds the value directly; Add Percent adds a percentage of the stat's base value."))
	EStatModifierOperation Operation = EStatModifierOperation::AddFlat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ToolTip = "Amount applied by this modifier. For multiplier stats, 0.10 usually means +10%; for Max Dash Charges, 1 means +1 charge."))
	float Value = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSharedPlayerStatsChanged);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API USharedPlayerStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USharedPlayerStatsComponent();

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void AddModifier(const FSharedPlayerStatModifier& Modifier);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	bool RemoveModifier(FName ModifierId);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ClearModifiersFromSource(FName SourceId);

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalStat(ESharedPlayerStatType Stat) const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalMoveSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalMaxHealthMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalPickupRadiusMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalDamageMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFinalAttackSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	int32 GetFinalMaxDashCharges() const;

	UPROPERTY(BlueprintAssignable, Category = "Stats", meta = (ToolTip = "Broadcast whenever shared player stat modifiers change."))
	FOnSharedPlayerStatsChanged OnStatsChanged;

private:
	float GetBaseStat(ESharedPlayerStatType Stat) const;

	UPROPERTY()
	TArray<FSharedPlayerStatModifier> Modifiers;
};
