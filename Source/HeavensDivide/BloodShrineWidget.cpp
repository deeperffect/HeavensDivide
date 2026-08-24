// Copyright Epic Games, Inc. All Rights Reserved.

#include "BloodShrineWidget.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UBloodShrineWidget::RebuildWidget()
{
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.0f, 70.0f, 0.0f, 0.0f))
		[
			SNew(SBorder)
			.Padding(FMargin(20.0f, 12.0f))
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SAssignNew(HeaderText, STextBlock).Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f)
				[
					SAssignNew(ProgressText, STextBlock)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SAssignNew(TimeText, STextBlock)
				]
			]
		];
}

void UBloodShrineWidget::ShowInteractionPrompt()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetLines(FText::FromString(TEXT("BLOOD SHRINE")), FText::FromString(TEXT("Press E to activate")), FText::GetEmpty());
}

void UBloodShrineWidget::ShowChallenge(int32 CurrentBlood, int32 RequiredBlood, float TimeRemaining)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetLines(
		FText::FromString(TEXT("BLOOD SHRINE")),
		FText::FromString(FString::Printf(TEXT("Blood: %d / %d"), CurrentBlood, RequiredBlood)),
		FText::FromString(FString::Printf(TEXT("Time: %.1f"), FMath::Max(0.0f, TimeRemaining))));
}

void UBloodShrineWidget::ShowResult(bool bSuccess)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetLines(FText::FromString(bSuccess ? TEXT("BLOOD SHRINE COMPLETE") : TEXT("BLOOD SHRINE FAILED")), FText::GetEmpty(), FText::GetEmpty());
}

void UBloodShrineWidget::HideStatus()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBloodShrineWidget::SetLines(const FText& Header, const FText& Progress, const FText& Time)
{
	if (HeaderText) HeaderText->SetText(Header);
	if (ProgressText) ProgressText->SetText(Progress);
	if (TimeText) TimeText->SetText(Time);
}
