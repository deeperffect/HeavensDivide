// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelUpWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
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
	ShowUpgradeChoices(PlayerUpgrades ? PlayerUpgrades->GetCurrentUpgradeChoices() : TArray<UUpgradeDefinition*>());
	ShowUpgradeOffers(PlayerUpgrades ? PlayerUpgrades->GetCurrentUpgradeOffers() : TArray<FUpgradeOffer>());
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
	ShowUpgradeChoices(PlayerUpgrades ? PlayerUpgrades->GetCurrentUpgradeChoices() : TArray<UUpgradeDefinition*>());
	ShowUpgradeOffers(PlayerUpgrades ? PlayerUpgrades->GetCurrentUpgradeOffers() : TArray<FUpgradeOffer>());
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
