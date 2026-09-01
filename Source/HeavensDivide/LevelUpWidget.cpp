// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelUpWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Materials/MaterialInterface.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"

void ULevelUpWidget::InitializeLevelUpWidget(ASurvivorPlayerController* InPlayerController)
{
	SurvivorPlayerController = InPlayerController;
	PlayerUpgrades = SurvivorPlayerController ? SurvivorPlayerController->GetPlayerUpgrades() : nullptr;
	bCategoryChoiceCommitted = false;
	bUpgradeChoiceCommitted = false;
	bSynergyDiscoveryMode = false;
	SetSynergyDiscoveryPresentation(false);
	EnsureUpgradeCardVisualStructure();
	RefreshCategoryChoices();
	InitializeControllerNavigation();
}

void ULevelUpWidget::InitializeDirectUpgradeWidget(ASurvivorPlayerController* InPlayerController)
{
	SurvivorPlayerController = InPlayerController;
	PlayerUpgrades = SurvivorPlayerController ? SurvivorPlayerController->GetPlayerUpgrades() : nullptr;
	bCategoryChoiceCommitted = true;
	bUpgradeChoiceCommitted = false;
	bSynergyDiscoveryMode = false;
	SetSynergyDiscoveryPresentation(false);
	EnsureUpgradeCardVisualStructure();
	ShowUpgradeChoices(PlayerUpgrades ? PlayerUpgrades->GetCurrentUpgradeChoices() : TArray<UUpgradeDefinition*>());
	ShowUpgradeOffers(PlayerUpgrades ? PlayerUpgrades->GetCurrentUpgradeOffers() : TArray<FUpgradeOffer>());
	RefreshUpgradeCardVisuals();
	InitializeControllerNavigation();
}

void ULevelUpWidget::InitializeSynergyDiscoveryWidget(ASurvivorPlayerController* InPlayerController)
{
	SurvivorPlayerController = InPlayerController;
	PlayerUpgrades = SurvivorPlayerController ? SurvivorPlayerController->GetPlayerUpgrades() : nullptr;
	bCategoryChoiceCommitted = true;
	bUpgradeChoiceCommitted = false;
	bSynergyDiscoveryMode = true;
	SetSynergyDiscoveryPresentation(true);
	EnsureUpgradeCardVisualStructure();
	ShowUpgradeChoices(PlayerUpgrades ? PlayerUpgrades->GetCurrentUpgradeChoices() : TArray<UUpgradeDefinition*>());
	ShowUpgradeOffers(PlayerUpgrades ? PlayerUpgrades->GetCurrentUpgradeOffers() : TArray<FUpgradeOffer>());
	RefreshUpgradeCardVisuals();
	InitializeControllerNavigation();
}

void ULevelUpWidget::RefreshCategoryChoices()
{
	EnsureCategoryCardVisualStructure();
	if (!PlayerUpgrades)
	{
		ShowCategoryChoices(TArray<EUpgradeCategory>());
		RefreshCategoryCardVisuals();
		return;
	}

	ShowCategoryChoices(PlayerUpgrades->GetCurrentCategoryChoices());
	RefreshCategoryCardVisuals();
}

bool ULevelUpWidget::SelectCategoryChoice(int32 ChoiceIndex)
{
	if (bCategoryChoiceCommitted)
	{
		UE_LOG(LogTemp, Warning, TEXT("LevelUpWidget category selection ignored: category already selected for this offer."));
		return false;
	}

	if (!PlayerUpgrades)
	{
		return false;
	}

	const TArray<EUpgradeCategory> CategoryChoices = PlayerUpgrades->GetCurrentCategoryChoices();
	if (!CategoryChoices.IsValidIndex(ChoiceIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("LevelUpWidget category index rejected: %d"), ChoiceIndex);
		return false;
	}

	const EUpgradeCategory SelectedCategory = CategoryChoices[ChoiceIndex];
	if (!PlayerUpgrades->SelectCategory(SelectedCategory, 3))
	{
		return false;
	}

	bCategoryChoiceCommitted = true;
	bUpgradeChoiceCommitted = false;

	const TArray<UUpgradeDefinition*> UpgradeChoices = PlayerUpgrades->GetCurrentUpgradeChoices();
	UE_LOG(LogTemp, Log, TEXT("UPGRADE OFFER:"));
	for (const UUpgradeDefinition* Upgrade : UpgradeChoices)
	{
		UE_LOG(LogTemp, Log, TEXT("  %s"), Upgrade ? *Upgrade->DisplayName.ToString() : TEXT("None"));
	}

	ShowUpgradeChoices(UpgradeChoices);
	ShowUpgradeOffers(PlayerUpgrades->GetCurrentUpgradeOffers());
	RefreshUpgradeCardVisuals();
	ControllerFocusedChoiceIndex = 0;
	bHasControllerSelection = false;
	MouseHoveredButton = nullptr;
	RefreshControllerFocus();
	return UpgradeChoices.Num() > 0;
}

