// Copyright Epic Games, Inc. All Rights Reserved.

#include "MainMenuWidget.h"
#include "BloodshiftMenuStyle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/Spacer.h"
#include "TesterBalanceWidget.h"
#include "MenuInkStyle.h"
#include "MetaSkillTreeWidget.h"

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
#include "Components/NativeWidgetHost.h"
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
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "Rendering/DrawElementTypes.h"
#include "Widgets/SLeafWidget.h"

namespace MainMenuReadability
{
	const FLinearColor Label(0.96f, 0.89f, 0.74f);
	const FLinearColor Ink(0.008f, 0.012f, 0.019f);
	const FLinearColor Gold(0.68f, 0.47f, 0.22f);

	// A native gradient stays smooth at every resolution and adds no texture asset.
	class SLeftMenuShade final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SLeftMenuShade) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs) {}
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
			FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool) const override
		{
			const FVector2D Size = Geometry.GetLocalSize();
			TArray<FSlateGradientStop> Stops;
			const float Opacity = 0.70f * Style.GetColorAndOpacityTint().A;
			Stops.Emplace(FVector2D::ZeroVector, FLinearColor(0.006f, 0.014f, 0.023f, Opacity));
			// Hold contrast across the labels, then gently clear before the central tree.
			for (int32 Index = 0; Index <= 16; ++Index)
			{
				const float T = Index / 16.0f;
				const float Fade = 1.0f - T * T * (3.0f - 2.0f * T);
				Stops.Emplace(FVector2D(Size.X * FMath::Lerp(0.18f, 0.40f, T), 0.0f),
					FLinearColor(0.006f, 0.014f, 0.023f, Opacity * Fade));
			}
			Stops.Emplace(FVector2D(Size.X, 0.0f), FLinearColor(0.006f, 0.014f, 0.023f, 0.0f));
			// Slate names the orientation of the stop lines: vertical lines fade left to right.
			FSlateDrawElement::MakeGradient(Elements, Layer, Geometry.ToPaintGeometry(), MoveTemp(Stops), Orient_Vertical);
			return Layer;
		}
	};
}

namespace MainMenuCopy
{
	static const FText Title = FText::FromString(TEXT("BLOODSHIFT"));
	static const FText ResetTitle = FText::FromString(TEXT("ERASE YOUR LEGACY?"));
	static const FText ResetBody = FText::FromString(TEXT("This permanently removes all learned skills, Soul Embers, unlocked synergies, and Twin Soul discovery progress.\n\nThis cannot be undone."));
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

UMainMenuWidget::UMainMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> Panel(TEXT("/Game/HeavensDivide/Blueprints/UI/SkillTree/AscensionPanel.AscensionPanel"));
	CollectionPanelTexture = Panel.Object;
	static ConstructorHelpers::FObjectFinder<UObject> Heading(TEXT("/Game/Assets/Fonts/Cinzel-Medium_Font.Cinzel-Medium_Font"));
	SecondaryHeadingFont = FSlateFontInfo(Heading.Object, 32, TEXT("Default"));
	SecondaryBodyFont = BloodshiftMenu::BodyFont(20);
	SecondaryHeadingColor = SecondaryBodyColor = BloodshiftMenu::Paper;
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
	if (bInRunSettings) ShowSettingsPanel();
	else { StartBackgroundMedia(); ShowMainPanel(); }
#if !UE_BUILD_SHIPPING
	// Read-only screen previews for visual QA; these never activate menu actions.
	FString PreviewPage;
    if (!bInRunSettings && GetWorld() && FParse::Value(FCommandLine::Get(), TEXT("MenuPreview="), PreviewPage))
    {
        // The game mode selects the main page immediately after AddToViewport.
        GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, PreviewPage]()
        {
            if (PreviewPage == TEXT("Collection")) ShowCollectionPanel();
            else if (PreviewPage == TEXT("SkillTree")) ShowSkillTree();
            else if (PreviewPage == TEXT("Settings")) ShowSettingsPanel();
            else if (PreviewPage == TEXT("Keybinds")) { ShowSettingsPanel(); HandleKeybindSettings(); }
            else if (PreviewPage == TEXT("Reset")) ShowResetConfirmation();
        }));
    }
