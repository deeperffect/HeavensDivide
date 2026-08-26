// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerUpgradeComponent.h"
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

	UFUNCTION(BlueprintImplementableEvent, Category = "Level Up|Rarity")
	void ShowUpgradeOffers(const TArray<FUpgradeOffer>& UpgradeOffers);

	UFUNCTION(BlueprintPure, Category = "Level Up|Rarity")
	bool GetOfferForUpgrade(UUpgradeDefinition* Upgrade, FUpgradeOffer& OutOffer) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Level Up")
	void SetSynergyDiscoveryPresentation(bool bIsDiscovery);
	virtual void SetSynergyDiscoveryPresentation_Implementation(bool bIsDiscovery);

	UPROPERTY(BlueprintAssignable, Category = "Level Up")
	FOnLevelUpWidgetSelectionCompleted OnSelectionCompleted;

	UFUNCTION(BlueprintImplementableEvent, Category = "Level Up|Controller")
	void SetControllerFocusPresentation(int32 ChoiceIndex, bool bShowingCategoryChoices);

protected:
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogInputEvent) override;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Level Up")
	TObjectPtr<ASurvivorPlayerController> SurvivorPlayerController;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Level Up")
	TObjectPtr<UPlayerUpgradeComponent> PlayerUpgrades;

private:
	void EnsureSynergyDiscoveryPresentation();
	void InitializeControllerNavigation();
	void MoveControllerFocus(int32 Direction);
	void RefreshControllerFocus();
	void RebuildFocusableChoices();
	FReply HandleControllerKey(const FKeyEvent& InKeyEvent);
	int32 GetVisibleChoiceCount() const;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> SynergyDiscoveryBanner;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SynergyDiscoveryTitle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SynergyDiscoverySubtitle;

	bool bCategoryChoiceCommitted = false;
	bool bUpgradeChoiceCommitted = false;
	bool bSynergyDiscoveryMode = false;
	int32 ControllerFocusedChoiceIndex = 0;
	bool bControllerAnalogNavigationHeld = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UButton>> FocusableChoiceButtons;
};
