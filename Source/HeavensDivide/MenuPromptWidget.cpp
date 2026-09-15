#include "MenuPromptWidget.h"

#include "MainMenuWidget.h"
#include "MenuInkStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

UMenuPromptWidget::UMenuPromptWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UMainMenuWidget> Menu(TEXT("/Game/HeavensDivide/Blueprints/UI/MainMenu/WBP_MainMenu"));
	MenuStyleClass = Menu.Succeeded() ? Menu.Class.Get() : UMainMenuWidget::StaticClass();
}

UVerticalBox* UMenuPromptWidget::BuildPromptPanel(FName TitleName, const FText& Heading, FVector2D PanelSize)
{
	if (!WidgetTree || WidgetTree->RootWidget) return nullptr;

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MenuPromptRoot"));
	WidgetTree->RootWidget = Root;
	UBorder* ScreenFade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuPromptScreenFade"));
	ScreenFade->SetBrushColor(FLinearColor(0.005f, 0.008f, 0.014f, 0.82f));
	UOverlaySlot* FadeSlot = Root->AddChildToOverlay(ScreenFade);
	FadeSlot->SetHorizontalAlignment(HAlign_Fill);
	FadeSlot->SetVerticalAlignment(VAlign_Fill);

	const UMainMenuWidget* Style = MenuStyleClass.GetDefaultObject();
	UScaleBox* Scaler = WidgetTree->ConstructWidget<UScaleBox>();
	Scaler->SetStretch(EStretch::ScaleToFit);
	Scaler->SetStretchDirection(EStretchDirection::DownOnly);
	UOverlaySlot* ScalerSlot = Root->AddChildToOverlay(Scaler);
	ScalerSlot->SetHorizontalAlignment(HAlign_Fill);
	ScalerSlot->SetVerticalAlignment(VAlign_Fill);
	ScalerSlot->SetPadding(FMargin(32.0f));
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(PanelSize.X);
	Scaler->SetContent(Size);
	UVerticalBox* PageStack = WidgetTree->ConstructWidget<UVerticalBox>();
	Size->SetContent(PageStack);
	USizeBox* ArtSize = WidgetTree->ConstructWidget<USizeBox>();
	ArtSize->SetHeightOverride(PanelSize.Y);
	UOverlay* PanelLayers = WidgetTree->ConstructWidget<UOverlay>();
	ArtSize->SetContent(PanelLayers);
	PageStack->AddChildToVerticalBox(ArtSize);
	PromptActions = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PromptActions"));
	PageStack->AddChildToVerticalBox(PromptActions)->SetPadding(FMargin(0, 24, 0, 0));
	UBorder* Fallback = WidgetTree->ConstructWidget<UBorder>();
	Fallback->SetBrushColor(Style->SettingsPopupBackgroundColor);
	Fallback->SetVisibility(Style->CollectionPanelTexture ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	UOverlaySlot* FallbackSlot = PanelLayers->AddChildToOverlay(Fallback);
	FallbackSlot->SetHorizontalAlignment(HAlign_Fill);
	FallbackSlot->SetVerticalAlignment(VAlign_Fill);
	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MenuPromptBackground"));
	Background->SetBrushFromTexture(Style->CollectionPanelTexture, false);
	Background->SetVisibility(Style->CollectionPanelTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	UOverlaySlot* BackgroundSlot = PanelLayers->AddChildToOverlay(Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuPromptStack"));
	UOverlaySlot* ContentSlot = PanelLayers->AddChildToOverlay(Stack);
	ContentSlot->SetHorizontalAlignment(HAlign_Center);
	ContentSlot->SetVerticalAlignment(VAlign_Center);
	ContentSlot->SetPadding(FMargin(54.0f, 40.0f));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TitleName);
	Title->SetText(Heading);
	Title->SetJustification(ETextJustify::Center);
	Title->SetColorAndOpacity(Style->SecondaryHeadingColor);
	FSlateFontInfo TitleFont = Style->SecondaryHeadingFont.Size > 0 ? Style->SecondaryHeadingFont : Title->GetFont();
	if (Style->SecondaryHeadingFont.Size <= 0) { TitleFont.Size = 38; TitleFont.TypefaceFontName = TEXT("Bold"); }
	Title->SetFont(TitleFont);
	Stack->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	if (Style->HorizontalBrushTexture)
	{
		USizeBox* DividerSize = WidgetTree->ConstructWidget<USizeBox>();
		DividerSize->SetHeightOverride(Style->HorizontalBrushSize.Y);
		UImage* Divider = WidgetTree->ConstructWidget<UImage>();
		Divider->SetBrushFromTexture(Style->HorizontalBrushTexture, false);
		Divider->SetVisibility(ESlateVisibility::HitTestInvisible);
		DividerSize->SetContent(Divider);
		Stack->AddChildToVerticalBox(DividerSize)->SetPadding(FMargin(0, 0, 0, 30));
	}

	return Stack;
}

UButton* UMenuPromptWidget::AddPromptButton(UHorizontalBox* Parent, const FText& Label, FName WidgetName)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);
	const UMainMenuWidget* Menu = MenuStyleClass.GetDefaultObject();
	FSlateBrush Empty;
	Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
	FButtonStyle Style = Button->GetStyle();
	Style.SetNormal(Empty); Style.SetHovered(Empty); Style.SetPressed(Empty); Style.SetDisabled(Empty);
	Button->SetStyle(Style);
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetMinDesiredWidth(340.0f);
	Size->SetMinDesiredHeight(52.0f);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
	Size->SetContent(Layers);
	USizeBox* InkSize = WidgetTree->ConstructWidget<USizeBox>();
	InkSize->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* InkSlot = Layers->AddChildToOverlay(InkSize);
	InkSlot->SetHorizontalAlignment(HAlign_Fill);
	InkSlot->SetVerticalAlignment(VAlign_Fill);
	UImage* Ink = WidgetTree->ConstructWidget<UImage>();
	Ink->SetBrush(MenuInkStyle::MakeBrush(Menu->InkBrushTexture));
	Ink->SetOpacity(0.0f);
	Ink->SetRenderTransformPivot(FVector2D(0.0f, 0.5f));
	InkSize->SetContent(Ink);
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
	Text->SetText(Label);
	Text->SetJustification(ETextJustify::Center);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.73f, 0.75f)));
	FSlateFontInfo Font = Menu->MenuButtonFont.Size > 0 ? Menu->MenuButtonFont : Text->GetFont();
	if (Menu->MenuButtonFont.Size <= 0) Font.Size = 24;
	Text->SetFont(Font);
	Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* LabelSlot = Layers->AddChildToOverlay(Text);
	LabelSlot->SetHorizontalAlignment(HAlign_Fill);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	LabelSlot->SetPadding(MenuInkStyle::ContentPadding(Label, Font));
	Button->AddChild(Size);
	ChoiceButtons.Add(Button);
	ChoiceInk.Add(Ink);
	ChoiceLabels.Add(Text);
	InkReveal.Add(0.0f);
	UHorizontalBoxSlot* ButtonSlot = Parent->AddChildToHorizontalBox(Button);
	ButtonSlot->SetPadding(FMargin(10.0f));
	ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	return Button;
}