#endif
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
	UNativeWidgetHost* MenuShade = WidgetTree->ConstructWidget<UNativeWidgetHost>(UNativeWidgetHost::StaticClass(), TEXT("MainMenuLeftShade"));
	MenuShade->SetContent(SNew(MainMenuReadability::SLeftMenuShade));
	MenuShade->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* ShadeSlot = MainPage->AddChildToCanvas(MenuShade);
	ShadeSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	ShadeSlot->SetOffsets(FMargin(0.0f));
	ShadeSlot->SetZOrder(-1);
	TesterBalancePage = CreateWidget<UTesterBalanceWidget>(this);
	MenuSwitcher->AddChild(TesterBalancePage);
	auto* TesterStack = WidgetTree->ConstructWidget<UVerticalBox>();
	auto* TesterSlot = MainPage->AddChildToCanvas(TesterStack);
	TesterSlot->SetAnchors(FAnchors(1, 1));
	TesterSlot->SetAlignment(FVector2D(1, 1));
	TesterSlot->SetPosition(FVector2D(-20, -12));
	TesterSlot->SetAutoSize(true);
	TesterStack->SetRenderTransformPivot(FVector2D(1, 1));
	TesterStack->SetRenderScale(FVector2D(.65f));
	AddMenuButton(TesterStack, FText::FromString(TEXT("TESTER BALANCE")), TEXT("TesterBalanceButton"))->OnClicked.AddDynamic(this, &UMainMenuWidget::ShowTesterBalance);
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
	LogoPresentation = LogoSize;
	LogoSize->SetRenderTransformPivot(FVector2D::ZeroVector);
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
	UButton* SkillButton = AddMenuButton(MainButtonStack, FText::FromString(TEXT("SKILL TREE")), TEXT("SkillTreeButton"));
	SkillButton->OnClicked.AddDynamic(this, &UMainMenuWidget::ShowSkillTree);
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
		PageSlot->SetPadding(FMargin(440.0f, 40.0f, 36.0f, 64.0f));
	};

	// Share the collection page's reserved navigation column and responsive bounds.
	SkillTreeOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SkillTreeOverlay"));
	SkillTreeOverlay->SetBrushColor(FLinearColor::Transparent);
	SkillTreeOverlay->SetPadding(FMargin(0));
	SkillTreeOverlay->SetClipping(EWidgetClipping::ClipToBounds);
	AttachPage(SkillTreeOverlay);
	SetSkillTreeVisible(false);

	UHorizontalBox* CollectionActions = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddPageAction(CollectionActions, FText::FromString(TEXT("BACK")), TEXT("CollectionBackButton"))
		->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleBack);
	CollectionOverlay = BuildSecondaryPageFrame(BuildCollectionPanel(), TEXT("CollectionOverlay"),
		CollectionPageOffset, CollectionContentPadding, FLinearColor(0.008f, 0.010f, 0.016f, 0.94f), CollectionActions,
		FVector2D(1320.0f, 900.0f), true);
	AttachPage(CollectionOverlay);
	SetCollectionVisible(false);
	UHorizontalBox* SettingsActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UButton* SettingsBack = AddPageAction(SettingsActions, FText::FromString(TEXT("BACK")), TEXT("SettingsBackButton"));
	SettingsBack->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleBack);
	UHorizontalBox* ResetActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UButton* CancelButton = AddPageAction(ResetActions, FText::FromString(TEXT("CANCEL")), TEXT("CancelResetButton"));
	UButton* ConfirmButton = AddPageAction(ResetActions, FText::FromString(TEXT("ERASE PROGRESS")), TEXT("ConfirmResetButton"), true);
	CancelButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleCancelReset);
	ConfirmButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleConfirmReset);

	SettingsPopupOverlay = BuildSecondaryPageFrame(BuildSettingsPanel(), TEXT("SettingsPopupOverlay"),
		SettingsPageOffset, SettingsPopupPadding, SettingsPopupBackgroundColor, SettingsActions, FVector2D(1000.0f, 660.0f));
	AttachPage(SettingsPopupOverlay);
	SetSettingsPopupVisible(false);

	UVerticalBox* ResetBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ResetConfirmationOverlay = BuildSecondaryPageFrame(ResetBox, TEXT("ResetConfirmationOverlay"),
		ResetPopupOffset, ResetPopupPadding, ResetPopupBackgroundColor, ResetActions, FVector2D(860.0f, 460.0f));
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
	ResetBody->SetWrapTextAt(740.0f);
	ResetBody->SetJustification(ETextJustify::Left);
	ResetBody->SetColorAndOpacity(FSlateColor(SecondaryBodyColor));
	FSlateFontInfo ResetBodyFont = SecondaryBodyFont.Size > 0 ? SecondaryBodyFont : ResetBody->GetFont();
	ResetBodyFont.Size = CollectionDescriptionFontSize;
	ResetBody->SetFont(ResetBodyFont);
	ResetBox->AddChildToVerticalBox(ResetBody)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
	SetResetConfirmationVisible(false);
}

UButton* UMainMenuWidget::AddPageAction(UHorizontalBox* Parent, const FText& Label, FName Name, bool bDanger)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    Button->SetStyle(BloodshiftMenu::ActionStyle(bDanger));
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
    Text->SetText(Label);
    Text->SetFont(BloodshiftMenu::BodyFont(18));
    Text->SetColorAndOpacity(FSlateColor::UseForeground());
    Button->SetContent(Text);
    Parent->AddChildToHorizontalBox(Button)->SetPadding(FMargin(12, 0, 0, 0));
    return Button;
}

UBorder* UMainMenuWidget::BuildSecondaryPageFrame(UWidget* Content, FName PageName, const FVector2D& PageOffset, const FMargin& ContentPadding, const FLinearColor& FallbackColor, UWidget* Footer, FVector2D PageSize, bool bAllowUpscaling)
{
    UBorder* Page = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), PageName);
    Page->SetBrushColor(FLinearColor::Transparent);
    Page->SetPadding(FMargin(0));
    Page->SetClipping(EWidgetClipping::ClipToBounds);
    UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    Scale->SetStretchDirection(bAllowUpscaling ? EStretchDirection::Both : EStretchDirection::DownOnly);
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(PageSize.X); Size->SetHeightOverride(PageSize.Y);
    UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
    UNativeWidgetHost* Surface = WidgetTree->ConstructWidget<UNativeWidgetHost>();
    Surface->SetContent(SNew(BloodshiftMenu::SPanelSurface).Danger(PageName == TEXT("ResetConfirmationOverlay")));
    Surface->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* SurfaceSlot = Layers->AddChildToOverlay(Surface);
    SurfaceSlot->SetHorizontalAlignment(HAlign_Fill); SurfaceSlot->SetVerticalAlignment(VAlign_Fill);
    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>();
    auto* StackSlot = Layers->AddChildToOverlay(Stack);
    StackSlot->SetPadding(ContentPadding);
    StackSlot->SetHorizontalAlignment(HAlign_Fill); StackSlot->SetVerticalAlignment(VAlign_Fill);
    auto* ContentSlot = Stack->AddChildToVerticalBox(Content);
    ContentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    if (Footer)
    {
        AddSecondaryPageDivider(Stack);
        auto* FooterSlot = Stack->AddChildToVerticalBox(Footer);
        FooterSlot->SetHorizontalAlignment(HAlign_Right);
    }
    Size->SetContent(Layers); Scale->SetContent(Size); Page->SetContent(Scale);
    return Page;
}