void ULevelUpWidget::InitializeControllerNavigation()
{
	SetIsFocusable(true);
	ControllerFocusedChoiceIndex = 0;
	bHasControllerSelection = false;
	bControllerAnalogNavigationHeld = false;
	MouseHoveredButton = nullptr;
	RefreshControllerFocus();
}

int32 ULevelUpWidget::GetVisibleChoiceCount() const
{
	if (!PlayerUpgrades) return 0;
	return bCategoryChoiceCommitted
		? PlayerUpgrades->GetCurrentUpgradeChoices().Num()
		: PlayerUpgrades->GetCurrentCategoryChoices().Num();
}

void ULevelUpWidget::MoveControllerFocus(int32 Direction)
{
	const int32 ChoiceCount = GetVisibleChoiceCount();
	if (ChoiceCount <= 0 || Direction == 0) return;
	MouseHoveredButton = nullptr;
	if (!bHasControllerSelection)
	{
		ControllerFocusedChoiceIndex = Direction > 0 ? 0 : ChoiceCount - 1;
		bHasControllerSelection = true;
	}
	else
	{
		ControllerFocusedChoiceIndex = (ControllerFocusedChoiceIndex + Direction + ChoiceCount) % ChoiceCount;
	}
	RefreshControllerFocus();
}

void ULevelUpWidget::RefreshControllerFocus()
{
	const int32 ChoiceCount = GetVisibleChoiceCount();
	ControllerFocusedChoiceIndex = ChoiceCount > 0 ? FMath::Clamp(ControllerFocusedChoiceIndex, 0, ChoiceCount - 1) : 0;
	RebuildFocusableChoices();
	if (bHasControllerSelection)
	{
		SetControllerFocusPresentation(ControllerFocusedChoiceIndex, !bCategoryChoiceCommitted);
	}
	RefreshChoiceScales();

	// The Blueprint buttons are mouse click targets with focus disabled. The focusable
	// native widget owns controller input and the indexed buttons provide presentation.
	SetUserFocus(SurvivorPlayerController);
}

void ULevelUpWidget::RefreshChoiceScales()
{
	for (int32 Index = 0; Index < FocusableChoiceButtons.Num(); ++Index)
	{
		if (UButton* Button = FocusableChoiceButtons[Index])
		{
			const bool bMouseHovered = Button == MouseHoveredButton;
			const bool bControllerSelected = !MouseHoveredButton && bHasControllerSelection && Index == ControllerFocusedChoiceIndex;
			Button->SetRenderScale(bMouseHovered || bControllerSelected ? FVector2D(1.045f) : FVector2D(1.0f));
		}
	}
}

void ULevelUpWidget::RefreshMouseHoveredButton()
{
	UButton* NewHoveredButton = nullptr;
	for (UButton* Button : FocusableChoiceButtons)
	{
		if (Button && Button->IsHovered())
		{
			NewHoveredButton = Button;
			break;
		}
	}
	if (MouseHoveredButton != NewHoveredButton)
	{
		MouseHoveredButton = NewHoveredButton;
		RefreshChoiceScales();
	}
}

void ULevelUpWidget::RebuildFocusableChoices()
{
	FocusableChoiceButtons.Reset();
	if (!WidgetTree) return;

	const FString RequiredPrefix = bCategoryChoiceCommitted ? TEXT("UpgradeButton_") : TEXT("CategoryButton_");
	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	for (UWidget* Widget : Widgets)
	{
		UButton* Button = Cast<UButton>(Widget);
		if (Button && Button->GetName().StartsWith(RequiredPrefix) && Button->IsVisible() && Button->GetIsEnabled())
		{
			FocusableChoiceButtons.Add(Button);
		}
	}
	FocusableChoiceButtons.Sort([](const UButton& A, const UButton& B)
	{
		return A.GetName() < B.GetName();
	});
}

FReply ULevelUpWidget::HandleControllerKey(const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Left || Key == EKeys::Up)
	{
		MoveControllerFocus(-1);
		return FReply::Handled();
	}
	if (Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Right || Key == EKeys::Down)
	{
		MoveControllerFocus(1);
		return FReply::Handled();
	}
	if (Key == EKeys::Gamepad_FaceButton_Bottom || Key == EKeys::Enter || Key == EKeys::SpaceBar)
	{
		if (!bHasControllerSelection)
		{
			ControllerFocusedChoiceIndex = 0;
			bHasControllerSelection = GetVisibleChoiceCount() > 0;
			MouseHoveredButton = nullptr;
			RefreshControllerFocus();
		}
		const bool bSelected = bCategoryChoiceCommitted
			? SelectUpgradeChoice(ControllerFocusedChoiceIndex)
			: SelectCategoryChoice(ControllerFocusedChoiceIndex);
		return bSelected ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::Gamepad_FaceButton_Right)
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply ULevelUpWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FReply Reply = HandleControllerKey(InKeyEvent);
	return Reply.IsEventHandled() ? Reply : Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply ULevelUpWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FReply Reply = HandleControllerKey(InKeyEvent);
	return Reply.IsEventHandled() ? Reply : Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply ULevelUpWidget::NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogInputEvent)
{
	const FKey Key = InAnalogInputEvent.GetKey();
	if (Key == EKeys::Gamepad_LeftX)
	{
		const float Value = InAnalogInputEvent.GetAnalogValue();
		if (FMath::Abs(Value) >= 0.5f)
		{
			if (!bControllerAnalogNavigationHeld) MoveControllerFocus(Value > 0.0f ? 1 : -1);
			bControllerAnalogNavigationHeld = true;
		}
		else
		{
			bControllerAnalogNavigationHeld = false;
		}
		return FReply::Handled();
	}
	return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogInputEvent);
}

