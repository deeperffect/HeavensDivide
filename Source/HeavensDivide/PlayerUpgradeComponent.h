// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UpgradeDefinition.h"
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
	int32 GetUpgradeLevelById(FName UpgradeId) const;

	UFUNCTION(BlueprintPure, Category = "Upgrades")
	bool HasUpgradeId(FName UpgradeId) const;

	UFUNCTION(BlueprintPure, Category = "Upgrades")
	bool CanAcquireUpgrade(UUpgradeDefinition* Upgrade) const;

	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	bool AcquireUpgrade(UUpgradeDefinition* Upgrade);

	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	void RebuildAllUpgradeModifiers();

	UFUNCTION(BlueprintCallable, Category = "Upgrades|Debug")
	void LogPlayerUpgradeStats() const;

	UFUNCTION(BlueprintPure, Category = "Upgrades|Selection")
	bool IsCategoryUnlocked(EUpgradeCategory Category) const;

	UFUNCTION(BlueprintPure, Category = "Upgrades|Selection")
	TArray<EUpgradeCategory> GetEligibleCategories() const;

	UFUNCTION(BlueprintCallable, Category = "Upgrades|Selection")
	TArray<EUpgradeCategory> RollCategoryChoices(int32 ChoiceCount = 2);

	UFUNCTION(BlueprintPure, Category = "Upgrades|Selection")
	TArray<UUpgradeDefinition*> GetEligibleUpgradesForCategory(EUpgradeCategory Category) const;

	UFUNCTION(BlueprintCallable, Category = "Upgrades|Selection")
	TArray<UUpgradeDefinition*> RollUpgradeChoices(EUpgradeCategory Category, int32 ChoiceCount = 3) const;

	UFUNCTION(BlueprintCallable, Category = "Upgrades|Selection")
	bool BeginUpgradeSelection(int32 CategoryChoiceCount = 2);

	UFUNCTION(BlueprintCallable, Category = "Upgrades|Selection")
	bool SelectCategory(EUpgradeCategory Category, int32 UpgradeChoiceCount = 3);

	UFUNCTION(BlueprintCallable, Category = "Upgrades|Selection")
	bool SelectUpgrade(UUpgradeDefinition* Upgrade);

	UFUNCTION(BlueprintPure, Category = "Upgrades|Selection")
	TArray<EUpgradeCategory> GetCurrentCategoryChoices() const;

	UFUNCTION(BlueprintPure, Category = "Upgrades|Selection")
	EUpgradeCategory GetSelectedCategory() const;

	UFUNCTION(BlueprintPure, Category = "Upgrades|Selection")
	TArray<UUpgradeDefinition*> GetCurrentUpgradeChoices() const;

	UFUNCTION(BlueprintCallable, Category = "Upgrades|Debug")
	bool DebugAcquireUpgrade(UUpgradeDefinition* Upgrade);

	UFUNCTION(BlueprintCallable, Category = "Upgrades|Debug")
	bool DebugForceAcquireUpgrade(UUpgradeDefinition* Upgrade, int32 Level = 1);

	UFUNCTION(BlueprintCallable, Category = "Upgrades|Debug")
	bool DebugBeginUpgradeSelection(int32 CategoryChoiceCount = 2);

	UFUNCTION(BlueprintCallable, Category = "Upgrades|Debug")
	bool DebugSelectCategory(EUpgradeCategory Category, int32 UpgradeChoiceCount = 3);

	UFUNCTION(BlueprintCallable, Category = "Upgrades|Debug")
	bool DebugSelectUpgrade(UUpgradeDefinition* Upgrade);

	UFUNCTION(BlueprintPure, Category = "Upgrades")
	TArray<UUpgradeDefinition*> GetEligibleUpgrades(const TArray<UUpgradeDefinition*>& CandidateUpgrades) const;

	UFUNCTION(BlueprintPure, Category = "Upgrades")
	int32 GetSpecialEffectLevel(EUpgradeSpecialEffect SpecialEffect) const;

	UPROPERTY(BlueprintAssignable, Category = "Upgrades")
	FOnUpgradeAcquired OnUpgradeAcquired;

	UPROPERTY(BlueprintAssignable, Category = "Upgrades")
	FOnUpgradeLevelChanged OnUpgradeLevelChanged;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrades")
	TArray<TObjectPtr<UUpgradeDefinition>> UpgradePool;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrades|Selection", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CategoryBadLuckWeightPerMiss = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrades|Selection", meta = (ClampMin = "1", UIMin = "1"))
	int32 SynergyUnlockLevel = 5;

private:
	float GetBaseCategoryWeight(EUpgradeCategory Category) const;
	float GetCategoryRollWeight(EUpgradeCategory Category) const;
	int32 GetCurrentPlayerLevel() const;
	bool HasAcquiredUpgradeInCategory(EUpgradeCategory Category) const;
	EUpgradeCategory PickWeightedCategory(const TArray<EUpgradeCategory>& Categories, const TArray<float>& Weights) const;
	void UpdateCategoryBadLuckHistory(const TArray<EUpgradeCategory>& OfferedCategories);
	void ClearCurrentOffer();
	FString CategoryToString(EUpgradeCategory Category) const;
	FString UpgradeToLogString(const UUpgradeDefinition* Upgrade) const;
	bool IsValidUpgradeDefinition(const UUpgradeDefinition* Upgrade) const;
	bool MeetsPrerequisites(const UUpgradeDefinition* Upgrade) const;
	void RebuildUpgradeModifiers(UUpgradeDefinition* Upgrade, int32 NewLevel);
	void ClearUpgradeModifiers(UUpgradeDefinition* Upgrade);
	FName MakeUpgradeModifierSourceId(const UUpgradeDefinition* Upgrade) const;
	FName MakeUpgradeModifierId(const UUpgradeDefinition* Upgrade, int32 ModifierIndex) const;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Upgrades", meta = (AllowPrivateAccess = "true"))
	TMap<FName, int32> UpgradeLevels;

	UPROPERTY()
	TMap<FName, TObjectPtr<UUpgradeDefinition>> AcquiredUpgradeDefinitions;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Upgrades|Selection", meta = (AllowPrivateAccess = "true"))
	TArray<EUpgradeCategory> CurrentCategoryChoices;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Upgrades|Selection", meta = (AllowPrivateAccess = "true"))
	EUpgradeCategory SelectedCategory = EUpgradeCategory::Global;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Upgrades|Selection", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UUpgradeDefinition>> CurrentUpgradeChoices;

	UPROPERTY()
	TMap<EUpgradeCategory, int32> CategoryRollsSinceLastOffered;

	bool bHasSelectedCategory = false;
};
