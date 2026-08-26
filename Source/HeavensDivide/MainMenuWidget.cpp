// Copyright Epic Games, Inc. All Rights Reserved.

#include "MainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HeavensDivideGameUserSettings.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "UpgradeDefinition.h"

namespace MainMenuCopy
{
	static const FText Title = FText::FromString(TEXT("HEAVENS DIVIDE"));
	static const FText ResetTitle = FText::FromString(TEXT("RESET ALL PROGRESS?"));
	static const FText ResetBody = FText::FromString(TEXT("This will permanently erase your unlocked Synergies and Twin Soul discovery progress."));
}

void UMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildMenu();
	ShowMainPanel();
}

void UMainMenuWidget::BuildMenu()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MainMenuRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuBackground"));
	Background->SetBrushColor(FLinearColor(0.012f, 0.018f, 0.032f, 1.0f));
	Root->AddChildToOverlay(Background);

	MenuSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("MenuSwitcher"));
	UOverlaySlot* SwitcherSlot = Root->AddChildToOverlay(MenuSwitcher);
	SwitcherSlot->SetHorizontalAlignment(HAlign_Center);
	SwitcherSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* MainPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainPanel"));
	MenuSwitcher->AddChild(MainPanel);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GameTitle"));
	Title->SetText(MainMenuCopy::Title);
	Title->SetJustification(ETextJustify::Center);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.93f, 0.76f, 0.34f)));
	FSlateFontInfo TitleFont = Title->GetFont();
	TitleFont.Size = 54;
	TitleFont.TypefaceFontName = TEXT("Bold");
	Title->SetFont(TitleFont);
	UVerticalBoxSlot* TitleSlot = MainPanel->AddChildToVerticalBox(Title);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 38.0f));

	NewRunButton = AddMenuButton(MainPanel, FText::FromString(TEXT("NEW RUN")), TEXT("NewRunButton"));
	UButton* CollectionButton = AddMenuButton(MainPanel, FText::FromString(TEXT("COLLECTION")), TEXT("CollectionButton"));
	UButton* SettingsButton = AddMenuButton(MainPanel, FText::FromString(TEXT("SETTINGS")), TEXT("SettingsButton"));
	UButton* ResetButton = AddMenuButton(MainPanel, FText::FromString(TEXT("RESET PROGRESS")), TEXT("ResetProgressButton"));
	UButton* ExitButton = AddMenuButton(MainPanel, FText::FromString(TEXT("EXIT GAME")), TEXT("ExitGameButton"));
	NewRunButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleNewRun);
	CollectionButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleCollection);
	SettingsButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSettings);
	ResetButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleResetProgress);
	ExitButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleExitGame);

	MenuSwitcher->AddChild(BuildCollectionPanel());
	MenuSwitcher->AddChild(BuildSettingsPanel());

	ResetConfirmationOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResetConfirmationOverlay"));
	ResetConfirmationOverlay->SetBrushColor(FLinearColor(0.035f, 0.01f, 0.015f, 0.98f));
	ResetConfirmationOverlay->SetPadding(FMargin(46.0f, 34.0f));
	UOverlaySlot* ResetOverlaySlot = Root->AddChildToOverlay(ResetConfirmationOverlay);
	ResetOverlaySlot->SetHorizontalAlignment(HAlign_Center);
	ResetOverlaySlot->SetVerticalAlignment(VAlign_Center);
	UVerticalBox* ResetBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ResetConfirmationOverlay->SetContent(ResetBox);
	UTextBlock* ResetTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ResetTitle->SetText(MainMenuCopy::ResetTitle);
	ResetTitle->SetJustification(ETextJustify::Center);
	ResetTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.30f, 0.25f)));
	FSlateFontInfo ResetFont = ResetTitle->GetFont();
	ResetFont.Size = 32;
	ResetFont.TypefaceFontName = TEXT("Bold");
	ResetTitle->SetFont(ResetFont);
	ResetBox->AddChildToVerticalBox(ResetTitle)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	UTextBlock* ResetBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ResetBody->SetText(MainMenuCopy::ResetBody);
	ResetBody->SetAutoWrapText(true);
	ResetBody->SetWrapTextAt(520.0f);
	ResetBody->SetJustification(ETextJustify::Center);
	ResetBody->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ResetBox->AddChildToVerticalBox(ResetBody)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
	UButton* CancelButton = AddMenuButton(ResetBox, FText::FromString(TEXT("CANCEL")), TEXT("CancelResetButton"));
	UButton* ConfirmButton = AddMenuButton(ResetBox, FText::FromString(TEXT("RESET")), TEXT("ConfirmResetButton"));
	CancelButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleCancelReset);
	ConfirmButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleConfirmReset);
	SetResetConfirmationVisible(false);
}