FReply ULevelUpWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	RefreshMouseHoveredButton();
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void ULevelUpWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	MouseHoveredButton = nullptr;
	RefreshChoiceScales();
	Super::NativeOnMouseLeave(InMouseEvent);
}

bool ULevelUpWidget::GetOfferForUpgrade(UUpgradeDefinition* Upgrade, FUpgradeOffer& OutOffer) const
{
	if (!PlayerUpgrades) return false;
	for (const FUpgradeOffer& Offer : PlayerUpgrades->GetCurrentUpgradeOffers())
	{
		if (Offer.UpgradeDefinition == Upgrade)
		{
			OutOffer = Offer;
			return true;
		}
	}
	return false;
}

bool ULevelUpWidget::SelectUpgradeChoice(int32 ChoiceIndex)
{
	if (bUpgradeChoiceCommitted)
	{
		UE_LOG(LogTemp, Warning, TEXT("LevelUpWidget upgrade selection ignored: upgrade already selected for this offer."));
		return false;
	}

	if (!PlayerUpgrades)
	{
		return false;
	}

	const TArray<UUpgradeDefinition*> UpgradeChoices = PlayerUpgrades->GetCurrentUpgradeChoices();
	if (!UpgradeChoices.IsValidIndex(ChoiceIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("LevelUpWidget upgrade index rejected: %d"), ChoiceIndex);
		return false;
	}

	UUpgradeDefinition* SelectedUpgrade = UpgradeChoices[ChoiceIndex];
	const bool bSelected = bSynergyDiscoveryMode
		? PlayerUpgrades->SelectSynergyDiscoveryUpgrade(SelectedUpgrade)
		: PlayerUpgrades->SelectUpgrade(SelectedUpgrade);
	if (!bSelected)
	{
		return false;
	}

	bUpgradeChoiceCommitted = true;

	UE_LOG(LogTemp, Log, TEXT("UPGRADE SELECTED: %s"), SelectedUpgrade ? *SelectedUpgrade->DisplayName.ToString() : TEXT("None"));
	UE_LOG(LogTemp, Log, TEXT("New upgrade level = %d"), PlayerUpgrades->GetUpgradeLevel(SelectedUpgrade));
	OnSelectionCompleted.Broadcast();
	return true;
}

int32 ULevelUpWidget::GetUpgradeLevel(UUpgradeDefinition* Upgrade) const
{
	return PlayerUpgrades ? PlayerUpgrades->GetUpgradeLevel(Upgrade) : 0;
}

UPlayerUpgradeComponent* ULevelUpWidget::GetPlayerUpgrades() const
{
	return PlayerUpgrades;
}

