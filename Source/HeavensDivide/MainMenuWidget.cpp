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
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetSwitcherSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HeavensDivideGameUserSettings.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "UpgradeDefinition.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "Materials/MaterialInterface.h"

namespace MainMenuCopy
{
	static const FText Title = FText::FromString(TEXT("HEAVENS DIVIDE"));
	static const FText ResetTitle = FText::FromString(TEXT("RESET ALL PROGRESS?"));
	static const FText ResetBody = FText::FromString(TEXT("This will permanently erase your unlocked Synergies and Twin Soul discovery progress."));
}

void UCollectionUpgradeTileButton::InitializeCollectionTile(UMainMenuWidget* InOwner, UUpgradeDefinition* InDefinition)
{
	CollectionOwner = InOwner;
	Definition = InDefinition;
	OnClicked.AddUniqueDynamic(this, &UCollectionUpgradeTileButton::HandleTileClicked);
	OnHovered.AddUniqueDynamic(this, &UCollectionUpgradeTileButton::HandleTileHovered);
}

void UCollectionUpgradeTileButton::HandleTileClicked()
{
	if (CollectionOwner) CollectionOwner->PreviewCollectionUpgrade(Definition, true);
}

void UCollectionUpgradeTileButton::HandleTileHovered()
{
	if (CollectionOwner) CollectionOwner->PreviewCollectionUpgrade(Definition, false);
}

void UMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildMenu();
	ShowMainPanel();
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	StartBackgroundMedia();
	ShowMainPanel();
}

void UMainMenuWidget::NativeDestruct()
{
	if (BackgroundMediaPlayer)
	{
		BackgroundMediaPlayer->OnMediaOpened.RemoveDynamic(this, &UMainMenuWidget::HandleBackgroundMediaOpened);
		BackgroundMediaPlayer->Close();
	}
	Super::NativeDestruct();
}

void UMainMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshMenuEntryPresentation(InDeltaTime);
	if (bCollectionOpen)
	{
		// Mouse hover and controller focus must not compete for the details panel.
		// NativeOnMouseMove disables focus presentation; key input enables it again.
		for (int32 Index = 0; bShowFocusHighlight && Index < CollectionTileButtons.Num(); ++Index)
		{
			if (CollectionTileButtons[Index] && CollectionTileButtons[Index]->HasAnyUserFocus()
				&& CollectionDefinitions.IsValidIndex(Index) && SelectedCollectionUpgrade != CollectionDefinitions[Index])
			{
				PreviewCollectionUpgrade(CollectionDefinitions[Index], true);
				break;
			}
		}
		RefreshCollectionTileVisuals();
	}
}

