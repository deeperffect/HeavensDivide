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
#include "Components/Slider.h"
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

	auto AttachPage = [Root](UBorder* Page)
	{
		UOverlaySlot* PageSlot = Root->AddChildToOverlay(Page);
		PageSlot->SetHorizontalAlignment(HAlign_Fill);
		PageSlot->SetVerticalAlignment(VAlign_Fill);
		PageSlot->SetPadding(FMargin(410.0f, 24.0f, 24.0f, 24.0f));
	};
	CollectionOverlay = BuildSecondaryPageFrame(BuildCollectionPanel(), TEXT("CollectionOverlay"),
		CollectionPageOffset, CollectionContentPadding, FLinearColor(0.008f, 0.010f, 0.016f, 0.94f));
	AttachPage(CollectionOverlay);
	SetCollectionVisible(false);

	auto AddFooterButton = [this](UHorizontalBox* Footer, const FText& Label, FName Name)
	{
		UVerticalBox* ButtonBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UButton* Button = AddMenuButton(ButtonBox, Label, Name);
		if (USizeBox* ButtonSize = Cast<USizeBox>(Button->GetChildAt(0)))
		{
			ButtonSize->SetWidthOverride(FMath::Max(60.0f, CollectionBackButtonSize.X));
			ButtonSize->SetHeightOverride(FMath::Max(1.0f, CollectionBackButtonSize.Y));
		}
		Footer->AddChildToHorizontalBox(ButtonBox)->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));
		return Button;
	};
	UHorizontalBox* SettingsActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	SettingsActions->SetRenderTranslation(SecondaryButtonsOffset);
	UButton* SettingsBack = AddFooterButton(SettingsActions, FText::FromString(TEXT("BACK")), TEXT("SettingsBackButton"));
	SettingsBack->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleBack);
	UHorizontalBox* ResetActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ResetActions->SetRenderTranslation(SecondaryButtonsOffset);
	UButton* CancelButton = AddFooterButton(ResetActions, FText::FromString(TEXT("CANCEL")), TEXT("CancelResetButton"));
	UButton* ConfirmButton = AddFooterButton(ResetActions, FText::FromString(TEXT("RESET")), TEXT("ConfirmResetButton"));
	CancelButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleCancelReset);
	ConfirmButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleConfirmReset);

	SettingsPopupOverlay = BuildSecondaryPageFrame(BuildSettingsPanel(), TEXT("SettingsPopupOverlay"),
		SettingsPageOffset, SettingsPopupPadding, SettingsPopupBackgroundColor, SettingsActions);
	AttachPage(SettingsPopupOverlay);
	SetSettingsPopupVisible(false);

	UVerticalBox* ResetBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ResetConfirmationOverlay = BuildSecondaryPageFrame(ResetBox, TEXT("ResetConfirmationOverlay"),
		ResetPopupOffset, ResetPopupPadding, ResetPopupBackgroundColor, ResetActions);
	AttachPage(ResetConfirmationOverlay);
	UTextBlock* ResetTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ResetTitle->SetText(MainMenuCopy::ResetTitle);
	ResetTitle->SetJustification(ETextJustify::Left);
	ResetTitle->SetColorAndOpacity(FSlateColor(ResetPopupTitleColor));
	FSlateFontInfo ResetFont = SecondaryHeadingFont.Size > 0 ? SecondaryHeadingFont : ResetTitle->GetFont();
	if (SecondaryHeadingFont.Size <= 0)
	{
		ResetFont.Size = CollectionDetailNameFontSize;
		ResetFont.TypefaceFontName = TEXT("Bold");
	}
	ResetTitle->SetFont(ResetFont);
	ResetBox->AddChildToVerticalBox(ResetTitle)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	AddSecondaryPageDivider(ResetBox);
	UTextBlock* ResetBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ResetBody->SetText(MainMenuCopy::ResetBody);
	ResetBody->SetAutoWrapText(true);
	ResetBody->SetWrapTextAt(520.0f);
	ResetBody->SetJustification(ETextJustify::Left);
	ResetBody->SetColorAndOpacity(FSlateColor(SecondaryBodyColor));
	FSlateFontInfo ResetBodyFont = SecondaryBodyFont.Size > 0 ? SecondaryBodyFont : ResetBody->GetFont();
	ResetBodyFont.Size = CollectionDescriptionFontSize;
	ResetBody->SetFont(ResetBodyFont);
	ResetBox->AddChildToVerticalBox(ResetBody)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
	SetResetConfirmationVisible(false);
}

