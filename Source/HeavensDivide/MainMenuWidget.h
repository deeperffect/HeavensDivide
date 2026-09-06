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
	class UBorder* BuildSecondaryPageFrame(UWidget* Content, FName PageName, const FVector2D& PageOffset, const FMargin& ContentPadding, const FLinearColor& FallbackColor, UWidget* Footer = nullptr);
	void AddSecondaryPageDivider(class UVerticalBox* Panel);
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

	// Cinematic Background

	/** Optional UI material used for the full-screen menu video. Takes priority over BackgroundMediaTexture. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Cinematic Background", meta = (AllowPrivateAccess = "true", DisplayPriority = "1"))
	TObjectPtr<UMaterialInterface> BackgroundMediaMaterial;

	/** Optional media texture displayed when no background material is assigned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Cinematic Background", meta = (AllowPrivateAccess = "true", DisplayPriority = "2"))
	TObjectPtr<UMediaTexture> BackgroundMediaTexture;

	/** Media Player opened and looped when this menu is constructed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Cinematic Background", meta = (AllowPrivateAccess = "true", DisplayPriority = "3"))
	TObjectPtr<UMediaPlayer> BackgroundMediaPlayer;

	/** File/stream source opened by BackgroundMediaPlayer. Leave empty to use the player's already configured source. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Cinematic Background", meta = (AllowPrivateAccess = "true", DisplayPriority = "4"))
	TObjectPtr<UMediaSource> BackgroundMediaSource;

	/** Strength of the subtle full-screen readability veil. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Cinematic Background", meta = (AllowPrivateAccess = "true", DisplayPriority = "5", ClampMin = "0.0", ClampMax = "1.0"))
	float ReadabilityOverlayOpacity = 0.32f;

	// Logo

	/** Optional logo that replaces the HEAVENS DIVIDE text title when assigned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Logo", meta = (AllowPrivateAccess = "true", DisplayPriority = "1"))
	TObjectPtr<UTexture2D> MainMenuLogo;

	/** Display size of the optional main-menu logo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Logo", meta = (AllowPrivateAccess = "true", DisplayPriority = "2", ClampMin = "1.0"))
	FVector2D MainMenuLogoSize = FVector2D(420.0f, 150.0f);

	/** Additional screen-space X/Y adjustment applied to the logo without moving the menu buttons. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Logo", meta = (AllowPrivateAccess = "true", DisplayPriority = "3"))
	FVector2D MainMenuLogoOffset = FVector2D::ZeroVector;

	/** Space between the title/logo region and the first menu option. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Logo", meta = (AllowPrivateAccess = "true", DisplayPriority = "4", ClampMin = "0.0"))
	float MainMenuLogoBottomSpacing = 34.0f;

	// Menu Buttons

	/** Additional X/Y adjustment for the five primary menu options. Use a negative Y value to move them upward. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Menu Buttons", meta = (AllowPrivateAccess = "true", DisplayPriority = "1"))
	FVector2D MainMenuButtonsOffset = FVector2D::ZeroVector;

	/** Shared font styling for all menu-entry labels. Leave Size at 0 to use the built-in 24 pt fallback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Menu Buttons", meta = (AllowPrivateAccess = "true", DisplayPriority = "2"))
	FSlateFontInfo MenuButtonFont;

	/** Optional transparent sumi-e brush texture revealed behind focused/hovered menu labels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Menu Buttons", meta = (AllowPrivateAccess = "true", DisplayPriority = "3"))
	TObjectPtr<UTexture2D> InkBrushTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Menu Buttons", meta = (AllowPrivateAccess = "true", DisplayPriority = "4", ClampMin = "0.05", ClampMax = "1.0"))
	float InkRevealDuration = 0.14f;

	// Collection - Page

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Page", meta = (AllowPrivateAccess = "true", DisplayPriority = "1"))
	FVector2D CollectionPageOffset = FVector2D::ZeroVector;

	/** Padding between the Collection background art edge and all page content. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Page", meta = (AllowPrivateAccess = "true", DisplayPriority = "2"))
	FMargin CollectionContentPadding = FMargin(28.0f, 22.0f);

	/** Shared grunge background for Collection, Settings, and Reset Progress. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Page", meta = (AllowPrivateAccess = "true", DisplayPriority = "3"))
	TObjectPtr<UTexture2D> CollectionPanelTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Page", meta = (AllowPrivateAccess = "true", DisplayPriority = "4"))
	FVector2D CollectionPanelImageScale = FVector2D(1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Page", meta = (AllowPrivateAccess = "true", DisplayPriority = "5"))
	FVector2D CollectionPanelImageOffset = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Page", meta = (AllowPrivateAccess = "true", DisplayPriority = "6", ClampMin = "8", ClampMax = "96"))
	int32 CollectionSubtitleFontSize = 14;

	// Collection - Tabs and Tiles

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Tabs and Tiles", meta = (AllowPrivateAccess = "true", DisplayPriority = "1", ClampMin = "8", ClampMax = "96"))
	int32 CollectionTabFontSize = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Tabs and Tiles", meta = (AllowPrivateAccess = "true", DisplayPriority = "2"))
	TObjectPtr<UTexture2D> TileBackgroundTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Tabs and Tiles", meta = (AllowPrivateAccess = "true", DisplayPriority = "3"))
	TObjectPtr<UTexture2D> TileSelectedTexture;

	/** Uniformly scales the background, icon, locked layer, and selected frame. 1.0 = 134 px; 0.5 = half size; 2.0 = double size. Applied when the Collection grid is built. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Tabs and Tiles", meta = (AllowPrivateAccess = "true", DisplayPriority = "4", ClampMin = "0.25", ClampMax = "3.0", UIMin = "0.25", UIMax = "3.0"))
	float CollectionTileScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Tabs and Tiles", meta = (AllowPrivateAccess = "true", DisplayPriority = "5", ClampMin = "0.0"))
	float CollectionTileSpacing = 6.0f;

	/** Design-space inset between the tile frame and icon. This is scaled together with the tile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Tabs and Tiles", meta = (AllowPrivateAccess = "true", DisplayPriority = "6", ClampMin = "0.0"))
	float CollectionTileIconPadding = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Tabs and Tiles", meta = (AllowPrivateAccess = "true", DisplayPriority = "7"))
	FLinearColor CollectionUnlockedCardColor = FLinearColor(0.09f, 0.12f, 0.19f, 0.98f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Tabs and Tiles", meta = (AllowPrivateAccess = "true", DisplayPriority = "8"))
	FLinearColor CollectionLockedCardColor = FLinearColor(0.035f, 0.04f, 0.055f, 0.92f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Tabs and Tiles", meta = (AllowPrivateAccess = "true", DisplayPriority = "9", ClampMin = "100.0"))
	FVector2D CollectionCardMinimumSize = FVector2D(340.0f, 150.0f);

	// Collection - Details Panel

	/** Moves the complete details panel. Negative Y moves it up; positive X moves it right. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Details Panel", meta = (AllowPrivateAccess = "true", DisplayPriority = "1"))
	FVector2D CollectionDetailsPanelOffset = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Details Panel", meta = (AllowPrivateAccess = "true", DisplayPriority = "2", ClampMin = "1.0"))
	FVector2D CollectionDetailsPanelSize = FVector2D(380.0f, 570.0f);

	/** Internal padding around the name, artwork, description, and stats. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Details Panel", meta = (AllowPrivateAccess = "true", DisplayPriority = "3"))
	FMargin CollectionDetailsContentPadding = FMargin(24.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Details Panel", meta = (AllowPrivateAccess = "true", DisplayPriority = "4"))
	TObjectPtr<UTexture2D> DetailsPanelTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Details Panel", meta = (AllowPrivateAccess = "true", DisplayPriority = "5"))
	FVector2D DetailsPanelImageScale = FVector2D(1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Details Panel", meta = (AllowPrivateAccess = "true", DisplayPriority = "6"))
	FVector2D DetailsPanelImageOffset = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Details Panel", meta = (AllowPrivateAccess = "true", DisplayPriority = "7", ClampMin = "1.0"))
	FVector2D CollectionDetailsArtworkSize = FVector2D(235.0f, 235.0f);

	/** Fine adjustment for the large details artwork without moving its title or description. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Details Panel", meta = (AllowPrivateAccess = "true", DisplayPriority = "8"))
	FVector2D CollectionDetailsArtworkOffset = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Details Panel", meta = (AllowPrivateAccess = "true", DisplayPriority = "9", ClampMin = "8", ClampMax = "96"))
	int32 CollectionDetailNameFontSize = 28;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Details Panel", meta = (AllowPrivateAccess = "true", DisplayPriority = "10", ClampMin = "8", ClampMax = "96"))
	int32 CollectionDescriptionFontSize = 17;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Details Panel", meta = (AllowPrivateAccess = "true", DisplayPriority = "11", ClampMin = "8", ClampMax = "96"))
	int32 CollectionStatsFontSize = 14;

	// Collection - Dividers and Corners

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Dividers and Corners", meta = (AllowPrivateAccess = "true", DisplayPriority = "1"))
	TObjectPtr<UTexture2D> VerticalDividerTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Dividers and Corners", meta = (AllowPrivateAccess = "true", DisplayPriority = "2", ClampMin = "1.0"))
	FVector2D VerticalDividerImageSize = FVector2D(18.0f, 570.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Dividers and Corners", meta = (AllowPrivateAccess = "true", DisplayPriority = "3"))
	FVector2D VerticalDividerOffset = FVector2D::ZeroVector;

	/** Shared brush for Collection separators and the Settings / Reset Progress dividers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Dividers and Corners", meta = (AllowPrivateAccess = "true", DisplayPriority = "4"))
	TObjectPtr<UTexture2D> HorizontalBrushTexture;

	/** X <= 0 keeps the horizontal brushes stretched to the available width. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Dividers and Corners", meta = (AllowPrivateAccess = "true", DisplayPriority = "5"))
	FVector2D HorizontalBrushSize = FVector2D(0.0f, 12.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Dividers and Corners", meta = (AllowPrivateAccess = "true", DisplayPriority = "6"))
	FVector2D TitleDividerOffset = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Dividers and Corners", meta = (AllowPrivateAccess = "true", DisplayPriority = "7"))
	FVector2D DetailsDividerOffset = FVector2D::ZeroVector;

	/** Optional separator placed between the Samurai/Ninja/Synergy tabs and the upgrade grid. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Dividers and Corners", meta = (AllowPrivateAccess = "true", DisplayPriority = "8"))
	TObjectPtr<UTexture2D> CategoryGridDividerTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Dividers and Corners", meta = (AllowPrivateAccess = "true", DisplayPriority = "9"))
	FVector2D CategoryGridDividerSize = FVector2D(0.0f, 12.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Dividers and Corners", meta = (AllowPrivateAccess = "true", DisplayPriority = "10"))
	FVector2D CategoryGridDividerOffset = FVector2D::ZeroVector;

	/** One top-left transparent corner texture, rotated for the other three corners. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Dividers and Corners", meta = (AllowPrivateAccess = "true", DisplayPriority = "11"))
	TObjectPtr<UTexture2D> CornerBrushTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Dividers and Corners", meta = (AllowPrivateAccess = "true", DisplayPriority = "12", ClampMin = "1.0"))
	FVector2D CornerBrushSize = FVector2D(96.0f, 96.0f);

	/** Positive values move every corner inward from its aligned panel edge. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Dividers and Corners", meta = (AllowPrivateAccess = "true", DisplayPriority = "13", ClampMin = "0.0"))
	FVector2D CornerBrushInset = FVector2D::ZeroVector;

	// Collection - Footer and Back Button

	/** Moves the unlocked/discovery footer. Positive X moves right; positive Y moves down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Footer and Back Button", meta = (AllowPrivateAccess = "true", DisplayPriority = "1"))
	FVector2D CollectionUnlockFooterOffset = FVector2D(24.0f, 70.0f);

	/** Dedicated typeface for the unlocked/discovery footer. Leave Size at 0 to inherit Secondary Body Font. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Footer and Back Button", meta = (AllowPrivateAccess = "true", DisplayPriority = "2"))
	FSlateFontInfo CollectionFooterFont;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Footer and Back Button", meta = (AllowPrivateAccess = "true", DisplayPriority = "3", ClampMin = "8", ClampMax = "96"))
	int32 CollectionFooterFontSize = 15;

	/** Fine adjustment for the Collection Back button. Positive X moves right; positive Y moves down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Footer and Back Button", meta = (AllowPrivateAccess = "true", DisplayPriority = "4"))
	FVector2D CollectionBackButtonOffset = FVector2D(0.0f, 18.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Collection|Footer and Back Button", meta = (AllowPrivateAccess = "true", DisplayPriority = "5", ClampMin = "60.0"))
	FVector2D CollectionBackButtonSize = FVector2D(150.0f, 52.0f);

	// Settings

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Settings", meta = (AllowPrivateAccess = "true", DisplayPriority = "1"))
	FVector2D SettingsPageOffset = FVector2D::ZeroVector;

	/** Fallback panel color when the shared Collection Panel Texture is unassigned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Settings", meta = (AllowPrivateAccess = "true", DisplayPriority = "2"))
	FLinearColor SettingsPopupBackgroundColor = FLinearColor(0.015f, 0.018f, 0.025f, 0.94f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Settings", meta = (AllowPrivateAccess = "true", DisplayPriority = "3"))
	FMargin SettingsPopupPadding = FMargin(46.0f, 34.0f);

	// Reset Confirmation

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Reset Confirmation", meta = (AllowPrivateAccess = "true", DisplayPriority = "1"))
	FVector2D ResetPopupOffset = FVector2D::ZeroVector;

	/** Fallback panel color when the shared Collection Panel Texture is unassigned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Reset Confirmation", meta = (AllowPrivateAccess = "true", DisplayPriority = "2"))
	FLinearColor ResetPopupBackgroundColor = FLinearColor(0.035f, 0.01f, 0.015f, 0.98f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Reset Confirmation", meta = (AllowPrivateAccess = "true", DisplayPriority = "3"))
	FLinearColor ResetPopupTitleColor = FLinearColor(0.95f, 0.30f, 0.25f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Reset Confirmation", meta = (AllowPrivateAccess = "true", DisplayPriority = "4"))
	FMargin ResetPopupPadding = FMargin(46.0f, 34.0f);

	// Shared Secondary Page Style

	/** Applied to Collection/Settings Back buttons and the reset dialog action buttons. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Shared Secondary Page Style", meta = (AllowPrivateAccess = "true", DisplayPriority = "1"))
	FVector2D SecondaryButtonsOffset = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Shared Secondary Page Style", meta = (AllowPrivateAccess = "true", DisplayPriority = "2"))
	FSlateFontInfo SecondaryHeadingFont;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Shared Secondary Page Style", meta = (AllowPrivateAccess = "true", DisplayPriority = "3"))
	FLinearColor SecondaryHeadingColor = FLinearColor(0.93f, 0.76f, 0.34f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Shared Secondary Page Style", meta = (AllowPrivateAccess = "true", DisplayPriority = "4"))
	FSlateFontInfo SecondaryBodyFont;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Menu|Shared Secondary Page Style", meta = (AllowPrivateAccess = "true", DisplayPriority = "5"))
	FLinearColor SecondaryBodyColor = FLinearColor(0.88f, 0.89f, 0.92f, 1.0f);

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
	TObjectPtr<UTextBlock> CollectionDetailDescription;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CollectionDetailStats;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCollectionUpgradeTileButton>> CollectionTileButtons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UBorder>> CollectionTileSelectionBorders;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CollectionTileSelectionImages;
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