void UMainMenuWidget::BuildMenu()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MainMenuRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuBackgroundFallback"));
	Background->SetBrushColor(FLinearColor(0.008f, 0.009f, 0.012f, 1.0f));
	Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	Root->AddChildToOverlay(Background);

	UScaleBox* BackgroundScaler = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("MenuVideoScaler"));
	BackgroundScaler->SetStretch(EStretch::ScaleToFill);
	BackgroundScaler->SetStretchDirection(EStretchDirection::Both);
	BackgroundScaler->SetClipping(EWidgetClipping::ClipToBounds);
	BackgroundScaler->SetVisibility(ESlateVisibility::HitTestInvisible);
	Root->AddChildToOverlay(BackgroundScaler);
	UImage* BackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MenuVideoImage"));
	FSlateBrush BackgroundBrush;
	BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
	BackgroundBrush.SetImageSize(FVector2D(1920.0f, 1080.0f));
	BackgroundBrush.SetResourceObject(BackgroundMediaMaterial
		? static_cast<UObject*>(BackgroundMediaMaterial)
		: static_cast<UObject*>(BackgroundMediaTexture));
	BackgroundImage->SetBrush(BackgroundBrush);
	BackgroundImage->SetOpacity(BackgroundBrush.GetResourceObject() ? 1.0f : 0.0f);
	BackgroundImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	BackgroundScaler->SetContent(BackgroundImage);

	UBorder* ReadabilityOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuReadabilityOverlay"));
	ReadabilityOverlay->SetBrushColor(FLinearColor(0.005f, 0.006f, 0.009f, FMath::Clamp(ReadabilityOverlayOpacity, 0.0f, 1.0f)));
	ReadabilityOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	Root->AddChildToOverlay(ReadabilityOverlay);

	MenuSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("MenuSwitcher"));
	UOverlaySlot* SwitcherSlot = Root->AddChildToOverlay(MenuSwitcher);
	SwitcherSlot->SetHorizontalAlignment(HAlign_Fill);
	SwitcherSlot->SetVerticalAlignment(VAlign_Fill);

	UCanvasPanel* MainPage = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MainPage"));
	MenuSwitcher->AddChild(MainPage);
	UVerticalBox* MainPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainPanel"));
	UCanvasPanelSlot* MainPanelSlot = MainPage->AddChildToCanvas(MainPanel);
	MainPanelSlot->SetAnchors(FAnchors(0.065f, 0.5f));
	MainPanelSlot->SetAlignment(FVector2D(0.0f, 0.5f));
	MainPanelSlot->SetAutoSize(true);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GameTitle"));
	Title->SetText(MainMenuCopy::Title);
	Title->SetJustification(ETextJustify::Left);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.94f, 0.92f)));
	FSlateFontInfo TitleFont = Title->GetFont();
	TitleFont.Size = 44;
	TitleFont.TypefaceFontName = TEXT("Bold");
	Title->SetFont(TitleFont);
	UVerticalBoxSlot* TitleSlot = MainPanel->AddChildToVerticalBox(Title);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, MainMenuLogoBottomSpacing));
	Title->SetVisibility(MainMenuLogo ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	USizeBox* LogoSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MainMenuLogoSize"));
	LogoSize->SetWidthOverride(FMath::Max(1.0f, MainMenuLogoSize.X));
	LogoSize->SetHeightOverride(FMath::Max(1.0f, MainMenuLogoSize.Y));
	LogoSize->SetRenderTranslation(MainMenuLogoOffset);
	LogoSize->SetVisibility(MainMenuLogo ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	UImage* LogoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MainMenuLogo"));
	LogoImage->SetBrushFromTexture(MainMenuLogo, true);
	LogoImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	LogoSize->SetContent(LogoImage);
	UVerticalBoxSlot* LogoSlot = MainPanel->AddChildToVerticalBox(LogoSize);
	LogoSlot->SetHorizontalAlignment(HAlign_Left);
	LogoSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, MainMenuLogoBottomSpacing));

	UVerticalBox* MainButtonStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainButtonStack"));
	MainButtonStack->SetRenderTranslation(MainMenuButtonsOffset);
	MainPanel->AddChildToVerticalBox(MainButtonStack)->SetHorizontalAlignment(HAlign_Left);
	NewRunButton = AddMenuButton(MainButtonStack, FText::FromString(TEXT("NEW RUN")), TEXT("NewRunButton"));
	CollectionMenuButton = AddMenuButton(MainButtonStack, FText::FromString(TEXT("COLLECTION")), TEXT("CollectionButton"));
	UButton* SettingsButton = AddMenuButton(MainButtonStack, FText::FromString(TEXT("SETTINGS")), TEXT("SettingsButton"));
	UButton* ResetButton = AddMenuButton(MainButtonStack, FText::FromString(TEXT("RESET PROGRESS")), TEXT("ResetProgressButton"));
	UButton* ExitButton = AddMenuButton(MainButtonStack, FText::FromString(TEXT("EXIT GAME")), TEXT("ExitGameButton"));
	NewRunButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleNewRun);
	CollectionMenuButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleCollection);
	SettingsButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSettings);
	ResetButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleResetProgress);
	ExitButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleExitGame);

	CollectionOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CollectionOverlay"));
	CollectionOverlay->SetBrushColor(FLinearColor(0.008f, 0.010f, 0.016f, 0.94f));
	CollectionOverlay->SetPadding(FMargin(28.0f, 22.0f));
	CollectionOverlay->SetRenderTranslation(CollectionPageOffset);
	CollectionOverlay->SetClipping(EWidgetClipping::ClipToBounds);
	UScaleBox* CollectionScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("CollectionResolutionScale"));
	CollectionScale->SetStretch(EStretch::ScaleToFit);
	CollectionScale->SetStretchDirection(EStretchDirection::DownOnly);
	USizeBox* CollectionDesignSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CollectionDesignSize"));
	CollectionDesignSize->SetWidthOverride(1030.0f);
	CollectionDesignSize->SetHeightOverride(780.0f);
	CollectionDesignSize->SetContent(BuildCollectionPanel());
	CollectionScale->SetContent(CollectionDesignSize);
	CollectionOverlay->SetContent(CollectionScale);
	UOverlaySlot* CollectionOverlaySlot = Root->AddChildToOverlay(CollectionOverlay);
	CollectionOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
	CollectionOverlaySlot->SetVerticalAlignment(VAlign_Fill);
	CollectionOverlaySlot->SetPadding(FMargin(410.0f, 24.0f, 24.0f, 24.0f));
	SetCollectionVisible(false);

	SettingsPopupOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SettingsPopupOverlay"));
	SettingsPopupOverlay->SetBrushColor(SettingsPopupBackgroundColor);
	SettingsPopupOverlay->SetPadding(SettingsPopupPadding);
	UOverlaySlot* SettingsOverlaySlot = Root->AddChildToOverlay(SettingsPopupOverlay);
	SettingsOverlaySlot->SetHorizontalAlignment(HAlign_Center);
	SettingsOverlaySlot->SetVerticalAlignment(VAlign_Center);
	SettingsPopupOverlay->SetContent(BuildSettingsPanel());
	SetSettingsPopupVisible(false);

	ResetConfirmationOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResetConfirmationOverlay"));
	ResetConfirmationOverlay->SetBrushColor(ResetPopupBackgroundColor);
	ResetConfirmationOverlay->SetPadding(ResetPopupPadding);
	ResetConfirmationOverlay->SetRenderTranslation(ResetPopupOffset);
	UOverlaySlot* ResetOverlaySlot = Root->AddChildToOverlay(ResetConfirmationOverlay);
	ResetOverlaySlot->SetHorizontalAlignment(HAlign_Center);
	ResetOverlaySlot->SetVerticalAlignment(VAlign_Center);
	UVerticalBox* ResetBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ResetConfirmationOverlay->SetContent(ResetBox);
	UTextBlock* ResetTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ResetTitle->SetText(MainMenuCopy::ResetTitle);
	ResetTitle->SetJustification(ETextJustify::Center);
	ResetTitle->SetColorAndOpacity(FSlateColor(ResetPopupTitleColor));
	FSlateFontInfo ResetFont = SecondaryHeadingFont.Size > 0 ? SecondaryHeadingFont : ResetTitle->GetFont();
	if (SecondaryHeadingFont.Size <= 0)
	{
		ResetFont.Size = 32;
		ResetFont.TypefaceFontName = TEXT("Bold");
	}
	ResetTitle->SetFont(ResetFont);
	ResetBox->AddChildToVerticalBox(ResetTitle)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	UTextBlock* ResetBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ResetBody->SetText(MainMenuCopy::ResetBody);
	ResetBody->SetAutoWrapText(true);
	ResetBody->SetWrapTextAt(520.0f);
	ResetBody->SetJustification(ETextJustify::Center);
	ResetBody->SetColorAndOpacity(FSlateColor(SecondaryBodyColor));
	if (SecondaryBodyFont.Size > 0) ResetBody->SetFont(SecondaryBodyFont);
	ResetBox->AddChildToVerticalBox(ResetBody)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
	UButton* CancelButton = AddMenuButton(ResetBox, FText::FromString(TEXT("CANCEL")), TEXT("CancelResetButton"));
	UButton* ConfirmButton = AddMenuButton(ResetBox, FText::FromString(TEXT("RESET")), TEXT("ConfirmResetButton"));
	CancelButton->SetRenderTranslation(SecondaryButtonsOffset);
	ConfirmButton->SetRenderTranslation(SecondaryButtonsOffset);
	CancelButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleCancelReset);
	ConfirmButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleConfirmReset);
	SetResetConfirmationVisible(false);
}