void UMainMenuWidget::AddSecondaryPageDivider(UVerticalBox* Panel)
{
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetHeightOverride(1.0f);
    UNativeWidgetHost* Line = WidgetTree->ConstructWidget<UNativeWidgetHost>();
    Line->SetContent(SNew(BloodshiftMenu::SOrnamentDivider));
    Line->SetVisibility(ESlateVisibility::HitTestInvisible);
    Size->SetContent(Line);
    Panel->AddChildToVerticalBox(Size)->SetPadding(FMargin(0, 12, 0, 16));
}

UVerticalBox* UMainMenuWidget::BuildCollectionPanel()
{
    UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CollectionPanel"));
    auto MakeText = [this](FName Name, const FString& Value, int32 Size, FLinearColor Color, bool bHeading = false)
    {
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        Text->SetText(FText::FromString(Value)); Text->SetColorAndOpacity(Color);
        FSlateFontInfo Font = bHeading && SecondaryHeadingFont.FontObject ? SecondaryHeadingFont : BloodshiftMenu::BodyFont(Size);
        Font.Size = Size; Text->SetFont(Font);
        return Text;
    };
    Panel->AddChildToVerticalBox(MakeText(TEXT("CollectionHeading"), TEXT("THE COLLECTION"), 34, BloodshiftMenu::Paper, true));
    Panel->AddChildToVerticalBox(MakeText(TEXT("CollectionSubtitle"), TEXT("Techniques discovered. Synergies awakened."), 18, BloodshiftMenu::Muted))
        ->SetPadding(FMargin(0, 8, 0, 2));
    AddSecondaryPageDivider(Panel);
    UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CollectionTabs"));
    Panel->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0, 0, 0, 16));
    auto AddTab = [this, Tabs, MakeText](FName Name, const FString& Label)
    {
        UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
        Button->SetStyle(BloodshiftMenu::ActionStyle());
        UTextBlock* Text = MakeText(NAME_None, Label, 17, BloodshiftMenu::Paper);
        Text->SetColorAndOpacity(FSlateColor::UseForeground()); Button->SetContent(Text);
        Tabs->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0, 0, 12, 0));
        return Button;
    };
    SamuraiCollectionTab = AddTab(TEXT("SamuraiCollectionTab"), TEXT("SAMURAI"));
    NinjaCollectionTab = AddTab(TEXT("NinjaCollectionTab"), TEXT("NINJA"));
    SynergyCollectionTab = AddTab(TEXT("SynergyCollectionTab"), TEXT("SYNERGIES"));
    SamuraiCollectionTab->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSamuraiCollectionTab);
    NinjaCollectionTab->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleNinjaCollectionTab);
    SynergyCollectionTab->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSynergyCollectionTab);

    UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CollectionBody"));
    Panel->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UVerticalBox* Library = WidgetTree->ConstructWidget<UVerticalBox>();
    Body->AddChildToHorizontalBox(Library)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>();
    ScrollSize->SetWidthOverride(704);
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CollectionScroll"));
    Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
    Scroll->SetScrollbarThickness(FVector2D(5, 5));
    ScrollSize->SetContent(Scroll);
    Library->AddChildToVerticalBox(ScrollSize)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    SynergyCollectionGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("CollectionGrid"));
    Scroll->AddChild(SynergyCollectionGrid);
    CollectionDiscoveryCountText = MakeText(TEXT("CollectionDiscoveryCount"), TEXT(""), 18, BloodshiftMenu::Paper);
    Library->AddChildToVerticalBox(CollectionDiscoveryCountText)->SetPadding(FMargin(0, 20, 0, 0));
    CollectionProgressText = MakeText(TEXT("CollectionDiscoveryProgress"), TEXT(""), 16, BloodshiftMenu::Gold);
    Library->AddChildToVerticalBox(CollectionProgressText)->SetPadding(FMargin(0, 6, 0, 0));

    USizeBox* Details = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CollectionDetailsPanel"));
    Details->SetWidthOverride(380);
    Body->AddChildToHorizontalBox(Details)->SetPadding(FMargin(28, 0, 0, 0));
    UOverlay* CardLayers = WidgetTree->ConstructWidget<UOverlay>(); Details->SetContent(CardLayers);
    UNativeWidgetHost* CardFrame = WidgetTree->ConstructWidget<UNativeWidgetHost>();
    CardFrame->SetContent(SNew(BloodshiftMenu::SInsetSurface)); CardFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* CardFrameSlot = CardLayers->AddChildToOverlay(CardFrame);
    CardFrameSlot->SetHorizontalAlignment(HAlign_Fill); CardFrameSlot->SetVerticalAlignment(VAlign_Fill);
    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrushColor(FLinearColor::Transparent); Card->SetPadding(FMargin(26));
    auto* CardSlot = CardLayers->AddChildToOverlay(Card);
    CardSlot->SetHorizontalAlignment(HAlign_Fill); CardSlot->SetVerticalAlignment(VAlign_Fill);
    UScrollBox* DetailScroll = WidgetTree->ConstructWidget<UScrollBox>();
    DetailScroll->SetScrollbarThickness(FVector2D(4, 4)); Card->SetContent(DetailScroll);
    UVerticalBox* DetailStack = WidgetTree->ConstructWidget<UVerticalBox>(); DetailScroll->AddChild(DetailStack);
    DetailStack->AddChildToVerticalBox(MakeText(NAME_None, TEXT("TECHNIQUE DETAILS"), 13, BloodshiftMenu::Muted))
        ->SetPadding(FMargin(0, 0, 0, 16));
    CollectionDetailName = MakeText(TEXT("CollectionDetailName"), TEXT(""), 26, BloodshiftMenu::Paper, true);
    CollectionDetailName->SetAutoWrapText(true); CollectionDetailName->SetWrapTextAt(310);
    DetailStack->AddChildToVerticalBox(CollectionDetailName)->SetPadding(FMargin(0, 0, 0, 22));
    USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
    IconSize->SetWidthOverride(CollectionDetailsArtworkSize.X); IconSize->SetHeightOverride(CollectionDetailsArtworkSize.Y);
    CollectionDetailIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CollectionDetailIcon"));
    IconSize->SetContent(CollectionDetailIcon);
    auto* IconSlot = DetailStack->AddChildToVerticalBox(IconSize);
    IconSlot->SetHorizontalAlignment(HAlign_Center); IconSlot->SetPadding(FMargin(0, 0, 0, 24));
    CollectionDetailDescription = MakeText(TEXT("CollectionDetailDescription"), TEXT(""), 20, BloodshiftMenu::Paper);
    CollectionDetailDescription->SetAutoWrapText(true); CollectionDetailDescription->SetWrapTextAt(310);
    DetailStack->AddChildToVerticalBox(CollectionDetailDescription);
    AddSecondaryPageDivider(DetailStack);
    CollectionDetailStats = MakeText(TEXT("CollectionDetailStats"), TEXT(""), 16, BloodshiftMenu::Muted);
    CollectionDetailStats->SetAutoWrapText(true); DetailStack->AddChildToVerticalBox(CollectionDetailStats);
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