void ULevelUpWidget::EnsureCategoryCardVisualStructure()
{
	if (!WidgetTree || CategoryArtworkImages.Num() == 2)
	{
		return;
	}

	CategoryArtworkImages.Reset();
	CategoryArtworkCoverImages.Reset();
	CategoryBorderImages.Reset();
	if (UWidget* CategoryPanel = WidgetTree->FindWidget(TEXT("CategoryPanel")))
	{
		if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(CategoryPanel->Slot))
		{
			const FAnchors ExistingAnchors = PanelSlot->GetAnchors();
			const FVector2D ExistingAlignment = PanelSlot->GetAlignment();
			const FVector2D ExistingPosition = PanelSlot->GetPosition();
			PanelSlot->SetAnchors(FAnchors(0.5f, ExistingAnchors.Minimum.Y, 0.5f, ExistingAnchors.Maximum.Y));
			PanelSlot->SetAlignment(FVector2D(0.5f, ExistingAlignment.Y));
			PanelSlot->SetPosition(FVector2D(0.0f, ExistingPosition.Y));
			PanelSlot->SetAutoSize(true);
		}
	}
	constexpr float CardWidth = 400.0f;
	constexpr float CardHeight = CardWidth * 1.5f;
	constexpr float CardVisualScale = 1.15f;
	constexpr float ArtworkInset = 22.0f;

	for (int32 Index = 0; Index < 2; ++Index)
	{
		UButton* CategoryButton = Cast<UButton>(WidgetTree->FindWidget(*FString::Printf(TEXT("CategoryButton_%d"), Index)));
		if (!CategoryButton)
		{
			CategoryArtworkImages.Add(nullptr);
			CategoryArtworkCoverImages.Add(nullptr);
			CategoryBorderImages.Add(nullptr);
			continue;
		}

		// Keep the two larger cards centered as a compact pair instead of allowing
		// their Horizontal Box slots to spread them across the available width.
		if (UHorizontalBoxSlot* ButtonSlot = Cast<UHorizontalBoxSlot>(CategoryButton->Slot))
		{
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ButtonSlot->SetHorizontalAlignment(HAlign_Center);
			ButtonSlot->SetVerticalAlignment(VAlign_Center);
			ButtonSlot->SetPadding(FMargin(12.0f, 0.0f));
		}

		if (UImage* ExistingArtwork = Cast<UImage>(WidgetTree->FindWidget(*FString::Printf(TEXT("CategoryArtwork_%d"), Index))))
		{
			CategoryArtworkImages.Add(ExistingArtwork);
			CategoryArtworkCoverImages.Add(Cast<UImage>(WidgetTree->FindWidget(*FString::Printf(TEXT("CategoryArtworkCover_%d"), Index))));
			CategoryBorderImages.Add(Cast<UImage>(WidgetTree->FindWidget(*FString::Printf(TEXT("CategoryBorder_%d"), Index))));
			continue;
		}

		FSlateBrush TransparentButtonBrush;
		TransparentButtonBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		FButtonStyle TransparentButtonStyle = CategoryButton->GetStyle();
		TransparentButtonStyle.SetNormal(TransparentButtonBrush);
		TransparentButtonStyle.SetHovered(TransparentButtonBrush);
		TransparentButtonStyle.SetPressed(TransparentButtonBrush);
		TransparentButtonStyle.SetDisabled(TransparentButtonBrush);
		CategoryButton->SetStyle(TransparentButtonStyle);

		UTextBlock* ExistingCategoryText = Cast<UTextBlock>(WidgetTree->FindWidget(
			*FString::Printf(TEXT("CategoryText_%d"), Index)));
		if (ExistingCategoryText)
		{
			// The Blueprint button content contains additional presentation widgets.
			// Detach only the label so the old backing plate is not carried into the
			// rebuilt card hierarchy with it.
			ExistingCategoryText->RemoveFromParent();
		}
		CategoryButton->ClearChildren();

		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), *FString::Printf(TEXT("CategoryCardSize_%d"), Index));
		CardSize->SetWidthOverride(CardWidth * CardVisualScale);
		CardSize->SetHeightOverride(CardHeight * CardVisualScale);

		UScaleBox* CardScaler = WidgetTree->ConstructWidget<UScaleBox>(
			UScaleBox::StaticClass(), *FString::Printf(TEXT("CategoryCardScaler_%d"), Index));
		CardScaler->SetStretch(EStretch::ScaleToFit);
		CardScaler->SetStretchDirection(EStretchDirection::Both);
		CardSize->SetContent(CardScaler);

		USizeBox* AuthoredCardSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), *FString::Printf(TEXT("CategoryAuthoredCardSize_%d"), Index));
		AuthoredCardSize->SetWidthOverride(CardWidth);
		AuthoredCardSize->SetHeightOverride(CardHeight);
		CardScaler->SetContent(AuthoredCardSize);

		UOverlay* CardLayers = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(), *FString::Printf(TEXT("CategoryCardLayers_%d"), Index));
		AuthoredCardSize->SetContent(CardLayers);
		CategoryButton->SetContent(CardSize);

		UScaleBox* ArtworkContainer = WidgetTree->ConstructWidget<UScaleBox>(
			UScaleBox::StaticClass(), *FString::Printf(TEXT("CategoryArtworkContainer_%d"), Index));
		ArtworkContainer->SetStretch(EStretch::ScaleToFill);
		ArtworkContainer->SetStretchDirection(EStretchDirection::Both);
		ArtworkContainer->SetClipping(EWidgetClipping::ClipToBounds);
		ArtworkContainer->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* ArtworkContainerSlot = CardLayers->AddChildToOverlay(ArtworkContainer);
		ArtworkContainerSlot->SetHorizontalAlignment(HAlign_Fill);
		ArtworkContainerSlot->SetVerticalAlignment(VAlign_Fill);
		ArtworkContainerSlot->SetPadding(FMargin(ArtworkInset));

		UImage* Artwork = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), *FString::Printf(TEXT("CategoryArtwork_%d"), Index));
		Artwork->SetVisibility(ESlateVisibility::HitTestInvisible);
		ArtworkContainer->SetContent(Artwork);

		UImage* CategoryBorder = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), *FString::Printf(TEXT("CategoryBorder_%d"), Index));
		CategoryBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
		CategoryBorder->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		CategoryBorder->SetRenderScale(FVector2D(1.04f, 1.04f));
		UOverlaySlot* BorderSlot = CardLayers->AddChildToOverlay(CategoryBorder);
		BorderSlot->SetHorizontalAlignment(HAlign_Fill);
		BorderSlot->SetVerticalAlignment(VAlign_Fill);

		// Redraw only the artwork interior above the border texture. This hides the
		// border asset's interior backing plate while leaving its outer frame on top.
		constexpr float BorderInteriorInset = 48.0f;
		UImage* ArtworkCover = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), *FString::Printf(TEXT("CategoryArtworkCover_%d"), Index));
		ArtworkCover->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* ArtworkCoverSlot = CardLayers->AddChildToOverlay(ArtworkCover);
		ArtworkCoverSlot->SetHorizontalAlignment(HAlign_Fill);
		ArtworkCoverSlot->SetVerticalAlignment(VAlign_Fill);
		ArtworkCoverSlot->SetPadding(FMargin(BorderInteriorInset));

		UBorder* TextLayer = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), *FString::Printf(TEXT("CategoryTextLayer_%d"), Index));
		FSlateBrush NoBackgroundBrush;
		NoBackgroundBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		TextLayer->SetBrush(NoBackgroundBrush);
		TextLayer->SetPadding(FMargin(24.0f, 12.0f));
		TextLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (ExistingCategoryText)
		{
			ExistingCategoryText->SetVisibility(ESlateVisibility::HitTestInvisible);
			TextLayer->SetContent(ExistingCategoryText);
		}
		UOverlaySlot* TextSlot = CardLayers->AddChildToOverlay(TextLayer);
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetVerticalAlignment(VAlign_Bottom);
		TextSlot->SetPadding(FMargin(36.0f, 36.0f, 36.0f, 52.0f));

		CategoryArtworkImages.Add(Artwork);
		CategoryArtworkCoverImages.Add(ArtworkCover);
		CategoryBorderImages.Add(CategoryBorder);
	}
}