UVerticalBox* UMainMenuWidget::BuildCollectionPanel()
{
	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CollectionPanel"));
	auto MakeText = [this](FName Name, const FString& Value, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(Value));
		Text->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = SecondaryBodyFont.Size > 0 ? SecondaryBodyFont : Text->GetFont();
		if (SecondaryBodyFont.Size <= 0) Font.Size = Size;
		Text->SetFont(Font);
		return Text;
	};

	UTextBlock* Title = MakeText(TEXT("CollectionTitle"), TEXT("COLLECTION"), 36, SecondaryHeadingColor);
	if (SecondaryHeadingFont.Size > 0) Title->SetFont(SecondaryHeadingFont);
	Panel->AddChildToVerticalBox(Title)->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 3.0f));
	Panel->AddChildToVerticalBox(MakeText(TEXT("CollectionSubtitle"), TEXT("DISCOVERED TECHNIQUES & SYNERGIES"), 14,
		FLinearColor(0.55f, 0.57f, 0.62f)))->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 18.0f));

	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CollectionTabs"));
	Panel->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	auto AddTab = [this, Tabs, MakeText](FName Name, const FString& Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style = Button->GetStyle();
		FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.SetNormal(Empty); Style.SetHovered(Empty); Style.SetPressed(Empty);
		Button->SetStyle(Style);
		Button->AddChild(MakeText(NAME_None, Label, 20, SecondaryBodyColor));
		UHorizontalBoxSlot* Slot = Tabs->AddChildToHorizontalBox(Button);
		Slot->SetPadding(FMargin(10.0f, 4.0f, 28.0f, 4.0f));
		return Button;
	};
	SamuraiCollectionTab = AddTab(TEXT("SamuraiCollectionTab"), TEXT("SAMURAI"));
	NinjaCollectionTab = AddTab(TEXT("NinjaCollectionTab"), TEXT("NINJA"));
	SynergyCollectionTab = AddTab(TEXT("SynergyCollectionTab"), TEXT("SYNERGY"));
	SamuraiCollectionTab->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSamuraiCollectionTab);
	NinjaCollectionTab->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleNinjaCollectionTab);
	SynergyCollectionTab->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSynergyCollectionTab);

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CollectionBody"));
	Panel->AddChildToVerticalBox(Body);
	UVerticalBox* Library = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CollectionLibrary"));
	UHorizontalBoxSlot* LibrarySlot = Body->AddChildToHorizontalBox(Library);
	LibrarySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LibrarySlot->SetPadding(FMargin(0.0f, 0.0f, 22.0f, 0.0f));
	USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ScrollSize->SetWidthOverride(570.0f); ScrollSize->SetHeightOverride(510.0f);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CollectionScroll"));
	Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	ScrollSize->SetContent(Scroll);
	SynergyCollectionGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("CollectionGrid"));
	SynergyCollectionGrid->SetMinDesiredSlotWidth(140.0f);
	SynergyCollectionGrid->SetMinDesiredSlotHeight(140.0f);
	Scroll->AddChild(SynergyCollectionGrid);
	Library->AddChildToVerticalBox(ScrollSize);
	CollectionDiscoveryCountText = MakeText(TEXT("CollectionDiscoveryCount"), TEXT(""), 15, FLinearColor(0.62f, 0.65f, 0.70f));
	Library->AddChildToVerticalBox(CollectionDiscoveryCountText)->SetPadding(FMargin(4.0f, 10.0f, 0.0f, 0.0f));
	CollectionProgressText = MakeText(TEXT("CollectionDiscoveryProgress"), TEXT(""), 14, FLinearColor(0.66f, 0.48f, 0.82f));
	Library->AddChildToVerticalBox(CollectionProgressText)->SetPadding(FMargin(4.0f, 3.0f, 0.0f, 0.0f));

	UBorder* Details = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CollectionDetailsPanel"));
	Details->SetBrushColor(FLinearColor(0.025f, 0.029f, 0.038f, 0.98f)); Details->SetPadding(FMargin(24.0f));
	UHorizontalBoxSlot* DetailsSlot = Body->AddChildToHorizontalBox(Details);
	DetailsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	USizeBox* DetailsSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DetailsSize->SetWidthOverride(380.0f); DetailsSize->SetHeightOverride(570.0f); Details->SetContent(DetailsSize);
	UVerticalBox* DetailStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass()); DetailsSize->SetContent(DetailStack);
	USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()); IconSize->SetWidthOverride(235.0f); IconSize->SetHeightOverride(235.0f);
	CollectionDetailIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CollectionDetailIcon"));
	UScaleBox* DetailIconScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	DetailIconScale->SetStretch(EStretch::ScaleToFit); DetailIconScale->SetStretchDirection(EStretchDirection::Both);
	DetailIconScale->SetContent(CollectionDetailIcon); IconSize->SetContent(DetailIconScale);
	UVerticalBoxSlot* DetailIconSlot = DetailStack->AddChildToVerticalBox(IconSize); DetailIconSlot->SetHorizontalAlignment(HAlign_Center); DetailIconSlot->SetPadding(FMargin(0,0,0,14));
	CollectionDetailName = MakeText(TEXT("CollectionDetailName"), TEXT(""), 28, SecondaryHeadingColor);
	CollectionDetailName->SetAutoWrapText(true); CollectionDetailName->SetWrapTextAt(330.0f);
	DetailStack->AddChildToVerticalBox(CollectionDetailName)->SetPadding(FMargin(0,0,0,5));
	CollectionDetailCategory = MakeText(TEXT("CollectionDetailCategory"), TEXT(""), 13, FLinearColor(0.55f,0.58f,0.64f));
	DetailStack->AddChildToVerticalBox(CollectionDetailCategory)->SetPadding(FMargin(0,0,0,18));
	CollectionDetailDescription = MakeText(TEXT("CollectionDetailDescription"), TEXT(""), 17, SecondaryBodyColor);
	CollectionDetailDescription->SetAutoWrapText(true); CollectionDetailDescription->SetWrapTextAt(330.0f);
	UScrollBox* DescriptionScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CollectionDescriptionScroll"));
	DescriptionScroll->SetScrollBarVisibility(ESlateVisibility::Visible); DescriptionScroll->AddChild(CollectionDetailDescription);
	USizeBox* DescriptionSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()); DescriptionSize->SetHeightOverride(150.0f); DescriptionSize->SetContent(DescriptionScroll);
	DetailStack->AddChildToVerticalBox(DescriptionSize)->SetPadding(FMargin(0,0,0,16));
	CollectionDetailStats = MakeText(TEXT("CollectionDetailStats"), TEXT(""), 14, FLinearColor(0.68f,0.70f,0.76f));
	CollectionDetailStats->SetAutoWrapText(true); DetailStack->AddChildToVerticalBox(CollectionDetailStats);

	UButton* BackButton = AddMenuButton(Panel, FText::FromString(TEXT("BACK")), TEXT("CollectionBackButton"));
	BackButton->SetRenderTranslation(SecondaryButtonsOffset);
	BackButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleBack);
	if (UVerticalBoxSlot* BackSlot = Cast<UVerticalBoxSlot>(BackButton->Slot)) BackSlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));
	return Panel;
}