void UMenuKeybindSelector::InitializeBinding(UMainMenuWidget* Owner,FName Action)
{
 MenuOwner=Owner;BindingAction=Action;
 SetAllowModifierKeys(false);SetAllowGamepadKeys(false);SetEscapeKeys({EKeys::Escape,EKeys::Gamepad_FaceButton_Right});
 SetKeySelectionText(FText::FromString(TEXT("Press a key...")));
 OnKeySelected.AddDynamic(this,&UMenuKeybindSelector::HandleBindingSelected);
}
void UMenuKeybindSelector::HandleBindingSelected(FInputChord Key)
{
 if(MenuOwner)MenuOwner->ApplyKeyBinding(BindingAction,Key);
}
UVerticalBox* UMainMenuWidget::BuildSettingsPanel()
{
 auto* Host=WidgetTree->ConstructWidget<UVerticalBox>();
 SettingsSwitcher=WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(),TEXT("SettingsSubpages"));
 Host->AddChildToVerticalBox(SettingsSwitcher)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 SettingsSwitcher->AddChild(BuildGeneralSettingsPanel());SettingsSwitcher->AddChild(BuildKeybindPanel());
 return Host;
}
UVerticalBox* UMainMenuWidget::BuildKeybindPanel()
{
 auto* Panel=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("KeybindPanel"));
 auto* Heading=WidgetTree->ConstructWidget<UTextBlock>();
 Heading->SetText(FText::FromString(TEXT("KEYBOARD & MOUSE")));Heading->SetColorAndOpacity(SecondaryHeadingColor);
 auto HeadingFont=SecondaryHeadingFont.Size>0?SecondaryHeadingFont:Heading->GetFont();
 if(SecondaryHeadingFont.Size<=0){HeadingFont.Size=CollectionDetailNameFontSize;HeadingFont.TypefaceFontName=TEXT("Bold");}
 Heading->SetFont(HeadingFont);Panel->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0,0,0,28));
 AddSecondaryPageDivider(Panel);
 auto* RootPanel=Panel;
 auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();
 RootPanel->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 Panel=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Panel);
 auto BodyFont=SecondaryBodyFont.Size>0?SecondaryBodyFont:Heading->GetFont();BodyFont.Size=CollectionDescriptionFontSize;
 auto* Help=WidgetTree->ConstructWidget<UTextBlock>();Help->SetFont(BodyFont);Help->SetColorAndOpacity(SecondaryBodyColor);
 Help->SetText(FText::FromString(TEXT("Keyboard & mouse. Select a binding, then press a key. Esc cancels.")));Help->SetAutoWrapText(true);
 Panel->AddChildToVerticalBox(Help)->SetPadding(FMargin(0,0,0,16));
 const TCHAR* Labels[]={TEXT("Move Forward"),TEXT("Move Backward"),TEXT("Move Left"),TEXT("Move Right"),TEXT("Swap Character"),TEXT("Dash"),TEXT("Interact"),TEXT("Combo Ability")};
 const auto Actions=UHeavensDivideGameUserSettings::GetBindableActions();KeybindSelectors.Reset();
 for(int32 Index=0;Index<Actions.Num();++Index)
 {
  auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Panel->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,4));
  auto* Label=WidgetTree->ConstructWidget<UTextBlock>();Label->SetText(FText::FromString(Labels[Index]));Label->SetFont(BodyFont);Label->SetColorAndOpacity(SecondaryBodyColor);
  auto* LabelSlot=Row->AddChildToHorizontalBox(Label);LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LabelSlot->SetVerticalAlignment(VAlign_Center);
  auto* Selector=WidgetTree->ConstructWidget<UMenuKeybindSelector>(UMenuKeybindSelector::StaticClass(),FName(FString(TEXT("Bind_"))+Actions[Index].ToString()));
  Selector->InitializeBinding(this,Actions[Index]);
  FTextBlockStyle TextStyle=Selector->GetTextStyle();TextStyle.SetFont(BodyFont);TextStyle.SetColorAndOpacity(FSlateColor::UseForeground());Selector->SetTextStyle(TextStyle);
  Selector->SetButtonStyle(BloodshiftMenu::ActionStyle());Selector->SetMargin(FMargin(16,8));
  auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetMinDesiredWidth(300);Size->SetMinDesiredHeight(42);Size->SetContent(Selector);Row->AddChildToHorizontalBox(Size);
  KeybindSelectors.Add(Selector);
 }
 KeybindStatus=WidgetTree->ConstructWidget<UTextBlock>();KeybindStatus->SetFont(BodyFont);KeybindStatus->SetColorAndOpacity(SecondaryBodyColor);KeybindStatus->SetAutoWrapText(true);
 Panel->AddChildToVerticalBox(KeybindStatus)->SetPadding(FMargin(0,12));
 auto* BindingActions=WidgetTree->ConstructWidget<UHorizontalBox>(); Panel->AddChildToVerticalBox(BindingActions);
 AddPageAction(BindingActions,FText::FromString(TEXT("RESTORE DEFAULTS")),TEXT("ResetKeybindsButton"))->OnClicked.AddDynamic(this,&UMainMenuWidget::HandleResetKeybinds);
 RefreshKeybindRows();return RootPanel;
}
void UMainMenuWidget::RefreshKeybindRows()
{
 TGuardValue<bool> Guard(bRefreshingKeybinds,true);
 const auto* Settings=UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
 for(const auto& Row:KeybindSelectors)if(Row)Row->SetSelectedKey(FInputChord(Settings?Settings->GetKeyBinding(Row->BindingAction):UHeavensDivideGameUserSettings::GetDefaultBinding(Row->BindingAction)));
}
void UMainMenuWidget::ApplyKeyBinding(FName Action,FInputChord Key)
{
 if(bRefreshingKeybinds)return;
 auto* Settings=UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
 const bool Saved=Settings&&Settings->SetKeyBinding(Action,Key.Key);
 if(KeybindStatus)KeybindStatus->SetText(FText::FromString(Saved?TEXT("Saved. Conflicting bindings exchange keys."):TEXT("Choose a keyboard or mouse button. Esc and ~ are reserved.")));
 RefreshKeybindRows();
}
bool UMainMenuWidget::IsSelectingKeybind() const
{
 return KeybindSelectors.ContainsByPredicate([](const auto& Row){return Row&&Row->GetIsSelectingKey();});
}
void UMainMenuWidget::HandleKeybindSettings()
{
 RefreshKeybindRows();if(KeybindStatus)KeybindStatus->SetText(FText::FromString(TEXT("Changes save automatically. Conflicting bindings exchange keys.")));
 if(SettingsSwitcher)SettingsSwitcher->SetActiveWidgetIndex(1);
 if(!KeybindSelectors.IsEmpty())KeybindSelectors[0]->SetUserFocus(GetOwningPlayer());
}
void UMainMenuWidget::HandleResetKeybinds()
{
 if(auto* Settings=UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings())Settings->ResetKeyBindings();
 RefreshKeybindRows();if(KeybindStatus)KeybindStatus->SetText(FText::FromString(TEXT("Default bindings restored.")));
}

