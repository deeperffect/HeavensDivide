// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerUpgradeComponent.h"
#include "LevelUpWidget.generated.h"

class ASurvivorPlayerController;
class UBorder;
class UImage;
class UHorizontalBox;
class UMaterialInterface;
class UTextBlock;
class UTexture2D;
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
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Level Up")
	TObjectPtr<ASurvivorPlayerController> SurvivorPlayerController;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Level Up")
	TObjectPtr<UPlayerUpgradeComponent> PlayerUpgrades;

	/** Centralized category-frame artwork. Assign these once on WBP_LevelUp Class Defaults. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Up|Card Presentation")
	TObjectPtr<UTexture2D> SamuraiCardBorder;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Up|Card Presentation")
	TObjectPtr<UTexture2D> NinjaCardBorder;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Up|Card Presentation")
	TObjectPtr<UTexture2D> SynergyCardBorder;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Up|Card Presentation")
	TObjectPtr<UTexture2D> GlobalCardBorder;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Up|Rarity Presentation")
	TObjectPtr<UMaterialInterface> RareRarityMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Up|Rarity Presentation")
	TObjectPtr<UMaterialInterface> EpicRarityMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Up|Rarity Presentation")
	TObjectPtr<UMaterialInterface> LegendaryRarityMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Up|Category Presentation")
	TObjectPtr<UTexture2D> SamuraiCategoryArtwork;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Up|Category Presentation")
	TObjectPtr<UTexture2D> NinjaCategoryArtwork;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Up|Category Presentation")
	TObjectPtr<UTexture2D> SynergyCategoryArtwork;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Up|Category Presentation")
	TObjectPtr<UTexture2D> GlobalCategoryArtwork;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Up|Category Presentation")
	TObjectPtr<UTexture2D> SamuraiCategoryBorder;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Up|Category Presentation")
	TObjectPtr<UTexture2D> NinjaCategoryBorder;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Up|Category Presentation")
	TObjectPtr<UTexture2D> SynergyCategoryBorder;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Up|Category Presentation")
	TObjectPtr<UTexture2D> GlobalCategoryBorder;

private:
	void EnsureCategoryCardVisualStructure();
	void RefreshCategoryCardVisuals();
	UTexture2D* GetCategoryArtwork(EUpgradeCategory Category) const;
	UTexture2D* GetCategoryBorder(EUpgradeCategory Category) const;
	void EnsureUpgradeCardVisualStructure();
	void RefreshUpgradeCardVisuals();
	UTexture2D* GetCardBorderForCategory(EUpgradeCategory Category) const;
	UMaterialInterface* GetRarityMaterial(EUpgradeRarity Rarity) const;
	void EnsureSynergyDiscoveryPresentation();
	void InitializeControllerNavigation();
	void MoveControllerFocus(int32 Direction);
	void RefreshControllerFocus();
	void RebuildFocusableChoices();
	void RefreshChoiceScales();
	void RefreshMouseHoveredButton();
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
	bool bHasControllerSelection = false;
	bool bControllerAnalogNavigationHeld = false;

	UPROPERTY(Transient)
	TObjectPtr<class UButton> MouseHoveredButton;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UButton>> FocusableChoiceButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> UpgradeArtworkImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> UpgradeBorderImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> UpgradeRarityGlowImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> UpgradeTitleTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> UpgradeDescriptionTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHorizontalBox>> UpgradeLevelDiamondRows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CategoryArtworkImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CategoryArtworkCoverImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CategoryBorderImages;
};