void UMainMenuWidget::RefreshCollection()
{
	if (!SynergyCollectionGrid || !CollectionDiscoveryCountText || !CollectionProgressText)
	{
		return;
	}
	USynergyMetaProgressionSubsystem* Meta = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
	if (!Meta)
	{
		CollectionDiscoveryCountText->SetText(FText::FromString(TEXT("COLLECTION UNAVAILABLE")));
		CollectionProgressText->SetText(FText::GetEmpty());
		return;
	}

	RebuildCollectionGrid();
}

UVerticalBox* UMainMenuWidget::BuildSettingsPanel()
{
	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsPanel"));
	Panel->SetRenderTranslation(SettingsPageOffset);

	UTextBlock* Heading = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SettingsHeading"));
	Heading->SetText(FText::FromString(TEXT("SETTINGS")));
	Heading->SetJustification(ETextJustify::Center);
	Heading->SetColorAndOpacity(FSlateColor(SecondaryHeadingColor));
	FSlateFontInfo HeadingFont = SecondaryHeadingFont.Size > 0 ? SecondaryHeadingFont : Heading->GetFont();
	if (SecondaryHeadingFont.Size <= 0)
	{
		HeadingFont.Size = 36;
		HeadingFont.TypefaceFontName = TEXT("Bold");
	}
	Heading->SetFont(HeadingFont);
	Panel->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 28.0f));

	UHorizontalBox* SettingRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("AutoTargetingRow"));
	Panel->AddChildToVerticalBox(SettingRow)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 28.0f));

	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AutoTargetingLabel"));
	Label->SetText(FText::FromString(TEXT("Auto Targeting")));
	Label->SetColorAndOpacity(FSlateColor(SecondaryBodyColor));
	FSlateFontInfo LabelFont = SecondaryBodyFont.Size > 0 ? SecondaryBodyFont : Label->GetFont();
	if (SecondaryBodyFont.Size <= 0) LabelFont.Size = 22;
	Label->SetFont(LabelFont);
	UHorizontalBoxSlot* LabelSlot = SettingRow->AddChildToHorizontalBox(Label);
	LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 28.0f, 0.0f));
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	AutoTargetingCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("AutoTargetingCheckBox"));
	SettingRow->AddChildToHorizontalBox(AutoTargetingCheckBox)->SetVerticalAlignment(VAlign_Center);
	AutoTargetingCheckBox->OnCheckStateChanged.AddDynamic(this, &UMainMenuWidget::HandleAutoTargetingChanged);

	AutoTargetingStateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AutoTargetingStateText"));
	AutoTargetingStateText->SetColorAndOpacity(FSlateColor(SecondaryHeadingColor));
	AutoTargetingStateText->SetFont(LabelFont);
	UHorizontalBoxSlot* StateSlot = SettingRow->AddChildToHorizontalBox(AutoTargetingStateText);
	StateSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));
	StateSlot->SetVerticalAlignment(VAlign_Center);

	UButton* BackButton = AddMenuButton(Panel, FText::FromString(TEXT("BACK")), TEXT("SettingsBackButton"));
	BackButton->SetRenderTranslation(SecondaryButtonsOffset);
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

