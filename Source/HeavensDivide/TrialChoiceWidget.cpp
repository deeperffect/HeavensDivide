#include "TrialChoiceWidget.h"

#include "MainMenuWidget.h"
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

UTrialChoiceWidget::UTrialChoiceWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UMainMenuWidget> Menu(TEXT("/Game/HeavensDivide/Blueprints/UI/MainMenu/WBP_MainMenu"));
	MenuStyleClass = Menu.Succeeded() ? Menu.Class.Get() : UMainMenuWidget::StaticClass();
}

void UTrialChoiceWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildChoiceScreen();
}

void UTrialChoiceWidget::BuildChoiceScreen()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("TrialChoiceRoot"));
	WidgetTree->RootWidget = Root;
	UBorder* ScreenFade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TrialChoiceScreenFade"));
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
	Size->SetWidthOverride(900.0f);
	Size->SetHeightOverride(500.0f);
	Scaler->SetContent(Size);
	UOverlay* PanelLayers = WidgetTree->ConstructWidget<UOverlay>();
	Size->SetContent(PanelLayers);
	UBorder* Fallback = WidgetTree->ConstructWidget<UBorder>();
	Fallback->SetBrushColor(Style->SettingsPopupBackgroundColor);
	Fallback->SetVisibility(Style->CollectionPanelTexture ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	UOverlaySlot* FallbackSlot = PanelLayers->AddChildToOverlay(Fallback);
	FallbackSlot->SetHorizontalAlignment(HAlign_Fill);
	FallbackSlot->SetVerticalAlignment(VAlign_Fill);
	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TrialChoiceBackground"));
	Background->SetBrushFromTexture(Style->CollectionPanelTexture, false);
	Background->SetVisibility(Style->CollectionPanelTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	UOverlaySlot* BackgroundSlot = PanelLayers->AddChildToOverlay(Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TrialChoiceStack"));
	UOverlaySlot* ContentSlot = PanelLayers->AddChildToOverlay(Stack);
	ContentSlot->SetHorizontalAlignment(HAlign_Center);
	ContentSlot->SetVerticalAlignment(VAlign_Center);
	ContentSlot->SetPadding(FMargin(54.0f, 40.0f));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TrialChoiceTitle"));
	Title->SetText(FText::FromString(TEXT("CHOOSE YOUR TRIAL")));
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

	UHorizontalBox* Choices = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TrialChoices"));
	Stack->AddChildToVerticalBox(Choices);
	SamuraiButton = AddTrialButton(Choices, FText::FromString(TEXT("SAMURAI TRIAL")), TEXT("SamuraiTrialButton"));
	UButton* NinjaButton = AddTrialButton(Choices, FText::FromString(TEXT("NINJA TRIAL")), TEXT("NinjaTrialButton"));
	SamuraiButton->OnClicked.AddDynamic(this, &UTrialChoiceWidget::HandleSamuraiSelected);
	NinjaButton->OnClicked.AddDynamic(this, &UTrialChoiceWidget::HandleNinjaSelected);
}

UButton* UTrialChoiceWidget::AddTrialButton(UHorizontalBox* Parent, const FText& Label, FName WidgetName)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);
	const UMainMenuWidget* Menu = MenuStyleClass.GetDefaultObject();
	FSlateBrush Empty;
	Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
	FButtonStyle Style = Button->GetStyle();
	Style.SetNormal(Empty); Style.SetHovered(Empty); Style.SetPressed(Empty); Style.SetDisabled(Empty);
	Button->SetStyle(Style);
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(340.0f);
	Size->SetHeightOverride(52.0f);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
	Size->SetContent(Layers);
	USizeBox* InkSize = WidgetTree->ConstructWidget<USizeBox>();
	InkSize->SetHeightOverride(100.0f);
	InkSize->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* InkSlot = Layers->AddChildToOverlay(InkSize);
	InkSlot->SetHorizontalAlignment(HAlign_Fill);
	InkSlot->SetVerticalAlignment(VAlign_Center);
	UImage* Ink = WidgetTree->ConstructWidget<UImage>();
	Ink->SetBrushFromTexture(Menu->InkBrushTexture, true);
	Ink->SetOpacity(0.0f);
	Ink->SetRenderTransformPivot(FVector2D(0.0f, 0.5f));
	Ink->SetRenderScale(FVector2D(0.84f, 1.0f));
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
	LabelSlot->SetPadding(FMargin(18, 0));
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

void UTrialChoiceWidget::InitializeTrialChoice(ARunObjectiveDirector* InObjectiveDirector)
{
	ObjectiveDirector = InObjectiveDirector;
	bChoiceSubmitted = false;
	if (SamuraiButton) SamuraiButton->SetKeyboardFocus();
}

void UTrialChoiceWidget::SubmitChoice(ECharacterTrialType SelectedTrial)
{
	if (bChoiceSubmitted || !ObjectiveDirector) return;
	bChoiceSubmitted = ObjectiveDirector->ResolveFirstTrialChoice(SelectedTrial);
}

void UTrialChoiceWidget::HandleSamuraiSelected() { SubmitChoice(ECharacterTrialType::Samurai); }
void UTrialChoiceWidget::HandleNinjaSelected() { SubmitChoice(ECharacterTrialType::Ninja); }

void UTrialChoiceWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
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
		ChoiceInk[Index]->SetRenderScale(FVector2D(FMath::Lerp(0.84f, 1.0f, Reveal), 1.0f));
		const FLinearColor Highlight = Style->InkBrushTexture ? FLinearColor(0.01f, 0.01f, 0.01f) : Style->SecondaryHeadingColor;
		ChoiceLabels[Index]->SetColorAndOpacity(FMath::Lerp(FLinearColor(0.72f, 0.73f, 0.75f), Highlight, Reveal));
	}
}

FReply UTrialChoiceWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!InMouseEvent.GetCursorDelta().IsNearlyZero()) bShowFocusHighlight = false;
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UTrialChoiceWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	bShowFocusHighlight = true;
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