UVerticalBox* UMainMenuWidget::BuildGeneralSettingsPanel()
{
    UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsPanel"));
    auto Text = [this](const TCHAR* Value, int32 Size, FLinearColor Color)
    {
        UTextBlock* Result = WidgetTree->ConstructWidget<UTextBlock>();
        Result->SetText(FText::FromString(Value)); Result->SetFont(BloodshiftMenu::BodyFont(Size));
        Result->SetColorAndOpacity(Color); return Result;
    };
    UTextBlock* Heading = Text(TEXT("SETTINGS"), 32, BloodshiftMenu::Paper);
    FSlateFontInfo HeadingFont = SecondaryHeadingFont; HeadingFont.Size = 32; Heading->SetFont(HeadingFont);
    Panel->AddChildToVerticalBox(Heading);
    Panel->AddChildToVerticalBox(Text(TEXT("Make the fight feel right."), 18, BloodshiftMenu::Muted))->SetPadding(FMargin(0, 8, 0, 0));
    AddSecondaryPageDivider(Panel);
    UHorizontalBox* AutoRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    Panel->AddChildToVerticalBox(AutoRow)->SetPadding(FMargin(0, 8, 0, 30));
    auto* AutoLabelSlot = AutoRow->AddChildToHorizontalBox(Text(TEXT("Auto targeting"), 21, BloodshiftMenu::Paper));
    AutoLabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); AutoLabelSlot->SetVerticalAlignment(VAlign_Center);
    AutoTargetingCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("AutoTargetingCheckBox"));
    FCheckBoxStyle CheckStyle = AutoTargetingCheckBox->GetWidgetStyle();
    CheckStyle.SetForegroundColor(BloodshiftMenu::Gold);
    for (FSlateBrush* Brush : { &CheckStyle.UncheckedImage, &CheckStyle.UncheckedHoveredImage, &CheckStyle.UncheckedPressedImage,
        &CheckStyle.CheckedImage, &CheckStyle.CheckedHoveredImage, &CheckStyle.CheckedPressedImage })
    {
        Brush->ImageSize = FVector2D(24, 24);
        Brush->TintColor = BloodshiftMenu::Gold;
    }
    AutoTargetingCheckBox->SetWidgetStyle(CheckStyle);
    AutoRow->AddChildToHorizontalBox(AutoTargetingCheckBox)->SetVerticalAlignment(VAlign_Center);
    AutoTargetingStateText = Text(TEXT("ON"), 18, BloodshiftMenu::Gold);
    USizeBox* StateWidth = WidgetTree->ConstructWidget<USizeBox>(); StateWidth->SetWidthOverride(58); StateWidth->SetContent(AutoTargetingStateText);
    AutoRow->AddChildToHorizontalBox(StateWidth)->SetPadding(FMargin(14, 0, 0, 0));
    AutoTargetingCheckBox->OnCheckStateChanged.AddDynamic(this, &UMainMenuWidget::HandleAutoTargetingChanged);
    auto AddSlider = [this, Panel, Text](const TCHAR* Label, const TCHAR* Help, FName Name, USlider*& OutSlider, UTextBlock*& OutValue)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        Panel->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 8, 0, 28));
        UVerticalBox* Labels = WidgetTree->ConstructWidget<UVerticalBox>();
        Labels->AddChildToVerticalBox(Text(Label, 21, BloodshiftMenu::Paper));
        Labels->AddChildToVerticalBox(Text(Help, 15, BloodshiftMenu::Muted))->SetPadding(FMargin(0, 6, 0, 0));
        Row->AddChildToHorizontalBox(Labels)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        USizeBox* Track = WidgetTree->ConstructWidget<USizeBox>(); Track->SetWidthOverride(300); Track->SetHeightOverride(36);
        OutSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), Name);
        OutSlider->SetMinValue(0); OutSlider->SetMaxValue(1); OutSlider->SetStepSize(.01f);
        FSliderStyle SliderStyle = OutSlider->GetWidgetStyle();
        FSlateBrush Bar = BloodshiftMenu::Solid(FLinearColor::White); Bar.ImageSize = FVector2D(1, 4);
        FSlateBrush Thumb = FSlateRoundedBoxBrush(BloodshiftMenu::Gold, 2.0f, BloodshiftMenu::Paper, 1.0f, FVector2D(12, 22));
        FSlateBrush HoverThumb = FSlateRoundedBoxBrush(BloodshiftMenu::Paper, 2.0f, BloodshiftMenu::Gold, 1.0f, FVector2D(12, 22));
        SliderStyle.SetNormalBarImage(Bar).SetHoveredBarImage(Bar).SetNormalThumbImage(Thumb).SetHoveredThumbImage(HoverThumb).SetBarThickness(4);
        OutSlider->SetWidgetStyle(SliderStyle);
        OutSlider->SetSliderBarColor(BloodshiftMenu::Rule); OutSlider->SetSliderHandleColor(FLinearColor::White);
        OutSlider->SetToolTipText(FText::FromString(Help)); Track->SetContent(OutSlider);
        Row->AddChildToHorizontalBox(Track)->SetVerticalAlignment(VAlign_Center);
        OutValue = Text(TEXT("100%"), 19, BloodshiftMenu::Paper);
        OutValue->SetJustification(ETextJustify::Right);
        USizeBox* ValueWidth = WidgetTree->ConstructWidget<USizeBox>(); ValueWidth->SetWidthOverride(72); ValueWidth->SetContent(OutValue);
        auto* ValueSlot = Row->AddChildToHorizontalBox(ValueWidth); ValueSlot->SetVerticalAlignment(VAlign_Center); ValueSlot->SetPadding(FMargin(16, 0, 0, 0));
    };
    USlider* Shake = nullptr; UTextBlock* ShakeValue = nullptr;
    AddSlider(TEXT("Camera shake"), TEXT("Set to zero for a steady camera."), TEXT("CameraShakeSlider"), Shake, ShakeValue);
    CameraShakeSlider = Shake; CameraShakeValueText = ShakeValue;
    CameraShakeSlider->OnValueChanged.AddDynamic(this, &UMainMenuWidget::HandleCameraShakeChanged);
    USlider* Volume = nullptr; UTextBlock* VolumeValue = nullptr;
    AddSlider(TEXT("Master volume"), TEXT("Overall game audio level."), TEXT("MasterVolumeSlider"), Volume, VolumeValue);
    MasterVolumeSlider = Volume; MasterVolumeValueText = VolumeValue;
    MasterVolumeSlider->OnValueChanged.AddDynamic(this, &UMainMenuWidget::HandleMasterVolumeChanged);
    UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
    Panel->AddChildToVerticalBox(Actions)->SetPadding(FMargin(-12, 8, 0, 0));
    AddPageAction(Actions, FText::FromString(TEXT("KEYBOARD & MOUSE")), TEXT("KeybindSettingsButton"))->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleKeybindSettings);
    RefreshAutoTargetingSetting();
    return Panel;
}