void UMainMenuWidget::RebuildCollectionGrid()
{
	if (!SynergyCollectionGrid) return;
	USynergyMetaProgressionSubsystem* Meta = GetGameInstance() ? GetGameInstance()->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
	if (!Meta) return;
	SynergyCollectionGrid->ClearChildren();
	CollectionTileButtons.Reset(); CollectionTileSelectionBorders.Reset(); CollectionTileIcons.Reset();
	CollectionDefinitions = Meta->GetCollectionUpgradeDefinitions(SelectedCollectionCategory);
	if (!CollectionDefinitions.Contains(SelectedCollectionUpgrade)) SelectedCollectionUpgrade = CollectionDefinitions.Num() ? CollectionDefinitions[0] : nullptr;
	int32 UnlockedCount = 0;
	for (int32 Index = 0; Index < CollectionDefinitions.Num(); ++Index)
	{
		UUpgradeDefinition* Definition = CollectionDefinitions[Index];
		const bool bUnlocked = Meta->IsCollectionUpgradeUnlocked(Definition);
		UnlockedCount += bUnlocked ? 1 : 0;
		UCollectionUpgradeTileButton* Tile = WidgetTree->ConstructWidget<UCollectionUpgradeTileButton>(UCollectionUpgradeTileButton::StaticClass());
		Tile->InitializeCollectionTile(this, Definition);
		FButtonStyle Style = Tile->GetStyle(); FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.SetNormal(Empty); Style.SetHovered(Empty); Style.SetPressed(Empty); Tile->SetStyle(Style);
		USizeBox* TileSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		TileSize->SetWidthOverride(134.0f); TileSize->SetHeightOverride(134.0f); Tile->AddChild(TileSize);
		UBorder* Selection = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Selection->SetPadding(FMargin(3.0f)); TileSize->SetContent(Selection);
		UBorder* Inner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Inner->SetBrushColor(bUnlocked ? CollectionUnlockedCardColor : CollectionLockedCardColor); Inner->SetPadding(FMargin(15.0f)); Selection->SetContent(Inner);
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass()); Inner->SetContent(Layers);
		UTexture2D* DisplayTexture = Definition ? (Definition->Icon ? Definition->Icon : Definition->CardArtwork) : nullptr;
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Icon->SetBrushFromTexture(DisplayTexture, true);
		Icon->SetColorAndOpacity(bUnlocked ? FLinearColor::White : FLinearColor(0.28f, 0.24f, 0.24f, 0.55f));
		Icon->SetVisibility(DisplayTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		UScaleBox* IconScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		IconScale->SetStretch(EStretch::ScaleToFit); IconScale->SetStretchDirection(EStretchDirection::Both); IconScale->SetContent(Icon);
		Layers->AddChildToOverlay(IconScale);
		if (DisplayTexture)
		{
			UE_LOG(LogTemp, Log, TEXT("Collection: Icon loaded for [%s] from %s"), *Definition->DisplayName.ToString(),
				Definition->Icon ? TEXT("Icon") : TEXT("CardArtwork fallback"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Collection WARNING: Missing icon for [%s]"), Definition ? *Definition->DisplayName.ToString() : TEXT("Invalid upgrade"));
		}
		if (!bUnlocked)
		{
			UTextBlock* Lock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); Lock->SetText(FText::FromString(TEXT("LOCKED")));
			Lock->SetJustification(ETextJustify::Center); Lock->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f,0.63f,0.66f)));
			FSlateFontInfo Font = Lock->GetFont(); Font.Size = 11; Lock->SetFont(Font);
			UOverlaySlot* LockSlot = Layers->AddChildToOverlay(Lock); LockSlot->SetHorizontalAlignment(HAlign_Center); LockSlot->SetVerticalAlignment(VAlign_Bottom);
		}
		UUniformGridSlot* GridSlot = SynergyCollectionGrid->AddChildToUniformGrid(Tile, Index / 4, Index % 4);
		GridSlot->SetHorizontalAlignment(HAlign_Center); GridSlot->SetVerticalAlignment(VAlign_Center);
		CollectionTileButtons.Add(Tile); CollectionTileSelectionBorders.Add(Selection); CollectionTileIcons.Add(Icon);
	}
	CollectionDiscoveryCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d UNLOCKED"), UnlockedCount, CollectionDefinitions.Num())));
	if (SelectedCollectionCategory == EUpgradeCategory::Synergy && UnlockedCount < CollectionDefinitions.Num())
		CollectionProgressText->SetText(FText::FromString(FString::Printf(TEXT("NEXT DISCOVERY  %d / %d"), Meta->GetTwinSoulDiscoveryProgress(), Meta->GetTwinSoulCompletionsPerDiscovery())));
	else CollectionProgressText->SetText(FText::GetEmpty());
	RefreshCollectionDetails(); RefreshCollectionTileVisuals();
}