void ULevelUpWidget::RefreshCategoryCardVisuals()
{
	EnsureCategoryCardVisualStructure();
	const TArray<EUpgradeCategory> Choices = PlayerUpgrades
		? PlayerUpgrades->GetCurrentCategoryChoices()
		: TArray<EUpgradeCategory>();

	for (int32 Index = 0; Index < 2; ++Index)
	{
		const bool bHasChoice = Choices.IsValidIndex(Index);
		const EUpgradeCategory Category = bHasChoice ? Choices[Index] : EUpgradeCategory::Global;
		UTexture2D* ArtworkTexture = bHasChoice ? GetCategoryArtwork(Category) : nullptr;
		UTexture2D* BorderTexture = bHasChoice ? GetCategoryBorder(Category) : nullptr;
		if (CategoryArtworkImages.IsValidIndex(Index) && CategoryArtworkImages[Index])
		{
			CategoryArtworkImages[Index]->SetBrushFromTexture(ArtworkTexture, true);
			CategoryArtworkImages[Index]->SetOpacity(ArtworkTexture ? 1.0f : 0.0f);
		}
		if (CategoryArtworkCoverImages.IsValidIndex(Index) && CategoryArtworkCoverImages[Index])
		{
			constexpr float CoverInset = 48.0f;
			constexpr float ArtworkInset = 22.0f;
			constexpr float CardWidth = 400.0f;
			constexpr float CardHeight = CardWidth * 1.5f;
			const FVector2D UVMin(
				(CoverInset - ArtworkInset) / (CardWidth - 2.0f * ArtworkInset),
				(CoverInset - ArtworkInset) / (CardHeight - 2.0f * ArtworkInset));
			CategoryArtworkCoverImages[Index]->SetBrushFromTexture(ArtworkTexture, true);
			FSlateBrush CoverBrush = CategoryArtworkCoverImages[Index]->GetBrush();
			CoverBrush.SetUVRegion(FBox2d(FVector2d(UVMin), FVector2d(FVector2D(1.0f) - UVMin)));
			CategoryArtworkCoverImages[Index]->SetBrush(CoverBrush);
			CategoryArtworkCoverImages[Index]->SetOpacity(ArtworkTexture ? 1.0f : 0.0f);
		}
		if (CategoryBorderImages.IsValidIndex(Index) && CategoryBorderImages[Index])
		{
			CategoryBorderImages[Index]->SetBrushFromTexture(BorderTexture, true);
			CategoryBorderImages[Index]->SetOpacity(BorderTexture ? 1.0f : 0.0f);
		}
	}
}

UTexture2D* ULevelUpWidget::GetCategoryArtwork(EUpgradeCategory Category) const
{
	switch (Category)
	{
	case EUpgradeCategory::Samurai: return SamuraiCategoryArtwork;
	case EUpgradeCategory::Ninja: return NinjaCategoryArtwork;
	case EUpgradeCategory::Synergy: return SynergyCategoryArtwork;
	case EUpgradeCategory::Global: return GlobalCategoryArtwork;
	default: return GlobalCategoryArtwork;
	}
}

