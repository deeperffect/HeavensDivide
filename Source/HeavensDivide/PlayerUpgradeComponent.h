// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerUpgradeComponent.generated.h"

class UUpgradeDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUpgradeAcquired, UUpgradeDefinition*, Upgrade, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUpgradeLevelChanged, UUpgradeDefinition*, Upgrade, int32, NewLevel);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API UPlayerUpgradeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerUpgradeComponent();

	UFUNCTION(BlueprintPure, Category = "Upgrades")
	int32 GetUpgradeLevel(UUpgradeDefinition* Upgrade) const;

	UFUNCTION(BlueprintPure, Category = "Upgrades")
	bool CanAcquireUpgrade(UUpgradeDefinition* Upgrade) const;

	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	bool AcquireUpgrade(UUpgradeDefinition* Upgrade);

	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	void RebuildAllUpgradeModifiers();

	UFUNCTION(BlueprintCallable, Category = "Upgrades|Debug")
	bool DebugAcquireUpgrade(UUpgradeDefinition* Upgrade);

	UFUNCTION(BlueprintPure, Category = "Upgrades")
	TArray<UUpgradeDefinition*> GetEligibleUpgrades(const TArray<UUpgradeDefinition*>& CandidateUpgrades) const;

	UPROPERTY(BlueprintAssignable, Category = "Upgrades")
	FOnUpgradeAcquired OnUpgradeAcquired;

	UPROPERTY(BlueprintAssignable, Category = "Upgrades")
	FOnUpgradeLevelChanged OnUpgradeLevelChanged;

private:
	bool IsValidUpgradeDefinition(const UUpgradeDefinition* Upgrade) const;
	void RebuildUpgradeModifiers(UUpgradeDefinition* Upgrade, int32 NewLevel);
	void ClearUpgradeModifiers(UUpgradeDefinition* Upgrade);
	FName MakeUpgradeModifierSourceId(const UUpgradeDefinition* Upgrade) const;
	FName MakeUpgradeModifierId(const UUpgradeDefinition* Upgrade, int32 ModifierIndex) const;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Upgrades", meta = (AllowPrivateAccess = "true"))
	TMap<FName, int32> UpgradeLevels;

	UPROPERTY()
	TMap<FName, TObjectPtr<UUpgradeDefinition>> AcquiredUpgradeDefinitions;
};