void UMainMenuWidget::PreviewCollectionUpgrade(UUpgradeDefinition* Definition, bool bCommitSelection)
{
	(void)bCommitSelection;
	if (!Definition) return;
	SelectedCollectionUpgrade = Definition;
	RefreshCollectionDetails(); RefreshCollectionTileVisuals();
}

void UMainMenuWidget::RefreshCollectionDetails()
{
	if (!CollectionDetailName || !SelectedCollectionUpgrade) return;
	USynergyMetaProgressionSubsystem* Meta = GetGameInstance() ? GetGameInstance()->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
	const bool bUnlocked = Meta && Meta->IsCollectionUpgradeUnlocked(SelectedCollectionUpgrade);
	UTexture2D* DisplayTexture = SelectedCollectionUpgrade->Icon ? SelectedCollectionUpgrade->Icon : SelectedCollectionUpgrade->CardArtwork;
	CollectionDetailIcon->SetBrushFromTexture(DisplayTexture, true);
	CollectionDetailIcon->SetColorAndOpacity(bUnlocked ? FLinearColor::White : FLinearColor(0.24f, 0.21f, 0.21f, 0.48f));
	CollectionDetailIcon->SetVisibility(DisplayTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	CollectionDetailName->SetText(bUnlocked ? SelectedCollectionUpgrade->DisplayName : FText::FromString(TEXT("UNKNOWN TECHNIQUE")));
	const FString Category = SelectedCollectionCategory == EUpgradeCategory::Samurai ? TEXT("SAMURAI UPGRADE")
		: SelectedCollectionCategory == EUpgradeCategory::Ninja ? TEXT("NINJA UPGRADE") : TEXT("TWIN SOUL SYNERGY");
	CollectionDetailCategory->SetText(FText::FromString(Category));
	CollectionDetailDescription->SetText(bUnlocked ? SelectedCollectionUpgrade->Description
		: FText::FromString(TEXT("LOCKED. Find this upgrade during a run to unlock it.")));
	CollectionDetailStats->SetText(bUnlocked ? FText::FromString(FString::Printf(TEXT("MAX LEVEL  %d\nRARITY  %s"),
		SelectedCollectionUpgrade->MaxLevel, *UEnum::GetDisplayValueAsText(SelectedCollectionUpgrade->Rarity).ToString().ToUpper())) : FText::GetEmpty());
}

void UMainMenuWidget::RefreshCollectionTileVisuals()
{
	const FLinearColor Accent = SelectedCollectionCategory == EUpgradeCategory::Samurai ? FLinearColor(0.82f,0.12f,0.10f)
		: SelectedCollectionCategory == EUpgradeCategory::Ninja ? FLinearColor(0.26f,0.45f,0.88f) : FLinearColor(0.88f,0.72f,0.30f);
	for (int32 Index = 0; Index < CollectionTileButtons.Num(); ++Index)
	{
		const bool bSelected = CollectionDefinitions.IsValidIndex(Index) && CollectionDefinitions[Index] == SelectedCollectionUpgrade;
		const bool bHot = bSelected || (CollectionTileButtons[Index] && (CollectionTileButtons[Index]->IsHovered() || CollectionTileButtons[Index]->HasAnyUserFocus()));
		if (CollectionTileSelectionBorders.IsValidIndex(Index) && CollectionTileSelectionBorders[Index])
			CollectionTileSelectionBorders[Index]->SetBrushColor(bHot ? Accent : FLinearColor(0.13f,0.14f,0.17f,1.0f));
	}
	if (SamuraiCollectionTab) SamuraiCollectionTab->SetColorAndOpacity(SelectedCollectionCategory == EUpgradeCategory::Samurai ? FLinearColor(1,0.35f,0.3f) : FLinearColor::White);
	if (NinjaCollectionTab) NinjaCollectionTab->SetColorAndOpacity(SelectedCollectionCategory == EUpgradeCategory::Ninja ? FLinearColor(0.45f,0.65f,1) : FLinearColor::White);
	if (SynergyCollectionTab) SynergyCollectionTab->SetColorAndOpacity(SelectedCollectionCategory == EUpgradeCategory::Synergy ? FLinearColor(1.0f,0.84f,0.42f) : FLinearColor::White);
}

void UMainMenuWidget::HandleSamuraiCollectionTab() { SelectedCollectionCategory = EUpgradeCategory::Samurai; RebuildCollectionGrid(); }
void UMainMenuWidget::HandleNinjaCollectionTab() { SelectedCollectionCategory = EUpgradeCategory::Ninja; RebuildCollectionGrid(); }
void UMainMenuWidget::HandleSynergyCollectionTab() { SelectedCollectionCategory = EUpgradeCategory::Synergy; RebuildCollectionGrid(); }

UButton* UMainMenuWidget::AddMenuButton(UVerticalBox* Parent, const FText& Label, FName WidgetName)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Button->IsFocusable = true;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	FSlateBrush EmptyBrush;
	EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	FButtonStyle TextButtonStyle = Button->GetStyle();
	TextButtonStyle.SetNormal(EmptyBrush);
	TextButtonStyle.SetHovered(EmptyBrush);
	TextButtonStyle.SetPressed(EmptyBrush);
	TextButtonStyle.SetDisabled(EmptyBrush);
	Button->SetStyle(TextButtonStyle);

	USizeBox* EntrySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	EntrySize->SetWidthOverride(340.0f);
	EntrySize->SetHeightOverride(52.0f);
	UOverlay* EntryLayers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	EntrySize->SetContent(EntryLayers);
	USizeBox* InkSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	InkSize->SetHeightOverride(100.0f);
	InkSize->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* InkSlot = EntryLayers->AddChildToOverlay(InkSize);
	InkSlot->SetHorizontalAlignment(HAlign_Fill);
	InkSlot->SetVerticalAlignment(VAlign_Center);
	UImage* InkImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	InkImage->SetBrushFromTexture(InkBrushTexture, true);
	InkImage->SetOpacity(0.0f);
	InkImage->SetRenderTransformPivot(FVector2D(0.0f, 0.5f));
	InkImage->SetRenderScale(FVector2D(0.84f, 1.0f));
	InkImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	InkSize->SetContent(InkImage);
	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Label);
	LabelText->SetJustification(ETextJustify::Left);
	LabelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.73f, 0.75f)));
	FSlateFontInfo ButtonFont = MenuButtonFont.Size > 0 ? MenuButtonFont : LabelText->GetFont();
	if (MenuButtonFont.Size <= 0) ButtonFont.Size = 24;
	LabelText->SetFont(ButtonFont);
	UOverlaySlot* LabelSlot = EntryLayers->AddChildToOverlay(LabelText);
	LabelSlot->SetHorizontalAlignment(HAlign_Left);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	LabelSlot->SetPadding(FMargin(18.0f, 0.0f));
	Button->AddChild(EntrySize);
	UVerticalBoxSlot* ButtonSlot = Parent->AddChildToVerticalBox(Button);
	ButtonSlot->SetPadding(FMargin(0.0f, 2.0f));
	ButtonSlot->SetHorizontalAlignment(HAlign_Left);
	MenuEntryButtons.Add(Button);
	MenuEntryBrushImages.Add(InkImage);
	MenuEntryLabels.Add(LabelText);
	MenuEntryRevealAmounts.Add(0.0f);
	return Button;
}