UVerticalBox* UMainMenuWidget::BuildCollectionPanel()
{
	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CollectionPanel"));
	auto AddCenteredText = [this, Panel](FName Name, const FText& Text, int32 Size, const FLinearColor& Color, float BottomPadding)
	{
		UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Block->SetText(Text);
		Block->SetJustification(ETextJustify::Center);
		Block->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Panel->AddChildToVerticalBox(Block)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
		return Block;
	};
	AddCenteredText(TEXT("CollectionTitle"), FText::FromString(TEXT("COLLECTION")), 38, FLinearColor(0.93f, 0.76f, 0.34f), 18.0f);
	AddCenteredText(TEXT("SynergiesSectionTitle"), FText::FromString(TEXT("SYNERGIES")), 26, FLinearColor::White, 8.0f);
	CollectionDiscoveryCountText = AddCenteredText(TEXT("CollectionDiscoveryCount"), FText::GetEmpty(), 20, FLinearColor(0.80f, 0.82f, 0.88f), 5.0f);
	CollectionProgressText = AddCenteredText(TEXT("CollectionDiscoveryProgress"), FText::GetEmpty(), 17, FLinearColor(0.66f, 0.58f, 0.82f), 18.0f);
	SynergyCollectionGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("SynergyCollectionGrid"));
	SynergyCollectionGrid->SetMinDesiredSlotWidth(340.0f);
	SynergyCollectionGrid->SetMinDesiredSlotHeight(150.0f);
	Panel->AddChildToVerticalBox(SynergyCollectionGrid)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	UButton* BackButton = AddMenuButton(Panel, FText::FromString(TEXT("BACK")), TEXT("CollectionBackButton"));
	BackButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleBack);
	return Panel;
}

void UMainMenuWidget::RefreshCollection()
{
	if (!SynergyCollectionGrid || !CollectionDiscoveryCountText || !CollectionProgressText)
	{
		return;
	}
	SynergyCollectionGrid->ClearChildren();
	USynergyMetaProgressionSubsystem* Meta = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
	if (!Meta)
	{
		CollectionDiscoveryCountText->SetText(FText::FromString(TEXT("COLLECTION UNAVAILABLE")));
		CollectionProgressText->SetText(FText::GetEmpty());
		return;
	}

	const TArray<UUpgradeDefinition*> Definitions = Meta->GetSynergyUpgradeDefinitions();
	int32 UnlockedCount = 0;
	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		UUpgradeDefinition* Definition = Definitions[Index];
		const bool bUnlocked = Definition && Meta->IsSynergyUpgradeUnlocked(Definition->MetaUnlockId);
		UnlockedCount += bUnlocked ? 1 : 0;
		AddSynergyCollectionCard(Definition, bUnlocked, Index);
	}

	CollectionDiscoveryCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d DISCOVERED"), UnlockedCount, Definitions.Num())));
	if (Definitions.Num() > 0 && UnlockedCount == Definitions.Num())
	{
		CollectionProgressText->SetText(FText::FromString(TEXT("ALL SYNERGIES DISCOVERED")));
	}
	else
	{
		CollectionProgressText->SetText(FText::FromString(FString::Printf(TEXT("Next Discovery: %d / %d"),
			Meta->GetTwinSoulDiscoveryProgress(), Meta->GetTwinSoulCompletionsPerDiscovery())));
	}
}