UBorder* UMainMenuWidget::BuildSecondaryPageFrame(UWidget* Content, FName PageName, const FVector2D& PageOffset, const FMargin& ContentPadding, const FLinearColor& FallbackColor, UWidget* Footer)
{
	UBorder* PageOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), PageName);
	PageOverlay->SetBrushColor(FLinearColor::Transparent);
	PageOverlay->SetPadding(FMargin(0.0f));
	PageOverlay->SetRenderTranslation(PageOffset);
	PageOverlay->SetClipping(EWidgetClipping::ClipToBounds);
	UScaleBox* PageScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), NAME_None);
	PageScale->SetStretch(EStretch::ScaleToFit);
	PageScale->SetStretchDirection(EStretchDirection::DownOnly);
	USizeBox* PageDesignSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
	PageDesignSize->SetWidthOverride(1030.0f);
	PageDesignSize->SetHeightOverride(780.0f + (Footer ? 24.0f + FMath::Max(1.0f, CollectionBackButtonSize.Y) : 0.0f));
	UOverlay* PageArtLayers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), NAME_None);
	if (Footer)
	{
		// Keep actions outside the artwork and its content padding.
		UVerticalBox* PageStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		PageDesignSize->SetContent(PageStack);
		USizeBox* ArtSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ArtSize->SetHeightOverride(780.0f);
		ArtSize->SetContent(PageArtLayers);
		PageStack->AddChildToVerticalBox(ArtSize);
		UVerticalBoxSlot* FooterSlot = PageStack->AddChildToVerticalBox(Footer);
		FooterSlot->SetHorizontalAlignment(HAlign_Right);
		FooterSlot->SetPadding(FMargin(0.0f, 14.0f, ContentPadding.Right, 0.0f));
	}
	else
	{
		PageDesignSize->SetContent(PageArtLayers);
	}
	UBorder* PageFallback = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), NAME_None);
	PageFallback->SetBrushColor(FallbackColor);
	PageFallback->SetVisibility(CollectionPanelTexture ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	UOverlaySlot* FallbackSlot = PageArtLayers->AddChildToOverlay(PageFallback);
	FallbackSlot->SetHorizontalAlignment(HAlign_Fill); FallbackSlot->SetVerticalAlignment(VAlign_Fill);
	UImage* PageBackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), NAME_None);
	PageBackgroundImage->SetBrushFromTexture(CollectionPanelTexture, false);
	PageBackgroundImage->SetRenderTransformPivot(FVector2D(0.5f));
	PageBackgroundImage->SetRenderScale(CollectionPanelImageScale);
	PageBackgroundImage->SetRenderTranslation(CollectionPanelImageOffset);
	PageBackgroundImage->SetVisibility(CollectionPanelTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	UOverlaySlot* BackgroundSlot = PageArtLayers->AddChildToOverlay(PageBackgroundImage);
	// Preserve the original Collection brush size: saved image scale/offset values
	// were tuned against it. Filling the panel first multiplies that scale again
	// and crops the artwork down to its dark center.
	BackgroundSlot->SetHorizontalAlignment(HAlign_Left);
	BackgroundSlot->SetVerticalAlignment(VAlign_Top);

	UOverlaySlot* ContentSlot = PageArtLayers->AddChildToOverlay(Content);
	ContentSlot->SetPadding(ContentPadding);
	ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Fill);
	auto AddCorner = [this, PageArtLayers](FName Name, EHorizontalAlignment Horizontal, EVerticalAlignment Vertical, float Angle)
	{
		USizeBox* CornerSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CornerSize->SetWidthOverride(FMath::Max(1.0f, CornerBrushSize.X)); CornerSize->SetHeightOverride(FMath::Max(1.0f, CornerBrushSize.Y));
		CornerSize->SetVisibility(CornerBrushTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		UImage* Corner = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Corner->SetBrushFromTexture(CornerBrushTexture, false); Corner->SetRenderTransformPivot(FVector2D(0.5f)); Corner->SetRenderTransformAngle(Angle);
		Corner->SetVisibility(ESlateVisibility::HitTestInvisible); CornerSize->SetContent(Corner);
		UOverlaySlot* CornerSlot = PageArtLayers->AddChildToOverlay(CornerSize);
		CornerSlot->SetHorizontalAlignment(Horizontal); CornerSlot->SetVerticalAlignment(Vertical);
		CornerSlot->SetPadding(FMargin(FMath::Max(0.0f, CornerBrushInset.X), FMath::Max(0.0f, CornerBrushInset.Y)));
	};
	AddCorner(NAME_None, HAlign_Left, VAlign_Top, 0.0f);
	AddCorner(NAME_None, HAlign_Right, VAlign_Top, 90.0f);
	AddCorner(NAME_None, HAlign_Right, VAlign_Bottom, 180.0f);
	AddCorner(NAME_None, HAlign_Left, VAlign_Bottom, 270.0f);
	PageScale->SetContent(PageDesignSize);
	PageOverlay->SetContent(PageScale);
	return PageOverlay;
}

