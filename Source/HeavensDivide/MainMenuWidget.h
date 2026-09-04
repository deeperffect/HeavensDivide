// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "MainMenuWidget.generated.h"

class UCheckBox;
class UImage;
class UMaterialInterface;
class UMediaPlayer;
class UMediaSource;
class UMediaTexture;
class UTextBlock;
class UTexture2D;
class UUniformGridPanel;
class UWidgetSwitcher;
class UUpgradeDefinition;
class UMainMenuWidget;
enum class EUpgradeCategory : uint8;

UCLASS()
class HEAVENSDIVIDE_API UCollectionUpgradeTileButton : public UButton
{
	GENERATED_BODY()

public:
	void InitializeCollectionTile(UMainMenuWidget* InOwner, UUpgradeDefinition* InDefinition);

private:
	UFUNCTION() void HandleTileClicked();
	UFUNCTION() void HandleTileHovered();
	UPROPERTY(Transient) TObjectPtr<UMainMenuWidget> CollectionOwner;
	UPROPERTY(Transient) TObjectPtr<UUpgradeDefinition> Definition;
};

UCLASS(BlueprintType, Blueprintable)
class HEAVENSDIVIDE_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Main Menu")
	void ShowMainPanel();

	UFUNCTION(BlueprintCallable, Category = "Main Menu")
	void ShowCollectionPanel();

	UFUNCTION(BlueprintCallable, Category = "Main Menu")
	void ShowSettingsPanel();

	UFUNCTION(BlueprintCallable, Category = "Main Menu")
	void ShowResetConfirmation();

	UFUNCTION(BlueprintCallable, Category = "Main Menu|Collection")
	void RefreshCollection();
	void PreviewCollectionUpgrade(UUpgradeDefinition* Definition, bool bCommitSelection);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void BuildMenu();
	class UVerticalBox* BuildCollectionPanel();
	class UVerticalBox* BuildSettingsPanel();
	void RefreshAutoTargetingSetting();
	void RebuildCollectionGrid();
	void RefreshCollectionDetails();
	void RefreshCollectionTileVisuals();
	void SetCollectionVisible(bool bVisible);
	UButton* AddMenuButton(class UVerticalBox* Parent, const FText& Label, FName WidgetName);
	void SetResetConfirmationVisible(bool bVisible);
	void SetSettingsPopupVisible(bool bVisible);
	void FocusNamedWidget(FName WidgetName);
	void StartBackgroundMedia();
	void RefreshMenuEntryPresentation(float DeltaTime);

	UFUNCTION()
	void HandleBackgroundMediaOpened(FString OpenedUrl);

	/** Optional UI material used for the full-screen menu video. Takes priority over BackgroundMediaTexture. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Cinematic Background", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> BackgroundMediaMaterial;

	/** Optional media texture displayed when no background material is assigned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Cinematic Background", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMediaTexture> BackgroundMediaTexture;

	/** Media Player opened and looped when this menu is constructed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Cinematic Background", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMediaPlayer> BackgroundMediaPlayer;

	/** File/stream source opened by BackgroundMediaPlayer. Leave empty to use the player's already configured source. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Cinematic Background", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMediaSource> BackgroundMediaSource;

	/** Strength of the subtle full-screen readability veil. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Cinematic Background", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float ReadabilityOverlayOpacity = 0.32f;

	/** Optional transparent sumi-e brush texture revealed behind focused/hovered menu labels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Ink Menu", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> InkBrushTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Ink Menu", meta = (AllowPrivateAccess = "true", ClampMin = "0.05", ClampMax = "1.0"))
	float InkRevealDuration = 0.14f;

	/** Shared font styling for all menu-entry labels. Leave Size at 0 to use the built-in 24 pt fallback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Ink Menu", meta = (AllowPrivateAccess = "true"))
	FSlateFontInfo MenuButtonFont;

	/** Optional logo that replaces the HEAVENS DIVIDE text title when assigned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Logo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> MainMenuLogo;

	/** Display size of the optional main-menu logo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Logo", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	FVector2D MainMenuLogoSize = FVector2D(420.0f, 150.0f);

	/** Additional screen-space X/Y adjustment applied to the logo without moving the menu buttons. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Logo", meta = (AllowPrivateAccess = "true"))
	FVector2D MainMenuLogoOffset = FVector2D::ZeroVector;

	/** Space between the title/logo region and the first menu option. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Logo", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MainMenuLogoBottomSpacing = 34.0f;

	/** Additional X/Y adjustment for the five primary menu options. Use a negative Y value to move them upward. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Layout", meta = (AllowPrivateAccess = "true"))
	FVector2D MainMenuButtonsOffset = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Layout", meta = (AllowPrivateAccess = "true"))
	FVector2D CollectionPageOffset = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Layout", meta = (AllowPrivateAccess = "true"))
	FVector2D SettingsPageOffset = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Layout", meta = (AllowPrivateAccess = "true"))
	FVector2D ResetPopupOffset = FVector2D::ZeroVector;

	/** Applied to Collection/Settings Back buttons and the reset dialog action buttons. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Layout", meta = (AllowPrivateAccess = "true"))
	FVector2D SecondaryButtonsOffset = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Typography", meta = (AllowPrivateAccess = "true"))
	FSlateFontInfo SecondaryHeadingFont;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Typography", meta = (AllowPrivateAccess = "true"))
	FSlateFontInfo SecondaryBodyFont;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Typography", meta = (AllowPrivateAccess = "true"))
	FLinearColor SecondaryHeadingColor = FLinearColor(0.93f, 0.76f, 0.34f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Typography", meta = (AllowPrivateAccess = "true"))
	FLinearColor SecondaryBodyColor = FLinearColor(0.88f, 0.89f, 0.92f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Reset Popup", meta = (AllowPrivateAccess = "true"))
	FLinearColor ResetPopupBackgroundColor = FLinearColor(0.035f, 0.01f, 0.015f, 0.98f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Reset Popup", meta = (AllowPrivateAccess = "true"))
	FLinearColor ResetPopupTitleColor = FLinearColor(0.95f, 0.30f, 0.25f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Reset Popup", meta = (AllowPrivateAccess = "true"))
	FMargin ResetPopupPadding = FMargin(46.0f, 34.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Settings Popup", meta = (AllowPrivateAccess = "true"))
	FLinearColor SettingsPopupBackgroundColor = FLinearColor(0.015f, 0.018f, 0.025f, 0.94f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Settings Popup", meta = (AllowPrivateAccess = "true"))
	FMargin SettingsPopupPadding = FMargin(46.0f, 34.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Collection", meta = (AllowPrivateAccess = "true"))
	FLinearColor CollectionUnlockedCardColor = FLinearColor(0.09f, 0.12f, 0.19f, 0.98f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Collection", meta = (AllowPrivateAccess = "true"))
	FLinearColor CollectionLockedCardColor = FLinearColor(0.035f, 0.04f, 0.055f, 0.92f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Secondary Pages|Collection", meta = (AllowPrivateAccess = "true", ClampMin = "100.0"))
	FVector2D CollectionCardMinimumSize = FVector2D(340.0f, 150.0f);

	UFUNCTION()
	void HandleNewRun();
	UFUNCTION()
	void HandleCollection();
	UFUNCTION()
	void HandleSettings();
	UFUNCTION()
	void HandleAutoTargetingChanged(bool bIsChecked);
	UFUNCTION()
	void HandleResetProgress();
	UFUNCTION()
	void HandleExitGame();
	UFUNCTION()
	void HandleBack();
	UFUNCTION()
	void HandleCancelReset();
	UFUNCTION()
	void HandleConfirmReset();
	UFUNCTION() void HandleSamuraiCollectionTab();
	UFUNCTION() void HandleNinjaCollectionTab();
	UFUNCTION() void HandleSynergyCollectionTab();

	UPROPERTY(Transient)
	TObjectPtr<UWidgetSwitcher> MenuSwitcher;
	UPROPERTY(Transient)
	TObjectPtr<class UBorder> ResetConfirmationOverlay;
	UPROPERTY(Transient)
	TObjectPtr<class UBorder> SettingsPopupOverlay;
	UPROPERTY(Transient)
	TObjectPtr<UButton> NewRunButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CollectionDiscoveryCountText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CollectionProgressText;
	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> SynergyCollectionGrid;
	UPROPERTY(Transient)
	TObjectPtr<class UBorder> CollectionOverlay;
	UPROPERTY(Transient)
	TObjectPtr<UButton> CollectionMenuButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> SamuraiCollectionTab;
	UPROPERTY(Transient)
	TObjectPtr<UButton> NinjaCollectionTab;
	UPROPERTY(Transient)
	TObjectPtr<UButton> SynergyCollectionTab;
	UPROPERTY(Transient)
	TObjectPtr<UImage> CollectionDetailIcon;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CollectionDetailName;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CollectionDetailCategory;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CollectionDetailDescription;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CollectionDetailStats;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCollectionUpgradeTileButton>> CollectionTileButtons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UBorder>> CollectionTileSelectionBorders;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CollectionTileIcons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UUpgradeDefinition>> CollectionDefinitions;
	UPROPERTY(Transient)
	TObjectPtr<UUpgradeDefinition> SelectedCollectionUpgrade;
	EUpgradeCategory SelectedCollectionCategory = static_cast<EUpgradeCategory>(0);
	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> AutoTargetingCheckBox;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AutoTargetingStateText;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> MenuEntryButtons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> MenuEntryBrushImages;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> MenuEntryLabels;
	TArray<float> MenuEntryRevealAmounts;

	bool bResetConfirmationOpen = false;
	bool bSettingsPopupOpen = false;
	bool bCollectionOpen = false;
	bool bShowFocusHighlight = false;
};