void UMainMenuWidget::RefreshAutoTargetingSetting()
{
	const UHeavensDivideGameUserSettings* Settings = UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
	const bool bEnabled = !Settings || Settings->IsAutoTargetingEnabled();
    const float Volume = Settings ? Settings->GetMasterVolume() : 1.f;
    if (MasterVolumeSlider) MasterVolumeSlider->SetValue(Volume);
    if (MasterVolumeValueText) MasterVolumeValueText->SetText(FText::AsPercent(Volume));
	const float ShakeIntensity = Settings ? Settings->GetCameraShakeIntensity() : 1.0f;
	if (CameraShakeSlider) CameraShakeSlider->SetValue(ShakeIntensity);
	if (CameraShakeValueText) CameraShakeValueText->SetText(FText::AsPercent(ShakeIntensity));
	if (AutoTargetingCheckBox) AutoTargetingCheckBox->SetIsChecked(bEnabled);
	if (AutoTargetingStateText) AutoTargetingStateText->SetText(FText::FromString(bEnabled ? TEXT("ON") : TEXT("OFF")));
}

void UMainMenuWidget::HandleMasterVolumeChanged(float Value)
{
    if (auto* Settings = UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings())
        Settings->SetMasterVolume(Value);
    if (MasterVolumeValueText) MasterVolumeValueText->SetText(FText::AsPercent(Value));
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
	const float TileScale = 1.10f;
	const float TileDisplayExtent = 134.0f * TileScale;
	const float TileSpacing = 20.0f;
	const float ContentExtent = TileDisplayExtent - 6.0f * TileScale;
	const float IconExtent = FMath::Max(1.0f, ContentExtent - 2.0f * FMath::Max(0.0f, CollectionTileIconPadding) * TileScale);
	// Four columns, with a separate gutter for the scrollbar.
	const int32 ColumnCount = FMath::Max(1, FMath::FloorToInt(680.0f / (TileDisplayExtent + TileSpacing)));
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
		BackgroundFallback->SetBrushColor(bUnlocked ? BloodshiftMenu::Surface : BloodshiftMenu::Ink);
		BackgroundFallback->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddFillLayer(ContentLayers, BackgroundFallback);
		UImage* TileBackground = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			*FString::Printf(TEXT("Img_TileBackground_%d"), Index));
		TileBackground->SetBrushFromTexture(TileBackgroundTexture, false);
		TileBackground->SetDesiredSizeOverride(FVector2D(ContentExtent));
		TileBackground->SetVisibility(ESlateVisibility::Collapsed);
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
			LockedOverlay->SetVisibility(ESlateVisibility::Collapsed);
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
		UNativeWidgetHost* TileOrnament = WidgetTree->ConstructWidget<UNativeWidgetHost>();
		TileOrnament->SetContent(SNew(BloodshiftMenu::SInsetSurface).Fill(false).Compact(true));
		TileOrnament->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddFillLayer(TileLayers, TileOrnament);
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
	const FLinearColor Accent = BloodshiftMenu::Gold;
	for (int32 Index = 0; Index < CollectionTileButtons.Num(); ++Index)
	{
		const bool bSelected = CollectionDefinitions.IsValidIndex(Index) && CollectionDefinitions[Index] == SelectedCollectionUpgrade;
		const bool bHot = bSelected || (CollectionTileButtons[Index] && (CollectionTileButtons[Index]->IsHovered()
			|| (bShowFocusHighlight && CollectionTileButtons[Index]->HasAnyUserFocus())));
		if (CollectionTileSelectionBorders.IsValidIndex(Index) && CollectionTileSelectionBorders[Index])
			CollectionTileSelectionBorders[Index]->SetBrushColor(bHot ? Accent : BloodshiftMenu::Rule);
		if (CollectionTileSelectionImages.IsValidIndex(Index) && CollectionTileSelectionImages[Index])
		{
			CollectionTileSelectionImages[Index]->SetColorAndOpacity(Accent);
			CollectionTileSelectionImages[Index]->SetVisibility(ESlateVisibility::Hidden);
		}
	}
    const UButton* Tabs[] = { SamuraiCollectionTab, NinjaCollectionTab, SynergyCollectionTab };
    const EUpgradeCategory Categories[] = { EUpgradeCategory::Samurai, EUpgradeCategory::Ninja, EUpgradeCategory::Synergy };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        if (UButton* Tab = const_cast<UButton*>(Tabs[Index]))
        {
            FButtonStyle Style = BloodshiftMenu::ActionStyle();
            if (SelectedCollectionCategory == Categories[Index])
                Style.SetNormal(Style.Hovered).SetNormalForeground(BloodshiftMenu::Ink);
            Tab->SetStyle(Style);
        }
    }
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
	EntrySize->SetMinDesiredWidth(340.0f);
	EntrySize->SetMinDesiredHeight(52.0f);
	UOverlay* EntryLayers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	EntrySize->SetContent(EntryLayers);
	USizeBox* InkSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	InkSize->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* InkSlot = EntryLayers->AddChildToOverlay(InkSize);
	InkSlot->SetHorizontalAlignment(HAlign_Fill);
	InkSlot->SetVerticalAlignment(VAlign_Fill);
	UImage* InkImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	InkImage->SetBrush(MenuInkStyle::MakeBrush(InkBrushTexture));
	InkImage->SetColorAndOpacity(MainMenuReadability::Gold);
	InkImage->SetOpacity(0.0f);
	InkImage->SetRenderTransformPivot(FVector2D(0.0f, 0.5f));
	InkImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	InkSize->SetContent(InkImage);
	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Label);
	LabelText->SetJustification(ETextJustify::Left);
	LabelText->SetColorAndOpacity(FSlateColor(MainMenuReadability::Label));
	FSlateFontInfo ButtonFont = MenuButtonFont.Size > 0 ? MenuButtonFont : LabelText->GetFont();
	if (MenuButtonFont.Size <= 0) ButtonFont.Size = 24;
	ButtonFont.OutlineSettings.OutlineSize = FMath::Max(1, ButtonFont.OutlineSettings.OutlineSize);
	ButtonFont.OutlineSettings.OutlineColor = MainMenuReadability::Ink;
	LabelText->SetFont(ButtonFont);
	LabelText->SetShadowOffset(FVector2D(0.0f, 2.0f));
	LabelText->SetShadowColorAndOpacity(FLinearColor(0.002f, 0.004f, 0.007f, 0.80f));
	UOverlaySlot* LabelSlot = EntryLayers->AddChildToOverlay(LabelText);
	LabelSlot->SetHorizontalAlignment(HAlign_Left);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	LabelSlot->SetPadding(MenuInkStyle::ContentPadding(Label, ButtonFont));
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
			|| (bSettingsPopupOpen && Button->GetFName() == TEXT("SettingsButton"))
			|| (bSkillTreeOpen && Button->GetFName() == TEXT("SkillTreeButton"))
			|| (bResetConfirmationOpen && Button->GetFName() == TEXT("ResetProgressButton"))
			|| (bShowFocusHighlight && Button->HasAnyUserFocus());
		if (!MenuEntryRevealAmounts.IsValidIndex(Index)) MenuEntryRevealAmounts.SetNumZeroed(MenuEntryButtons.Num());
		float& Reveal = MenuEntryRevealAmounts[Index];
		Reveal = FMath::FInterpConstantTo(Reveal, bHighlighted ? 1.0f : 0.0f, DeltaTime, Speed);
		if (MenuEntryBrushImages.IsValidIndex(Index) && MenuEntryBrushImages[Index])
		{
			MenuEntryBrushImages[Index]->SetOpacity(InkBrushTexture ? Reveal : 0.0f);
		}
		if (MenuEntryLabels.IsValidIndex(Index) && MenuEntryLabels[Index])
		{
			const FLinearColor Normal = MainMenuReadability::Label;
			const FLinearColor Highlighted = InkBrushTexture ? MainMenuReadability::Ink : MainMenuReadability::Gold;
			MenuEntryLabels[Index]->SetColorAndOpacity(FSlateColor(FMath::Lerp(Normal, Highlighted, Reveal)));
			MenuEntryLabels[Index]->SetShadowColorAndOpacity(FLinearColor(0.002f, 0.004f, 0.007f,
				FMath::Lerp(0.80f, InkBrushTexture ? 0.0f : 0.80f, Reveal)));
		}
	}
}