void UMainMenuWidget::StartBackgroundMedia()
{
	if (!BackgroundMediaPlayer) return;
	BackgroundMediaPlayer->OnMediaOpened.RemoveDynamic(this, &UMainMenuWidget::HandleBackgroundMediaOpened);
	BackgroundMediaPlayer->OnMediaOpened.AddUniqueDynamic(this, &UMainMenuWidget::HandleBackgroundMediaOpened);
	if (BackgroundMediaSource)
	{
		BackgroundMediaPlayer->OpenSource(BackgroundMediaSource);
	}
	else if (BackgroundMediaPlayer->IsReady())
	{
		BackgroundMediaPlayer->Play();
	}
}

void UMainMenuWidget::HandleBackgroundMediaOpened(FString OpenedUrl)
{
	if (BackgroundMediaPlayer)
	{
		BackgroundMediaPlayer->Play();
	}
}

void UMainMenuWidget::RefreshMenuEntryPresentation(float DeltaTime)
{
	const float Speed = 1.0f / FMath::Max(0.05f, InkRevealDuration);
	for (int32 Index = 0; Index < MenuEntryButtons.Num(); ++Index)
	{
		UButton* Button = MenuEntryButtons[Index];
		if (!Button) continue;
		const bool bHighlighted = Button->IsHovered() || (bCollectionOpen && Button == CollectionMenuButton)
			|| (bShowFocusHighlight && Button->HasAnyUserFocus());
		if (!MenuEntryRevealAmounts.IsValidIndex(Index)) MenuEntryRevealAmounts.SetNumZeroed(MenuEntryButtons.Num());
		float& Reveal = MenuEntryRevealAmounts[Index];
		Reveal = FMath::FInterpConstantTo(Reveal, bHighlighted ? 1.0f : 0.0f, DeltaTime, Speed);
		if (MenuEntryBrushImages.IsValidIndex(Index) && MenuEntryBrushImages[Index])
		{
			MenuEntryBrushImages[Index]->SetOpacity(InkBrushTexture ? Reveal : 0.0f);
			MenuEntryBrushImages[Index]->SetRenderScale(FVector2D(FMath::Lerp(0.84f, 1.0f, Reveal), 1.0f));
		}
		if (MenuEntryLabels.IsValidIndex(Index) && MenuEntryLabels[Index])
		{
			const FLinearColor Normal(0.72f, 0.73f, 0.75f);
			const FLinearColor Highlighted(0.01f, 0.01f, 0.01f);
			MenuEntryLabels[Index]->SetColorAndOpacity(FSlateColor(FMath::Lerp(Normal, Highlighted, Reveal)));
		}
	}
}