void UMainMenuWidget::AddSecondaryPageDivider(UVerticalBox* Panel)
{
	USizeBox* DividerSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	if (HorizontalBrushSize.X > 0.0f) DividerSize->SetWidthOverride(HorizontalBrushSize.X);
	DividerSize->SetHeightOverride(FMath::Max(1.0f, HorizontalBrushSize.Y));
	DividerSize->SetRenderTranslation(TitleDividerOffset);
	DividerSize->SetVisibility(HorizontalBrushTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	UImage* Divider = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Divider->SetBrushFromTexture(HorizontalBrushTexture, false);
	Divider->SetVisibility(ESlateVisibility::HitTestInvisible);
	DividerSize->SetContent(Divider);
	Panel->AddChildToVerticalBox(DividerSize)->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 18.0f));
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
		Font.Size = Size;
		Text->SetFont(Font);
		return Text;
	};

	Panel->AddChildToVerticalBox(MakeText(TEXT("CollectionSubtitle"), TEXT("DISCOVERED TECHNIQUES & SYNERGIES"), CollectionSubtitleFontSize,
		FLinearColor(0.55f, 0.57f, 0.62f)))->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 6.0f));
	USizeBox* TitleDividerSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	if (HorizontalBrushSize.X > 0.0f) TitleDividerSize->SetWidthOverride(HorizontalBrushSize.X);
	TitleDividerSize->SetHeightOverride(FMath::Max(1.0f, HorizontalBrushSize.Y));
	TitleDividerSize->SetRenderTranslation(TitleDividerOffset);
	TitleDividerSize->SetVisibility(HorizontalBrushTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	UImage* TitleDivider = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Img_TitleDivider"));
	TitleDivider->SetBrushFromTexture(HorizontalBrushTexture, false); TitleDivider->SetVisibility(ESlateVisibility::HitTestInvisible);
	TitleDividerSize->SetContent(TitleDivider);
	Panel->AddChildToVerticalBox(TitleDividerSize)->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 10.0f));

	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CollectionTabs"));
	Panel->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	auto AddTab = [this, Tabs, MakeText](FName Name, const FString& Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style = Button->GetStyle();
		FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.SetNormal(Empty); Style.SetHovered(Empty); Style.SetPressed(Empty);
		Button->SetStyle(Style);
		Button->AddChild(MakeText(NAME_None, Label, CollectionTabFontSize, SecondaryBodyColor));
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
	USizeBox* CategoryDividerSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	if (CategoryGridDividerSize.X > 0.0f) CategoryDividerSizeBox->SetWidthOverride(CategoryGridDividerSize.X);
	CategoryDividerSizeBox->SetHeightOverride(FMath::Max(1.0f, CategoryGridDividerSize.Y));
	CategoryDividerSizeBox->SetRenderTranslation(CategoryGridDividerOffset);
	CategoryDividerSizeBox->SetVisibility(CategoryGridDividerTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	UImage* CategoryDivider = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Img_CategoryGridDivider"));
	CategoryDivider->SetBrushFromTexture(CategoryGridDividerTexture, false);
	CategoryDivider->SetVisibility(ESlateVisibility::HitTestInvisible); CategoryDividerSizeBox->SetContent(CategoryDivider);
	Panel->AddChildToVerticalBox(CategoryDividerSizeBox)->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 10.0f));

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CollectionBody"));
	Panel->AddChildToVerticalBox(Body);
	UVerticalBox* Library = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CollectionLibrary"));
	UHorizontalBoxSlot* LibrarySlot = Body->AddChildToHorizontalBox(Library);
	LibrarySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LibrarySlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ScrollSize->SetWidthOverride(570.0f); ScrollSize->SetHeightOverride(510.0f);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CollectionScroll"));
	Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	ScrollSize->SetContent(Scroll);
	SynergyCollectionGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("CollectionGrid"));
	const float CollectionTileExtent = 134.0f * FMath::Clamp(CollectionTileScale, 0.25f, 3.0f);
	SynergyCollectionGrid->SetMinDesiredSlotWidth(CollectionTileExtent + FMath::Max(0.0f, CollectionTileSpacing));
	SynergyCollectionGrid->SetMinDesiredSlotHeight(CollectionTileExtent + FMath::Max(0.0f, CollectionTileSpacing));
	Scroll->AddChild(SynergyCollectionGrid);
	Library->AddChildToVerticalBox(ScrollSize);
	UVerticalBox* CollectionFooter = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CollectionUnlockFooter"));
	CollectionFooter->SetRenderTranslation(CollectionUnlockFooterOffset);
	Library->AddChildToVerticalBox(CollectionFooter);
	CollectionDiscoveryCountText = MakeText(TEXT("CollectionDiscoveryCount"), TEXT(""), CollectionFooterFontSize, FLinearColor(0.62f, 0.65f, 0.70f));
	FSlateFontInfo FooterFont = CollectionFooterFont.Size > 0 ? CollectionFooterFont
		: (SecondaryBodyFont.Size > 0 ? SecondaryBodyFont : CollectionDiscoveryCountText->GetFont());
	FooterFont.Size = CollectionFooterFontSize;
	CollectionDiscoveryCountText->SetFont(FooterFont);
	CollectionFooter->AddChildToVerticalBox(CollectionDiscoveryCountText)->SetPadding(FMargin(4.0f, 10.0f, 0.0f, 0.0f));
	CollectionProgressText = MakeText(TEXT("CollectionDiscoveryProgress"), TEXT(""), CollectionFooterFontSize, FLinearColor(0.66f, 0.48f, 0.82f));
	CollectionProgressText->SetFont(FooterFont);
	CollectionFooter->AddChildToVerticalBox(CollectionProgressText)->SetPadding(FMargin(4.0f, 3.0f, 0.0f, 0.0f));

	USizeBox* VerticalDividerSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	VerticalDividerSize->SetWidthOverride(FMath::Max(1.0f, VerticalDividerImageSize.X));
	VerticalDividerSize->SetHeightOverride(FMath::Max(1.0f, VerticalDividerImageSize.Y));
	VerticalDividerSize->SetRenderTranslation(VerticalDividerOffset);
	UImage* VerticalDivider = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Img_VerticalDivider"));
	VerticalDivider->SetBrushFromTexture(VerticalDividerTexture, false);
	VerticalDivider->SetVisibility(VerticalDividerTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	VerticalDividerSize->SetContent(VerticalDivider);
	Body->AddChildToHorizontalBox(VerticalDividerSize)->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));

	USizeBox* Details = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CollectionDetailsPanel"));
	Details->SetWidthOverride(FMath::Max(1.0f, CollectionDetailsPanelSize.X));
	Details->SetHeightOverride(FMath::Max(1.0f, CollectionDetailsPanelSize.Y));
	Details->SetRenderTranslation(CollectionDetailsPanelOffset);
	UHorizontalBoxSlot* DetailsSlot = Body->AddChildToHorizontalBox(Details);
	DetailsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	UOverlay* DetailsLayers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CollectionDetailsLayers")); Details->SetContent(DetailsLayers);
	UBorder* DetailsFallback = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DetailsPanelFallback"));
	DetailsFallback->SetBrushColor(FLinearColor(0.025f, 0.029f, 0.038f, 0.98f));
	DetailsFallback->SetVisibility(DetailsPanelTexture ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	DetailsLayers->AddChildToOverlay(DetailsFallback);
	UImage* DetailsBackground = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Img_DetailsPanelBackground"));
	DetailsBackground->SetBrushFromTexture(DetailsPanelTexture, false);
	DetailsBackground->SetRenderTransformPivot(FVector2D(0.5f));
	DetailsBackground->SetRenderScale(DetailsPanelImageScale);
	DetailsBackground->SetRenderTranslation(DetailsPanelImageOffset);
	DetailsBackground->SetVisibility(DetailsPanelTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	DetailsLayers->AddChildToOverlay(DetailsBackground);
	UBorder* DetailsPadding = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	DetailsPadding->SetBrushColor(FLinearColor::Transparent); DetailsPadding->SetPadding(CollectionDetailsContentPadding);
	DetailsLayers->AddChildToOverlay(DetailsPadding);
	UVerticalBox* DetailStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass()); DetailsPadding->SetContent(DetailStack);
	CollectionDetailName = MakeText(TEXT("CollectionDetailName"), TEXT(""), CollectionDetailNameFontSize, SecondaryHeadingColor);
	CollectionDetailName->SetAutoWrapText(false);
	DetailStack->AddChildToVerticalBox(CollectionDetailName)->SetPadding(FMargin(0,0,0,12));
	USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconSize->SetWidthOverride(FMath::Max(1.0f, CollectionDetailsArtworkSize.X));
	IconSize->SetHeightOverride(FMath::Max(1.0f, CollectionDetailsArtworkSize.Y));
	IconSize->SetRenderTranslation(CollectionDetailsArtworkOffset);
	CollectionDetailIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CollectionDetailIcon"));
	UOverlay* DetailIconArea = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CollectionDetailIconArea"));
	IconSize->SetContent(DetailIconArea);
	UOverlaySlot* DetailImageSlot = DetailIconArea->AddChildToOverlay(CollectionDetailIcon);
	DetailImageSlot->SetHorizontalAlignment(HAlign_Left); DetailImageSlot->SetVerticalAlignment(VAlign_Center);
	UVerticalBoxSlot* DetailIconSlot = DetailStack->AddChildToVerticalBox(IconSize); DetailIconSlot->SetHorizontalAlignment(HAlign_Left); DetailIconSlot->SetPadding(FMargin(0,0,0,14));
	CollectionDetailDescription = MakeText(TEXT("CollectionDetailDescription"), TEXT(""), CollectionDescriptionFontSize, SecondaryBodyColor);
	CollectionDetailDescription->SetAutoWrapText(true); CollectionDetailDescription->SetWrapTextAt(330.0f);
	DetailStack->AddChildToVerticalBox(CollectionDetailDescription)->SetPadding(FMargin(0,0,0,16));
	USizeBox* DetailsDividerSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	if (HorizontalBrushSize.X > 0.0f) DetailsDividerSize->SetWidthOverride(HorizontalBrushSize.X);
	DetailsDividerSize->SetHeightOverride(FMath::Max(1.0f, HorizontalBrushSize.Y));
	DetailsDividerSize->SetRenderTranslation(DetailsDividerOffset);
	DetailsDividerSize->SetVisibility(HorizontalBrushTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	UImage* DetailsDivider = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Img_DetailsDivider"));
	DetailsDivider->SetBrushFromTexture(HorizontalBrushTexture, false); DetailsDivider->SetVisibility(ESlateVisibility::HitTestInvisible);
	DetailsDividerSize->SetContent(DetailsDivider); DetailStack->AddChildToVerticalBox(DetailsDividerSize)->SetPadding(FMargin(0,0,0,8));
	CollectionDetailStats = MakeText(TEXT("CollectionDetailStats"), TEXT(""), CollectionStatsFontSize, FLinearColor(0.68f,0.70f,0.76f));
	CollectionDetailStats->SetAutoWrapText(true); DetailStack->AddChildToVerticalBox(CollectionDetailStats);

	UButton* BackButton = AddMenuButton(Panel, FText::FromString(TEXT("BACK")), TEXT("CollectionBackButton"));
	BackButton->SetRenderTranslation(CollectionBackButtonOffset);
	BackButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleBack);
	if (USizeBox* BackSize = Cast<USizeBox>(BackButton->GetChildAt(0)))
	{
		BackSize->SetWidthOverride(FMath::Max(60.0f, CollectionBackButtonSize.X));
		BackSize->SetHeightOverride(FMath::Max(1.0f, CollectionBackButtonSize.Y));
	}
	if (UVerticalBoxSlot* BackSlot = Cast<UVerticalBoxSlot>(BackButton->Slot))
	{
		BackSlot->SetHorizontalAlignment(HAlign_Right);
		BackSlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));
	}
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


	UTextBlock* Heading = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SettingsHeading"));
	Heading->SetText(FText::FromString(TEXT("SETTINGS")));
	Heading->SetJustification(ETextJustify::Left);
	Heading->SetColorAndOpacity(FSlateColor(SecondaryHeadingColor));
	FSlateFontInfo HeadingFont = SecondaryHeadingFont.Size > 0 ? SecondaryHeadingFont : Heading->GetFont();
	if (SecondaryHeadingFont.Size <= 0)
	{
		HeadingFont.Size = CollectionDetailNameFontSize;
		HeadingFont.TypefaceFontName = TEXT("Bold");
	}
	Heading->SetFont(HeadingFont);
	Panel->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 28.0f));

	AddSecondaryPageDivider(Panel);
	UHorizontalBox* SettingRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("AutoTargetingRow"));
	Panel->AddChildToVerticalBox(SettingRow)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 28.0f));

	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AutoTargetingLabel"));
	Label->SetText(FText::FromString(TEXT("Auto Targeting")));
	Label->SetColorAndOpacity(FSlateColor(SecondaryBodyColor));
	FSlateFontInfo LabelFont = SecondaryBodyFont.Size > 0 ? SecondaryBodyFont : Label->GetFont();
	LabelFont.Size = CollectionDescriptionFontSize;
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

	UHorizontalBox* ShakeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CameraShakeRow"));
	Panel->AddChildToVerticalBox(ShakeRow)->SetPadding(FMargin(0, 6, 0, 28));
	UTextBlock* ShakeLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ShakeLabel->SetText(FText::FromString(TEXT("Camera Shake Intensity")));
	ShakeLabel->SetFont(LabelFont);
	ShakeLabel->SetColorAndOpacity(FSlateColor(SecondaryBodyColor));
	ShakeRow->AddChildToHorizontalBox(ShakeLabel)->SetPadding(FMargin(0, 0, 28, 0));
	CameraShakeSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("CameraShakeSlider"));
	CameraShakeSlider->SetMinValue(0.0f);
	CameraShakeSlider->SetMaxValue(1.0f);
	CameraShakeSlider->SetStepSize(0.01f);
	CameraShakeSlider->SetToolTipText(FText::FromString(TEXT("Camera shake strength. 0% disables shake without changing other feedback.")));
	UHorizontalBoxSlot* SliderSlot = ShakeRow->AddChildToHorizontalBox(CameraShakeSlider);
	SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SliderSlot->SetVerticalAlignment(VAlign_Center);
	CameraShakeValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CameraShakeValueText->SetFont(LabelFont);
	CameraShakeValueText->SetColorAndOpacity(FSlateColor(SecondaryHeadingColor));
	ShakeRow->AddChildToHorizontalBox(CameraShakeValueText)->SetPadding(FMargin(12, 0, 0, 0));
	CameraShakeSlider->OnValueChanged.AddDynamic(this, &UMainMenuWidget::HandleCameraShakeChanged);
	RefreshAutoTargetingSetting();
	return Panel;
}