void UMainMenuWidget::ShowMainPanel()
{
	if (bInRunSettings) { InRunSettingsClosed.ExecuteIfBound(); return; }
	SetSkillTreeVisible(false);
	if (auto* Meta = GetGameInstance() ? GetGameInstance()->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr) Meta->RetrySkillReward();
	SetResetConfirmationVisible(false);
	SetSettingsPopupVisible(false);
	SetCollectionVisible(false);
	if (MenuSwitcher) MenuSwitcher->SetActiveWidgetIndex(0);
	if (NewRunButton) NewRunButton->SetUserFocus(GetOwningPlayer());
}

void UMainMenuWidget::ShowCollectionPanel()
{
	SetSkillTreeVisible(false);
	SetResetConfirmationVisible(false);
	SetSettingsPopupVisible(false);
	if (MenuSwitcher) MenuSwitcher->SetActiveWidgetIndex(0);
	SetCollectionVisible(true);
	RefreshCollection();
	if (CollectionTileButtons.Num() > 0 && CollectionTileButtons[0]) CollectionTileButtons[0]->SetUserFocus(GetOwningPlayer());
}

void UMainMenuWidget::OpenInRunSettings(FSimpleDelegate OnClosed)
{
    bInRunSettings = true;
    InRunSettingsClosed = OnClosed;
    if (auto* Root = Cast<UOverlay>(WidgetTree->RootWidget))
        for (auto* Child : Root->GetAllChildren())
            Child->SetVisibility(Child == SettingsPopupOverlay ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (auto* Background = Cast<UBorder>(GetWidgetFromName(TEXT("MenuBackgroundFallback"))))
    {
        Background->SetBrushColor(FLinearColor(.005f, .008f, .014f, .82f));
        Background->SetVisibility(ESlateVisibility::HitTestInvisible);
        if (auto* BackgroundSlot = Cast<UOverlaySlot>(Background->Slot))
        {
            BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
            BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
        }
    }
    if (SettingsPopupOverlay)
        if (auto* PageSlot = Cast<UOverlaySlot>(SettingsPopupOverlay->Slot)) PageSlot->SetPadding(FMargin(32));
    ShowSettingsPanel();
}

void UMainMenuWidget::ShowSettingsPanel()
{
	if(SettingsSwitcher)SettingsSwitcher->SetActiveWidgetIndex(0);
	SetSkillTreeVisible(false);
	SetResetConfirmationVisible(false);
	SetCollectionVisible(false);
	RefreshAutoTargetingSetting();
	if (MenuSwitcher) MenuSwitcher->SetActiveWidgetIndex(0);
	SetSettingsPopupVisible(true);
	if (AutoTargetingCheckBox) AutoTargetingCheckBox->SetUserFocus(GetOwningPlayer());
}

void UMainMenuWidget::ShowResetConfirmation()
{
	SetSkillTreeVisible(false);
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

void UMainMenuWidget::RefreshPagePresentation()
{
    const bool bPageOpen = bCollectionOpen || bSkillTreeOpen || bSettingsPopupOpen || bResetConfirmationOpen;
    if (LogoPresentation) LogoPresentation->SetRenderScale(FVector2D(bPageOpen ? 0.67f : 1.0f));
}

void UMainMenuWidget::SetResetConfirmationVisible(bool bVisible)
{
	bResetConfirmationOpen = bVisible;
	RefreshPagePresentation();
	if (ResetConfirmationOverlay)
	{
		ResetConfirmationOverlay->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UMainMenuWidget::SetSettingsPopupVisible(bool bVisible)
{
	bSettingsPopupOpen = bVisible;
	RefreshPagePresentation();
	if (SettingsPopupOverlay)
	{
		SettingsPopupOverlay->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UMainMenuWidget::SetCollectionVisible(bool bVisible)
{
	bCollectionOpen = bVisible;
	RefreshPagePresentation();
	if (CollectionOverlay) CollectionOverlay->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UMainMenuWidget::SetSkillTreeVisible(bool bVisible)
{
	bSkillTreeOpen = bVisible;
	RefreshPagePresentation();
	if (SkillTreeOverlay) SkillTreeOverlay->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UMainMenuWidget::HandleNewRun()
{
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Lvl_B1_Lvl1"));
}

void UMainMenuWidget::HandleCollection() { ShowCollectionPanel(); }
void UMainMenuWidget::ShowTesterBalance()
{
    if (bInRunSettings || !TesterBalancePage) return;
    ShowMainPanel();
    TesterBalancePage->Open(this);
    MenuSwitcher->SetActiveWidget(TesterBalancePage);
}
void UMainMenuWidget::HandleSettings() { ShowSettingsPanel(); }
void UMainMenuWidget::HandleResetProgress() { ShowResetConfirmation(); }
void UMainMenuWidget::HandleBack() { if(bSettingsPopupOpen&&SettingsSwitcher&&SettingsSwitcher->GetActiveWidgetIndex()==1){ShowSettingsPanel();FocusNamedWidget(TEXT("KeybindSettingsButton"));}else ShowMainPanel(); }
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
	if(IsSelectingKeybind())return FReply::Unhandled();
	bShowFocusHighlight = true;
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		if (bSkillTreeOpen)
		{
			ShowMainPanel();
			return FReply::Handled();
		}
		if (bResetConfirmationOpen)
		{
			SetResetConfirmationVisible(false);
			return FReply::Handled();
		}
		if (bSettingsPopupOpen)
		{
			HandleBack();
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
	if(IsSelectingKeybind())return FReply::Unhandled();
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
			HandleBack();
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

void UMainMenuWidget::ShowSkillTree()
{
 ShowMainPanel();
 if (!SkillTreeOverlay) return;
 if (auto* Tree = CreateWidget<UMetaSkillTreeWidget>(GetOwningPlayer()))
 {
  Tree->MenuOwner = this;
  // Let the tree use all available space beside the navigation column.
  // Its own scale box preserves proportions at every viewport size.
  SkillTreeOverlay->SetContent(Tree);
  SetSkillTreeVisible(true);
  Tree->SetUserFocus(GetOwningPlayer());
 }
}