void UMainMenuWidget::ShowMainPanel()
{
	SetResetConfirmationVisible(false);
	SetSettingsPopupVisible(false);
	SetCollectionVisible(false);
	if (MenuSwitcher) MenuSwitcher->SetActiveWidgetIndex(0);
	if (NewRunButton) NewRunButton->SetUserFocus(GetOwningPlayer());
}

void UMainMenuWidget::ShowCollectionPanel()
{
	SetResetConfirmationVisible(false);
	SetSettingsPopupVisible(false);
	if (MenuSwitcher) MenuSwitcher->SetActiveWidgetIndex(0);
	SetCollectionVisible(true);
	RefreshCollection();
	if (CollectionTileButtons.Num() > 0 && CollectionTileButtons[0]) CollectionTileButtons[0]->SetUserFocus(GetOwningPlayer());
}

void UMainMenuWidget::ShowSettingsPanel()
{
	SetResetConfirmationVisible(false);
	SetCollectionVisible(false);
	RefreshAutoTargetingSetting();
	if (MenuSwitcher) MenuSwitcher->SetActiveWidgetIndex(0);
	SetSettingsPopupVisible(true);
	if (AutoTargetingCheckBox) AutoTargetingCheckBox->SetUserFocus(GetOwningPlayer());
}

void UMainMenuWidget::ShowResetConfirmation()
{
	SetSettingsPopupVisible(false);
	SetCollectionVisible(false);
	SetResetConfirmationVisible(true);
	FocusNamedWidget(TEXT("CancelResetButton"));
}

void UMainMenuWidget::FocusNamedWidget(FName WidgetName)
{
	if (UWidget* Widget = GetWidgetFromName(WidgetName))
	{
		Widget->SetUserFocus(GetOwningPlayer());
	}
}

void UMainMenuWidget::SetResetConfirmationVisible(bool bVisible)
{
	bResetConfirmationOpen = bVisible;
	if (ResetConfirmationOverlay)
	{
		ResetConfirmationOverlay->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UMainMenuWidget::SetSettingsPopupVisible(bool bVisible)
{
	bSettingsPopupOpen = bVisible;
	if (SettingsPopupOverlay)
	{
		SettingsPopupOverlay->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UMainMenuWidget::SetCollectionVisible(bool bVisible)
{
	bCollectionOpen = bVisible;
	if (CollectionOverlay) CollectionOverlay->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UMainMenuWidget::HandleNewRun()
{
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Lvl_B1_Lvl1"));
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
	bShowFocusHighlight = true;
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		if (bResetConfirmationOpen)
		{
			SetResetConfirmationVisible(false);
			return FReply::Handled();
		}
		if (bSettingsPopupOpen)
		{
			ShowMainPanel();
			return FReply::Handled();
		}
		if (bCollectionOpen)
		{
			ShowMainPanel();
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

FReply UMainMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	bShowFocusHighlight = true;
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		if (bResetConfirmationOpen)
		{
			SetResetConfirmationVisible(false);
			ShowMainPanel();
			return FReply::Handled();
		}
		if (bSettingsPopupOpen)
		{
			ShowMainPanel();
			return FReply::Handled();
		}
		if (bCollectionOpen)
		{
			ShowMainPanel();
			return FReply::Handled();
		}
		if (MenuSwitcher && MenuSwitcher->GetActiveWidgetIndex() != 0)
		{
			ShowMainPanel();
			return FReply::Handled();
		}
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UMainMenuWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	bShowFocusHighlight = false;
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}
