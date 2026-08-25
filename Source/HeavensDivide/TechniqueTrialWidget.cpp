#include "TechniqueTrialWidget.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UTechniqueTrialWidget::RebuildWidget()
{
	return SNew(SBox).WidthOverride(460.0f)
	[
		SNew(SBorder).Padding(FMargin(22.0f)).BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.025f, 0.88f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SAssignNew(Header, STextBlock).Text(FText::FromString(TEXT("SAMURAI TECHNIQUE TRIAL"))).ColorAndOpacity(FLinearColor(0.9f, 0.7f, 0.25f))]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 8)[SAssignNew(Stance, STextBlock)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SAssignNew(Instruction, STextBlock).Text(FText::FromString(TEXT("HOLD YOUR GROUND")))]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SAssignNew(Time, STextBlock)]
		]
	];
}

void UTechniqueTrialWidget::ShowComplete()
{
	if (Header) Header->SetText(FText::FromString(TEXT("TRIAL COMPLETE")));
	if (Stance) Stance->SetText(FText::GetEmpty());
	if (Instruction) Instruction->SetText(FText::GetEmpty());
	if (Time) Time->SetText(FText::GetEmpty());
}

void UTechniqueTrialWidget::ShowMemoryStatus(int32 RoundIndex, int32 RoundCount, const FText& PhaseText, int32 StepIndex, int32 StepCount)
{
	if (Header) Header->SetText(FText::FromString(TEXT("SAMURAI MEMORY TRIAL")));
	if (Stance) Stance->SetText(FText::FromString(FString::Printf(TEXT("ROUND %d / %d"), RoundIndex, RoundCount)));
	if (Instruction) Instruction->SetText(PhaseText);
	if (Time) Time->SetText(StepCount > 0 ? FText::FromString(FString::Printf(TEXT("%d / %d"), StepIndex, StepCount)) : FText::GetEmpty());
}