UTexture2D* ULevelUpWidget::GetCategoryBorder(EUpgradeCategory Category) const
{
	switch (Category)
	{
	case EUpgradeCategory::Samurai: return SamuraiCategoryBorder;
	case EUpgradeCategory::Ninja: return NinjaCategoryBorder;
	case EUpgradeCategory::Synergy: return SynergyCategoryBorder;
	case EUpgradeCategory::Global: return GlobalCategoryBorder;
	default: return GlobalCategoryBorder;
	}
}

void ULevelUpWidget::EnsureUpgradeCardVisualStructure()
{
	if (!WidgetTree || UpgradeArtworkImages.Num() == 3)
	{
		return;
	}

	UpgradeArtworkImages.Reset();
	UpgradeBorderImages.Reset();
	UpgradeRarityGlowImages.Reset();
	if (!RareRarityMaterial)
	{
		RareRarityMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/HeavensDivide/Blueprints/UI/UpgradeUI/Materials/MI_UI_UpgradeRare.MI_UI_UpgradeRare"));
	}
	if (!EpicRarityMaterial)
	{
		EpicRarityMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/HeavensDivide/Blueprints/UI/UpgradeUI/Materials/MI_UI_UpgradeEpic.MI_UI_UpgradeEpic"));
	}
	if (!LegendaryRarityMaterial)
	{
		LegendaryRarityMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/HeavensDivide/Blueprints/UI/UpgradeUI/Materials/MI_UI_UpgradeLegendary.MI_UI_UpgradeLegendary"));
	}
	constexpr float CardWidth = 400.0f;
	constexpr float CardHeight = CardWidth * 1.5f;
	constexpr float CardVisualScale = 1.15f;
	constexpr float ArtworkInset = 22.0f;
	constexpr float TextInset = 26.0f;

	for (int32 Index = 0; Index < 3; ++Index)
	{
		UButton* UpgradeButton = Cast<UButton>(WidgetTree->FindWidget(*FString::Printf(TEXT("UpgradeButton_%d"), Index)));
		if (!UpgradeButton)
		{
			UpgradeArtworkImages.Add(nullptr);
			UpgradeBorderImages.Add(nullptr);
			UpgradeRarityGlowImages.Add(nullptr);
			continue;
		}

		// Do not rebuild a slot if this widget instance is initialized more than once.
		if (UImage* ExistingArtwork = Cast<UImage>(WidgetTree->FindWidget(*FString::Printf(TEXT("UpgradeArtwork_%d"), Index))))
		{
			UpgradeArtworkImages.Add(ExistingArtwork);
			UpgradeBorderImages.Add(Cast<UImage>(WidgetTree->FindWidget(*FString::Printf(TEXT("UpgradeBorder_%d"), Index))));
			UImage* ExistingGlow = Cast<UImage>(WidgetTree->FindWidget(*FString::Printf(TEXT("UpgradeRarityGlow_%d"), Index)));
			UpgradeRarityGlowImages.Add(ExistingGlow);
			continue;
		}

		// Preserve the button as the mouse/controller interaction target while removing
		// the old Blueprint-authored colored background from every visual state.
		FSlateBrush TransparentButtonBrush;
		TransparentButtonBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		FButtonStyle TransparentButtonStyle = UpgradeButton->GetStyle();
		TransparentButtonStyle.SetNormal(TransparentButtonBrush);
		TransparentButtonStyle.SetHovered(TransparentButtonBrush);
		TransparentButtonStyle.SetPressed(TransparentButtonBrush);
		TransparentButtonStyle.SetDisabled(TransparentButtonBrush);
		UpgradeButton->SetStyle(TransparentButtonStyle);

		UWidget* ExistingTextContent = UpgradeButton->GetContent();
		UpgradeButton->ClearChildren();

		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), *FString::Printf(TEXT("UpgradeCardSize_%d"), Index));
		CardSize->SetWidthOverride(CardWidth * CardVisualScale);
		CardSize->SetHeightOverride(CardHeight * CardVisualScale);

		// Scale the complete existing card composition as one unit. Keeping its authored
		// 2:3 canvas intact scales artwork, text, rarity effects, and frame uniformly,
		// while the outer size participates in layout so adjacent cards cannot overlap.
		UScaleBox* CardScaler = WidgetTree->ConstructWidget<UScaleBox>(
			UScaleBox::StaticClass(), *FString::Printf(TEXT("UpgradeCardScaler_%d"), Index));
		CardScaler->SetStretch(EStretch::ScaleToFit);
		CardScaler->SetStretchDirection(EStretchDirection::Both);
		CardSize->SetContent(CardScaler);

		USizeBox* AuthoredCardSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), *FString::Printf(TEXT("UpgradeAuthoredCardSize_%d"), Index));
		AuthoredCardSize->SetWidthOverride(CardWidth);
		AuthoredCardSize->SetHeightOverride(CardHeight);
		CardScaler->SetContent(AuthoredCardSize);

		UOverlay* CardLayers = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(), *FString::Printf(TEXT("UpgradeCardLayers_%d"), Index));
		AuthoredCardSize->SetContent(CardLayers);
		UpgradeButton->SetContent(CardSize);

		UScaleBox* ArtworkContainer = WidgetTree->ConstructWidget<UScaleBox>(
			UScaleBox::StaticClass(), *FString::Printf(TEXT("UpgradeArtworkContainer_%d"), Index));
		ArtworkContainer->SetStretch(EStretch::ScaleToFill);
		ArtworkContainer->SetStretchDirection(EStretchDirection::Both);
		ArtworkContainer->SetClipping(EWidgetClipping::ClipToBounds);
		ArtworkContainer->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* ArtworkContainerSlot = CardLayers->AddChildToOverlay(ArtworkContainer);
		ArtworkContainerSlot->SetHorizontalAlignment(HAlign_Fill);
		ArtworkContainerSlot->SetVerticalAlignment(VAlign_Fill);
		ArtworkContainerSlot->SetPadding(FMargin(ArtworkInset));

		UImage* Artwork = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), *FString::Printf(TEXT("UpgradeArtwork_%d"), Index));
		Artwork->SetVisibility(ESlateVisibility::HitTestInvisible);
		ArtworkContainer->SetContent(Artwork);

		UImage* RarityGlow = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), *FString::Printf(TEXT("UpgradeRarityGlow_%d"), Index));
		RarityGlow->SetVisibility(ESlateVisibility::Collapsed);
		UOverlaySlot* RarityGlowSlot = CardLayers->AddChildToOverlay(RarityGlow);
		RarityGlowSlot->SetHorizontalAlignment(HAlign_Fill);
		RarityGlowSlot->SetVerticalAlignment(VAlign_Fill);
		RarityGlowSlot->SetPadding(FMargin(8.0f));

		UBorder* TextReadabilityLayer = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), *FString::Printf(TEXT("UpgradeTextLayer_%d"), Index));
		TextReadabilityLayer->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.02f, 0.42f));
		TextReadabilityLayer->SetPadding(FMargin(18.0f, 20.0f));
		TextReadabilityLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (ExistingTextContent)
		{
			TextReadabilityLayer->SetContent(ExistingTextContent);
		}
		UOverlaySlot* TextSlot = CardLayers->AddChildToOverlay(TextReadabilityLayer);
		TextSlot->SetHorizontalAlignment(HAlign_Fill);
		TextSlot->SetVerticalAlignment(VAlign_Fill);
		TextSlot->SetPadding(FMargin(TextInset));

		UImage* CategoryBorder = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), *FString::Printf(TEXT("UpgradeBorder_%d"), Index));
		CategoryBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* BorderSlot = CardLayers->AddChildToOverlay(CategoryBorder);
		BorderSlot->SetHorizontalAlignment(HAlign_Fill);
		BorderSlot->SetVerticalAlignment(VAlign_Fill);

		UpgradeArtworkImages.Add(Artwork);
		UpgradeBorderImages.Add(CategoryBorder);
		UpgradeRarityGlowImages.Add(RarityGlow);
	}
}