UVerticalBox* UMainMenuWidget::BuildSettingsPanel()
{
	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsPanel"));

	UTextBlock* Heading = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SettingsHeading"));
	Heading->SetText(FText::FromString(TEXT("SETTINGS")));
	Heading->SetJustification(ETextJustify::Center);
	Heading->SetColorAndOpacity(FSlateColor(FLinearColor(0.93f, 0.76f, 0.34f)));
	FSlateFontInfo HeadingFont = Heading->GetFont();
	HeadingFont.Size = 36;
	HeadingFont.TypefaceFontName = TEXT("Bold");
	Heading->SetFont(HeadingFont);
	Panel->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 28.0f));

	UHorizontalBox* SettingRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("AutoTargetingRow"));
	Panel->AddChildToVerticalBox(SettingRow)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 28.0f));

	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AutoTargetingLabel"));
	Label->SetText(FText::FromString(TEXT("Auto Targeting")));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo LabelFont = Label->GetFont();
	LabelFont.Size = 22;
	Label->SetFont(LabelFont);
	UHorizontalBoxSlot* LabelSlot = SettingRow->AddChildToHorizontalBox(Label);
	LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 28.0f, 0.0f));
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	AutoTargetingCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("AutoTargetingCheckBox"));
	SettingRow->AddChildToHorizontalBox(AutoTargetingCheckBox)->SetVerticalAlignment(VAlign_Center);
	AutoTargetingCheckBox->OnCheckStateChanged.AddDynamic(this, &UMainMenuWidget::HandleAutoTargetingChanged);

	AutoTargetingStateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AutoTargetingStateText"));
	AutoTargetingStateText->SetColorAndOpacity(FSlateColor(FLinearColor(0.93f, 0.76f, 0.34f)));
	AutoTargetingStateText->SetFont(LabelFont);
	UHorizontalBoxSlot* StateSlot = SettingRow->AddChildToHorizontalBox(AutoTargetingStateText);
	StateSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));
	StateSlot->SetVerticalAlignment(VAlign_Center);

	UButton* BackButton = AddMenuButton(Panel, FText::FromString(TEXT("BACK")), TEXT("SettingsBackButton"));
	BackButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleBack);
	RefreshAutoTargetingSetting();
	return Panel;
}

void UMainMenuWidget::RefreshAutoTargetingSetting()
{
	const UHeavensDivideGameUserSettings* Settings = UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
	const bool bEnabled = !Settings || Settings->IsAutoTargetingEnabled();
	if (AutoTargetingCheckBox) AutoTargetingCheckBox->SetIsChecked(bEnabled);
	if (AutoTargetingStateText) AutoTargetingStateText->SetText(FText::FromString(bEnabled ? TEXT("ON") : TEXT("OFF")));
}

void UMainMenuWidget::HandleAutoTargetingChanged(bool bIsChecked)
{
	if (UHeavensDivideGameUserSettings* Settings = UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings())
	{
		Settings->SetAutoTargetingEnabled(bIsChecked);
	}
	if (AutoTargetingStateText) AutoTargetingStateText->SetText(FText::FromString(bIsChecked ? TEXT("ON") : TEXT("OFF")));
}

