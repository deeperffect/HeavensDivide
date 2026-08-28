// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelUpWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
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
	if (!PlayerUpgrades)
	{
		ShowCategoryChoices(TArray<EUpgradeCategory>());
		return;
	}

	ShowCategoryChoices(PlayerUpgrades->GetCurrentCategoryChoices());
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
	RefreshControllerFocus();
	return UpgradeChoices.Num() > 0;
}

void ULevelUpWidget::InitializeControllerNavigation()
{
	SetIsFocusable(true);
	ControllerFocusedChoiceIndex = 0;
	bControllerAnalogNavigationHeld = false;
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
	ControllerFocusedChoiceIndex = (ControllerFocusedChoiceIndex + Direction + ChoiceCount) % ChoiceCount;
	RefreshControllerFocus();
}

void ULevelUpWidget::RefreshControllerFocus()
{
	const int32 ChoiceCount = GetVisibleChoiceCount();
	ControllerFocusedChoiceIndex = ChoiceCount > 0 ? FMath::Clamp(ControllerFocusedChoiceIndex, 0, ChoiceCount - 1) : 0;
	RebuildFocusableChoices();
	SetControllerFocusPresentation(ControllerFocusedChoiceIndex, !bCategoryChoiceCommitted);

	for (int32 Index = 0; Index < FocusableChoiceButtons.Num(); ++Index)
	{
		if (UButton* Button = FocusableChoiceButtons[Index])
		{
			Button->SetRenderScale(Index == ControllerFocusedChoiceIndex ? FVector2D(1.045f) : FVector2D(1.0f));
		}
	}

	// The Blueprint buttons are mouse click targets with focus disabled. The focusable
	// native widget owns controller input and the indexed buttons provide presentation.
	SetUserFocus(SurvivorPlayerController);
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

void ULevelUpWidget::EnsureUpgradeCardVisualStructure()
{
	if (!WidgetTree || UpgradeArtworkImages.Num() == 3)
	{
		return;
	}

	UpgradeArtworkImages.Reset();
	UpgradeBorderImages.Reset();
	constexpr float CardWidth = 400.0f;
	constexpr float CardHeight = CardWidth * 1.5f;
	constexpr float ArtworkInset = 22.0f;
	constexpr float TextInset = 26.0f;

	for (int32 Index = 0; Index < 3; ++Index)
	{
		UButton* UpgradeButton = Cast<UButton>(WidgetTree->FindWidget(*FString::Printf(TEXT("UpgradeButton_%d"), Index)));
		if (!UpgradeButton)
		{
			UpgradeArtworkImages.Add(nullptr);
			UpgradeBorderImages.Add(nullptr);
			continue;
		}

		// Do not rebuild a slot if this widget instance is initialized more than once.
		if (UImage* ExistingArtwork = Cast<UImage>(WidgetTree->FindWidget(*FString::Printf(TEXT("UpgradeArtwork_%d"), Index))))
		{
			UpgradeArtworkImages.Add(ExistingArtwork);
			UpgradeBorderImages.Add(Cast<UImage>(WidgetTree->FindWidget(*FString::Printf(TEXT("UpgradeBorder_%d"), Index))));
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
		CardSize->SetWidthOverride(CardWidth);
		CardSize->SetHeightOverride(CardHeight);

		UOverlay* CardLayers = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(), *FString::Printf(TEXT("UpgradeCardLayers_%d"), Index));
		CardSize->SetContent(CardLayers);
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
	}
}

void ULevelUpWidget::RefreshUpgradeCardVisuals()
{
	EnsureUpgradeCardVisualStructure();
	const TArray<UUpgradeDefinition*> Choices = PlayerUpgrades
		? PlayerUpgrades->GetCurrentUpgradeChoices()
		: TArray<UUpgradeDefinition*>();

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
