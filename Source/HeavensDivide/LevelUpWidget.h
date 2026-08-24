// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UpgradeDefinition.h"
#include "LevelUpWidget.generated.h"

class ASurvivorPlayerController;
class UBorder;
class UTextBlock;
class UPlayerUpgradeComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLevelUpWidgetSelectionCompleted);

UCLASS(BlueprintType, Blueprintable)
class HEAVENSDIVIDE_API ULevelUpWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Level Up")
	void InitializeLevelUpWidget(ASurvivorPlayerController* InPlayerController);

	UFUNCTION(BlueprintCallable, Category = "Level Up")
	void InitializeDirectUpgradeWidget(ASurvivorPlayerController* InPlayerController);

	UFUNCTION(BlueprintCallable, Category = "Level Up")
	void InitializeSynergyDiscoveryWidget(ASurvivorPlayerController* InPlayerController);

	UFUNCTION(BlueprintCallable, Category = "Level Up")
	void RefreshCategoryChoices();

	UFUNCTION(BlueprintCallable, Category = "Level Up")
	bool SelectCategoryChoice(int32 ChoiceIndex);

	UFUNCTION(BlueprintCallable, Category = "Level Up")
	bool SelectUpgradeChoice(int32 ChoiceIndex);

	UFUNCTION(BlueprintPure, Category = "Level Up")
	int32 GetUpgradeLevel(UUpgradeDefinition* Upgrade) const;

	UFUNCTION(BlueprintPure, Category = "Level Up")
	UPlayerUpgradeComponent* GetPlayerUpgrades() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Level Up")
	void ShowCategoryChoices(const TArray<EUpgradeCategory>& CategoryChoices);

	UFUNCTION(BlueprintImplementableEvent, Category = "Level Up")
	void ShowUpgradeChoices(const TArray<UUpgradeDefinition*>& UpgradeChoices);

	UFUNCTION(BlueprintNativeEvent, Category = "Level Up")
	void SetSynergyDiscoveryPresentation(bool bIsDiscovery);
	virtual void SetSynergyDiscoveryPresentation_Implementation(bool bIsDiscovery);

	UPROPERTY(BlueprintAssignable, Category = "Level Up")
	FOnLevelUpWidgetSelectionCompleted OnSelectionCompleted;

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Level Up")
	TObjectPtr<ASurvivorPlayerController> SurvivorPlayerController;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Level Up")
	TObjectPtr<UPlayerUpgradeComponent> PlayerUpgrades;

private:
	void EnsureSynergyDiscoveryPresentation();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> SynergyDiscoveryBanner;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SynergyDiscoveryTitle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SynergyDiscoverySubtitle;

	bool bCategoryChoiceCommitted = false;
	bool bUpgradeChoiceCommitted = false;
	bool bSynergyDiscoveryMode = false;
};