void ULevelUpWidget::RefreshUpgradeCardVisuals()
{
	EnsureUpgradeCardVisualStructure();
	const TArray<UUpgradeDefinition*> Choices = PlayerUpgrades
		? PlayerUpgrades->GetCurrentUpgradeChoices()
		: TArray<UUpgradeDefinition*>();
	const TArray<FUpgradeOffer> Offers = PlayerUpgrades
		? PlayerUpgrades->GetCurrentUpgradeOffers()
		: TArray<FUpgradeOffer>();

	for (int32 Index = 0; Index < 3; ++Index)
	{
		UUpgradeDefinition* Upgrade = Choices.IsValidIndex(Index) ? Choices[Index] : nullptr;
		if (UpgradeArtworkImages.IsValidIndex(Index) && UpgradeArtworkImages[Index])
		{
			UpgradeArtworkImages[Index]->SetBrushFromTexture(Upgrade ? Upgrade->CardArtwork : nullptr, true);
			UpgradeArtworkImages[Index]->SetOpacity(Upgrade && Upgrade->CardArtwork ? 1.0f : 0.0f);
		}
		if (UpgradeBorderImages.IsValidIndex(Index) && UpgradeBorderImages[Index])
		{
			UTexture2D* BorderTexture = Upgrade ? GetCardBorderForCategory(Upgrade->Category) : nullptr;
			UpgradeBorderImages[Index]->SetBrushFromTexture(BorderTexture, true);
			UpgradeBorderImages[Index]->SetOpacity(BorderTexture ? 1.0f : 0.0f);
		}
		if (UpgradeRarityGlowImages.IsValidIndex(Index) && UpgradeRarityGlowImages[Index])
		{
			const EUpgradeRarity DisplayRarity = Offers.IsValidIndex(Index) && Offers[Index].bDisplaysRarity
				? Offers[Index].RolledRarity
				: EUpgradeRarity::Common;
			UMaterialInterface* RarityMaterial = GetRarityMaterial(DisplayRarity);
			if (Upgrade && RarityMaterial)
			{
				UpgradeRarityGlowImages[Index]->SetBrushFromMaterial(RarityMaterial);
				UpgradeRarityGlowImages[Index]->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				UpgradeRarityGlowImages[Index]->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

UTexture2D* ULevelUpWidget::GetCardBorderForCategory(EUpgradeCategory Category) const
{
	switch (Category)
	{
	case EUpgradeCategory::Samurai: return SamuraiCardBorder;
	case EUpgradeCategory::Ninja: return NinjaCardBorder;
	case EUpgradeCategory::Synergy: return SynergyCardBorder;
	case EUpgradeCategory::Global: return GlobalCardBorder;
	default: return GlobalCardBorder;
	}
}

UMaterialInterface* ULevelUpWidget::GetRarityMaterial(EUpgradeRarity Rarity) const
{
	switch (Rarity)
	{
	case EUpgradeRarity::Rare: return RareRarityMaterial;
	case EUpgradeRarity::Epic: return EpicRarityMaterial;
	case EUpgradeRarity::Legendary: return LegendaryRarityMaterial;
	default: return nullptr;
	}
}

void ULevelUpWidget::SetSynergyDiscoveryPresentation_Implementation(bool bIsDiscovery)
{
	EnsureSynergyDiscoveryPresentation();
	if (SynergyDiscoveryBanner)
	{
		SynergyDiscoveryBanner->SetVisibility(bIsDiscovery ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void ULevelUpWidget::EnsureSynergyDiscoveryPresentation()
{
	if (SynergyDiscoveryBanner || !WidgetTree)
	{
		return;
	}

	UWidget* ExistingRoot = WidgetTree->RootWidget;
	if (!ExistingRoot)
	{
		return;
	}

	UCanvasPanel* PresentationRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DiscoveryPresentationRoot"));
	WidgetTree->RootWidget = PresentationRoot;
	UCanvasPanelSlot* ExistingRootSlot = PresentationRoot->AddChildToCanvas(ExistingRoot);
	ExistingRootSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	ExistingRootSlot->SetOffsets(FMargin(0.0f));

	SynergyDiscoveryBanner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SynergyDiscoveryBanner"));
	SynergyDiscoveryBanner->SetBrushColor(FLinearColor(0.075f, 0.025f, 0.12f, 0.94f));
	SynergyDiscoveryBanner->SetPadding(FMargin(38.0f, 14.0f, 38.0f, 16.0f));
	SynergyDiscoveryBanner->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBox* TextStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SynergyDiscoveryTextStack"));
	SynergyDiscoveryBanner->SetContent(TextStack);

	SynergyDiscoveryTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SynergyDiscoveryTitle"));
	SynergyDiscoveryTitle->SetText(FText::FromString(TEXT("SYNERGY DISCOVERED")));
	SynergyDiscoveryTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.72f, 0.20f)));
	SynergyDiscoveryTitle->SetJustification(ETextJustify::Center);
	FSlateFontInfo TitleFont = SynergyDiscoveryTitle->GetFont();
	TitleFont.Size = 30;
	TitleFont.TypefaceFontName = TEXT("Bold");
	SynergyDiscoveryTitle->SetFont(TitleFont);
	TextStack->AddChildToVerticalBox(SynergyDiscoveryTitle);

	SynergyDiscoverySubtitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SynergyDiscoverySubtitle"));
	SynergyDiscoverySubtitle->SetText(FText::FromString(TEXT("Choose one Synergy to permanently unlock.")));
	SynergyDiscoverySubtitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.88f, 0.97f)));
	SynergyDiscoverySubtitle->SetJustification(ETextJustify::Center);
	FSlateFontInfo SubtitleFont = SynergyDiscoverySubtitle->GetFont();
	SubtitleFont.Size = 17;
	SynergyDiscoverySubtitle->SetFont(SubtitleFont);
	UVerticalBoxSlot* SubtitleSlot = TextStack->AddChildToVerticalBox(SynergyDiscoverySubtitle);
	SubtitleSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	UCanvasPanelSlot* BannerSlot = PresentationRoot->AddChildToCanvas(SynergyDiscoveryBanner);
	BannerSlot->SetAnchors(FAnchors(0.5f, 0.0f));
	BannerSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	BannerSlot->SetPosition(FVector2D(0.0f, 38.0f));
	BannerSlot->SetAutoSize(true);
	BannerSlot->SetZOrder(100);
}