void UMainMenuWidget::AddSynergyCollectionCard(UUpgradeDefinition* Definition, bool bUnlocked, int32 CardIndex)
{
	if (!Definition || !SynergyCollectionGrid) return;
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Card->SetBrushColor(bUnlocked ? FLinearColor(0.09f, 0.12f, 0.19f, 0.98f) : FLinearColor(0.035f, 0.04f, 0.055f, 0.92f));
	Card->SetPadding(FMargin(20.0f, 15.0f));
	UVerticalBox* CardContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(CardContent);
	auto AddCardText = [this, CardContent](const FText& Text, int32 Size, const FLinearColor& Color, float BottomPadding)
	{
		UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Block->SetText(Text);
		Block->SetAutoWrapText(true);
		Block->SetWrapTextAt(300.0f);
		Block->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		CardContent->AddChildToVerticalBox(Block)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
	};
	AddCardText(bUnlocked ? Definition->DisplayName : FText::FromString(TEXT("???")), 22,
		bUnlocked ? FLinearColor(0.93f, 0.76f, 0.34f) : FLinearColor(0.42f, 0.43f, 0.47f), 8.0f);
	AddCardText(bUnlocked ? Definition->Description : FText::FromString(TEXT("Not yet discovered.")), 15,
		bUnlocked ? FLinearColor(0.88f, 0.89f, 0.92f) : FLinearColor(0.36f, 0.37f, 0.40f), 10.0f);
	AddCardText(FText::FromString(bUnlocked ? TEXT("UNLOCKED") : TEXT("LOCKED")), 14,
		bUnlocked ? FLinearColor(0.30f, 0.85f, 0.48f) : FLinearColor(0.48f, 0.30f, 0.32f), 0.0f);
	UUniformGridSlot* CardSlot = SynergyCollectionGrid->AddChildToUniformGrid(Card, CardIndex / 2, CardIndex % 2);
	CardSlot->SetHorizontalAlignment(HAlign_Fill);
	CardSlot->SetVerticalAlignment(VAlign_Fill);
}

UButton* UMainMenuWidget::AddMenuButton(UVerticalBox* Parent, const FText& Label, FName WidgetName)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);
	Button->SetBackgroundColor(FLinearColor(0.12f, 0.15f, 0.22f, 0.96f));
	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Label);
	LabelText->SetJustification(ETextJustify::Center);
	LabelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo ButtonFont = LabelText->GetFont();
	ButtonFont.Size = 22;
	LabelText->SetFont(ButtonFont);
	Button->AddChild(LabelText);
	UVerticalBoxSlot* ButtonSlot = Parent->AddChildToVerticalBox(Button);
	ButtonSlot->SetPadding(FMargin(0.0f, 5.0f));
	ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	return Button;
}

void UMainMenuWidget::ShowMainPanel()
{
	SetResetConfirmationVisible(false);
	if (MenuSwitcher) MenuSwitcher->SetActiveWidgetIndex(0);
	if (NewRunButton) NewRunButton->SetKeyboardFocus();
}

void UMainMenuWidget::ShowCollectionPanel()
{
	SetResetConfirmationVisible(false);
	RefreshCollection();
	if (MenuSwitcher) MenuSwitcher->SetActiveWidgetIndex(1);
}

void UMainMenuWidget::ShowSettingsPanel()
{
	SetResetConfirmationVisible(false);
	RefreshAutoTargetingSetting();
	if (MenuSwitcher) MenuSwitcher->SetActiveWidgetIndex(2);
}

void UMainMenuWidget::ShowResetConfirmation()
{
	SetResetConfirmationVisible(true);
}

void UMainMenuWidget::SetResetConfirmationVisible(bool bVisible)
{
	bResetConfirmationOpen = bVisible;
	if (ResetConfirmationOverlay)
	{
		ResetConfirmationOverlay->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UMainMenuWidget::HandleNewRun()
{
	UGameplayStatics::OpenLevel(this, TEXT("Lvl_B1_Arena"));
}

void UMainMenuWidget::HandleCollection() { ShowCollectionPanel(); }
void UMainMenuWidget::HandleSettings() { ShowSettingsPanel(); }
void UMainMenuWidget::HandleResetProgress() { ShowResetConfirmation(); }
void UMainMenuWidget::HandleBack() { ShowMainPanel(); }
void UMainMenuWidget::HandleCancelReset() { SetResetConfirmationVisible(false); }

void UMainMenuWidget::HandleConfirmReset()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USynergyMetaProgressionSubsystem* Meta = GameInstance->GetSubsystem<USynergyMetaProgressionSubsystem>())
		{
			Meta->ResetMetaProgression();
			RefreshCollection();
		}
	}
	SetResetConfirmationVisible(false);
}

void UMainMenuWidget::HandleExitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

FReply UMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (bResetConfirmationOpen)
		{
			SetResetConfirmationVisible(false);
			return FReply::Handled();
		}
		if (MenuSwitcher && MenuSwitcher->GetActiveWidgetIndex() != 0)
		{
			ShowMainPanel();
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