UTextBlock* UMenuPromptWidget::AddPromptText(UVerticalBox* Parent, FName Name, const FText& Text, bool bEmphasis, float BottomPadding)
{
    const auto* Style = MenuStyleClass.GetDefaultObject();
    auto* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    Block->SetText(Text);
    Block->SetJustification(ETextJustify::Center);
    const FSlateFontInfo& Font = bEmphasis ? Style->SecondaryHeadingFont : Style->SecondaryBodyFont;
    if (Font.Size > 0) Block->SetFont(Font);
    Block->SetColorAndOpacity(bEmphasis ? Style->SecondaryHeadingColor : Style->SecondaryBodyColor);
    Parent->AddChildToVerticalBox(Block)->SetPadding(FMargin(0, 0, 0, BottomPadding));
    return Block;
}

void UMenuPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const UMainMenuWidget* Style = MenuStyleClass.GetDefaultObject();
	for (int32 Index = 0; Index < ChoiceButtons.Num(); ++Index)
	{
		const bool bHighlighted = ChoiceButtons[Index]->IsHovered()
			|| (bShowFocusHighlight && ChoiceButtons[Index]->HasAnyUserFocus());
		float& Reveal = InkReveal[Index];
		Reveal = FMath::FInterpConstantTo(Reveal, bHighlighted ? 1.0f : 0.0f, InDeltaTime,
			1.0f / FMath::Max(0.05f, Style->InkRevealDuration));
		ChoiceInk[Index]->SetOpacity(Style->InkBrushTexture ? Reveal : 0.0f);
		const FLinearColor Highlight = Style->InkBrushTexture ? FLinearColor(0.01f, 0.01f, 0.01f) : Style->SecondaryHeadingColor;
		ChoiceLabels[Index]->SetColorAndOpacity(FMath::Lerp(FLinearColor(0.72f, 0.73f, 0.75f), Highlight, Reveal));
	}
}

FReply UMenuPromptWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!InMouseEvent.GetCursorDelta().IsNearlyZero()) bShowFocusHighlight = false;
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UMenuPromptWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	bShowFocusHighlight = true;
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