void UMainMenuWidget::RefreshAutoTargetingSetting()
{
	const UHeavensDivideGameUserSettings* Settings = UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
	const bool bEnabled = !Settings || Settings->IsAutoTargetingEnabled();
	const float ShakeIntensity = Settings ? Settings->GetCameraShakeIntensity() : 1.0f;
	if (CameraShakeSlider) CameraShakeSlider->SetValue(ShakeIntensity);
	if (CameraShakeValueText) CameraShakeValueText->SetText(FText::AsPercent(ShakeIntensity));
	if (AutoTargetingCheckBox) AutoTargetingCheckBox->SetIsChecked(bEnabled);
	if (AutoTargetingStateText) AutoTargetingStateText->SetText(FText::FromString(bEnabled ? TEXT("ON") : TEXT("OFF")));
}

void UMainMenuWidget::HandleCameraShakeChanged(float Value)
{
	if (UHeavensDivideGameUserSettings* Settings = UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings())
		Settings->SetCameraShakeIntensity(Value);
	if (CameraShakeValueText) CameraShakeValueText->SetText(FText::AsPercent(Value));
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
	CollectionTileButtons.Reset(); CollectionTileSelectionBorders.Reset(); CollectionTileSelectionImages.Reset(); CollectionTileIcons.Reset();
	CollectionDefinitions = Meta->GetCollectionUpgradeDefinitions(SelectedCollectionCategory);
	if (!CollectionDefinitions.Contains(SelectedCollectionUpgrade)) SelectedCollectionUpgrade = CollectionDefinitions.Num() ? CollectionDefinitions[0] : nullptr;
	int32 UnlockedCount = 0;
	const float TileScale = FMath::Clamp(CollectionTileScale, 0.25f, 3.0f);
	const float TileDisplayExtent = 134.0f * TileScale;
	const float TileSpacing = FMath::Max(0.0f, CollectionTileSpacing);
	const float ContentExtent = TileDisplayExtent - 6.0f * TileScale;
	const float IconExtent = FMath::Max(1.0f, ContentExtent - 2.0f * FMath::Max(0.0f, CollectionTileIconPadding) * TileScale);
	// The library is 570 px wide; reserve 20 px for the scrollbar.
	const int32 ColumnCount = FMath::Max(1, FMath::FloorToInt(550.0f / (TileDisplayExtent + TileSpacing)));
	SynergyCollectionGrid->SetMinDesiredSlotWidth(TileDisplayExtent + TileSpacing);
	SynergyCollectionGrid->SetMinDesiredSlotHeight(TileDisplayExtent + TileSpacing);
	// Size the artwork directly, so fitting containers cannot cancel the requested scale.
	const auto AddFillLayer = [](UOverlay* Parent, UWidget* Child)
	{
		UOverlaySlot* LayerSlot = Parent->AddChildToOverlay(Child);
		LayerSlot->SetHorizontalAlignment(HAlign_Fill);
		LayerSlot->SetVerticalAlignment(VAlign_Fill);
		return LayerSlot;
	};
	for (int32 Index = 0; Index < CollectionDefinitions.Num(); ++Index)
	{
		UUpgradeDefinition* Definition = CollectionDefinitions[Index];
		const bool bUnlocked = Meta->IsCollectionUpgradeUnlocked(Definition);
		UnlockedCount += bUnlocked ? 1 : 0;
		UCollectionUpgradeTileButton* Tile = WidgetTree->ConstructWidget<UCollectionUpgradeTileButton>(UCollectionUpgradeTileButton::StaticClass());
		Tile->InitializeCollectionTile(this, Definition);
		FButtonStyle Style = Tile->GetStyle(); FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.SetNormal(Empty); Style.SetHovered(Empty); Style.SetPressed(Empty);
		Style.SetNormalPadding(FMargin(0.0f)); Style.SetPressedPadding(FMargin(0.0f)); Tile->SetStyle(Style);
		USizeBox* TileSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		TileSize->SetWidthOverride(TileDisplayExtent); TileSize->SetHeightOverride(TileDisplayExtent); Tile->AddChild(TileSize);
		UOverlay* TileLayers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass()); TileSize->SetContent(TileLayers);
		UBorder* Selection = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Selection->SetBrushColor(FLinearColor(0.13f,0.14f,0.17f,1.0f)); Selection->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddFillLayer(TileLayers, Selection);
		UOverlay* ContentLayers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UOverlaySlot* ContentLayerSlot = AddFillLayer(TileLayers, ContentLayers); ContentLayerSlot->SetPadding(FMargin(3.0f * TileScale));
		UBorder* BackgroundFallback = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		BackgroundFallback->SetBrushColor(bUnlocked ? CollectionUnlockedCardColor : CollectionLockedCardColor);
		BackgroundFallback->SetVisibility(TileBackgroundTexture ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		AddFillLayer(ContentLayers, BackgroundFallback);
		UImage* TileBackground = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			*FString::Printf(TEXT("Img_TileBackground_%d"), Index));
		TileBackground->SetBrushFromTexture(TileBackgroundTexture, false);
		TileBackground->SetDesiredSizeOverride(FVector2D(ContentExtent));
		TileBackground->SetVisibility(TileBackgroundTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		AddFillLayer(ContentLayers, TileBackground);
		UTexture2D* DisplayTexture = Definition ? (Definition->Icon ? Definition->Icon : Definition->CardArtwork) : nullptr;
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Icon->SetBrushFromTexture(DisplayTexture, true);
		Icon->SetColorAndOpacity(bUnlocked ? FLinearColor::White : FLinearColor(0.28f, 0.24f, 0.24f, 0.55f));
		Icon->SetVisibility(DisplayTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		FVector2D IconSize(IconExtent);
		if (DisplayTexture)
		{
			const FVector2D SourceSize(FMath::Max(1, DisplayTexture->GetSizeX()), FMath::Max(1, DisplayTexture->GetSizeY()));
			IconSize = SourceSize * (IconExtent / FMath::Max(SourceSize.X, SourceSize.Y));
		}
		Icon->SetDesiredSizeOverride(IconSize);
		UOverlaySlot* IconSlot = ContentLayers->AddChildToOverlay(Icon);
		IconSlot->SetHorizontalAlignment(HAlign_Center); IconSlot->SetVerticalAlignment(VAlign_Center);
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
			UImage* LockedOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
				*FString::Printf(TEXT("Img_LockedOverlay_%d"), Index));
			LockedOverlay->SetBrushFromTexture(TileBackgroundTexture, false);
			LockedOverlay->SetDesiredSizeOverride(FVector2D(ContentExtent));
			LockedOverlay->SetColorAndOpacity(FLinearColor(0.18f, 0.025f, 0.025f, 0.46f));
			LockedOverlay->SetVisibility(TileBackgroundTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			AddFillLayer(ContentLayers, LockedOverlay);
			UTextBlock* Lock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); Lock->SetText(FText::FromString(TEXT("LOCKED")));
			Lock->SetJustification(ETextJustify::Center); Lock->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f,0.63f,0.66f)));
			FSlateFontInfo Font = Lock->GetFont(); Font.Size = FMath::Max(1, FMath::RoundToInt(11.0f * TileScale)); Lock->SetFont(Font);
			UOverlaySlot* LockSlot = ContentLayers->AddChildToOverlay(Lock); LockSlot->SetHorizontalAlignment(HAlign_Center); LockSlot->SetVerticalAlignment(VAlign_Bottom);
		}
		UImage* SelectedFrame = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			*FString::Printf(TEXT("Img_TileSelectedFrame_%d"), Index));
		SelectedFrame->SetBrushFromTexture(TileSelectedTexture, false);
		SelectedFrame->SetDesiredSizeOverride(FVector2D(TileDisplayExtent));
		SelectedFrame->SetVisibility(ESlateVisibility::Hidden);
		AddFillLayer(TileLayers, SelectedFrame);
		UUniformGridSlot* GridSlot = SynergyCollectionGrid->AddChildToUniformGrid(Tile, Index / ColumnCount, Index % ColumnCount);
		GridSlot->SetHorizontalAlignment(HAlign_Center); GridSlot->SetVerticalAlignment(VAlign_Center);
		CollectionTileButtons.Add(Tile); CollectionTileSelectionBorders.Add(Selection); CollectionTileSelectionImages.Add(SelectedFrame); CollectionTileIcons.Add(Icon);
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
	if (DisplayTexture)
	{
		const FVector2D SourceSize(FMath::Max(1, DisplayTexture->GetSizeX()), FMath::Max(1, DisplayTexture->GetSizeY()));
		const float FitScale = FMath::Min(FMath::Max(1.0f, CollectionDetailsArtworkSize.X) / SourceSize.X,
			FMath::Max(1.0f, CollectionDetailsArtworkSize.Y) / SourceSize.Y);
		CollectionDetailIcon->SetDesiredSizeOverride(SourceSize * FitScale);
	}
	CollectionDetailIcon->SetColorAndOpacity(bUnlocked ? FLinearColor::White : FLinearColor(0.24f, 0.21f, 0.21f, 0.48f));
	CollectionDetailIcon->SetVisibility(DisplayTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	CollectionDetailName->SetText(bUnlocked ? SelectedCollectionUpgrade->DisplayName : FText::FromString(TEXT("UNKNOWN TECHNIQUE")));
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
		const bool bHot = bSelected || (CollectionTileButtons[Index] && (CollectionTileButtons[Index]->IsHovered()
			|| (bShowFocusHighlight && CollectionTileButtons[Index]->HasAnyUserFocus())));
		if (CollectionTileSelectionBorders.IsValidIndex(Index) && CollectionTileSelectionBorders[Index])
			CollectionTileSelectionBorders[Index]->SetBrushColor(TileSelectedTexture ? FLinearColor::Transparent
				: (bHot ? Accent : FLinearColor(0.13f,0.14f,0.17f,1.0f)));
		if (CollectionTileSelectionImages.IsValidIndex(Index) && CollectionTileSelectionImages[Index])
		{
			CollectionTileSelectionImages[Index]->SetColorAndOpacity(Accent);
			CollectionTileSelectionImages[Index]->SetVisibility(TileSelectedTexture && bHot
				? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
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
