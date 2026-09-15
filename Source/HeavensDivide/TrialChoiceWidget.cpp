#include "TrialChoiceWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"

void UTrialChoiceWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildChoiceScreen();
}

void UTrialChoiceWidget::BuildChoiceScreen()
{
    auto* Stack = BuildPromptPanel(TEXT("TrialChoiceTitle"), FText::FromString(TEXT("CHOOSE YOUR TRIAL")));
    if (!Stack) return;

	UHorizontalBox* Choices = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TrialChoices"));
	PromptActions->AddChildToVerticalBox(Choices);
	SamuraiButton = AddPromptButton(Choices, FText::FromString(TEXT("SAMURAI TRIAL")), TEXT("SamuraiTrialButton"));
	UButton* NinjaButton = AddPromptButton(Choices, FText::FromString(TEXT("NINJA TRIAL")), TEXT("NinjaTrialButton"));
	SamuraiButton->OnClicked.AddDynamic(this, &UTrialChoiceWidget::HandleSamuraiSelected);
	NinjaButton->OnClicked.AddDynamic(this, &UTrialChoiceWidget::HandleNinjaSelected);
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
