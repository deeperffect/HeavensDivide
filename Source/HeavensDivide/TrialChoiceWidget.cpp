#include "TrialChoiceWidget.h"

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
	Root->AddChildToOverlay(ScreenFade);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TrialChoicePanel"));
	Panel->SetBrushColor(FLinearColor(0.035f, 0.025f, 0.045f, 0.98f));
	Panel->SetPadding(FMargin(54.0f, 40.0f));
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TrialChoiceStack"));
	Panel->SetContent(Stack);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TrialChoiceTitle"));
	Title->SetText(FText::FromString(TEXT("CHOOSE YOUR TRIAL")));
	Title->SetJustification(ETextJustify::Center);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.82f, 0.48f)));
	FSlateFontInfo TitleFont = Title->GetFont();
	TitleFont.Size = 38;
	Title->SetFont(TitleFont);
	Stack->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 28.0f));

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
	Button->SetBackgroundColor(FLinearColor(0.13f, 0.10f, 0.17f, 1.0f));
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(Label);
	Text->SetJustification(ETextJustify::Center);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 24;
	Text->SetFont(Font);
	Button->AddChild(Text);
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
