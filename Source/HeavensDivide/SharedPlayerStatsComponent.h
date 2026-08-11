// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CharacterStatsComponent.h"
#include "Components/ActorComponent.h"
#include "SharedPlayerStatsComponent.generated.h"

UENUM(BlueprintType)
enum class ESharedPlayerStatType : uint8
{
	MoveSpeedMultiplier,
	MaxHealthMultiplier,
	PickupRadiusMultiplier,
	DamageMultiplier
};

USTRUCT(BlueprintType)
struct FSharedPlayerStatModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FName ModifierId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FName SourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	ESharedPlayerStatType Stat = ESharedPlayerStatType::MoveSpeedMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	EStatModifierOperation Operation = EStatModifierOperation::AddFlat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
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

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnSharedPlayerStatsChanged OnStatsChanged;

private:
	float GetBaseStat(ESharedPlayerStatType Stat) const;

	UPROPERTY()
	TArray<FSharedPlayerStatModifier> Modifiers;
};
