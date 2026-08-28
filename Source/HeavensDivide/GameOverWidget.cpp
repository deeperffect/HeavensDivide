// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameOverWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerHUDWidget.h"
#include "SurvivorPlayerController.h"

void UGameOverWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildGameOverScreen();
}

void UGameOverWidget::BuildGameOverScreen()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("GameOverRoot"));
	WidgetTree->RootWidget = Root;
	UBorder* ScreenFade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GameOverScreenFade"));
	ScreenFade->SetBrushColor(FLinearColor(0.005f, 0.008f, 0.014f, 0.86f));
	Root->AddChildToOverlay(ScreenFade);
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GameOverPanel"));
	Panel->SetBrushColor(FLinearColor(0.045f, 0.025f, 0.035f, 0.98f));
	Panel->SetPadding(FMargin(64.0f, 42.0f));
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Center);
	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GameOverStack"));
	Panel->SetContent(Stack);

	auto AddText = [this, Stack](FName Name, const FText& Text, int32 Size, const FLinearColor& Color, float BottomPadding)
	{
		UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Block->SetText(Text);
		Block->SetJustification(ETextJustify::Center);
		Block->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Stack->AddChildToVerticalBox(Block)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
		return Block;
	};
	AddText(TEXT("RunOverTitle"), FText::FromString(TEXT("RUN OVER")), 48, FLinearColor(0.90f, 0.22f, 0.20f), 30.0f);
	AddText(TEXT("TimeSurvivedLabel"), FText::FromString(TEXT("TIME SURVIVED")), 18, FLinearColor(0.72f, 0.73f, 0.78f), 7.0f);
	FinalRunTimeText = AddText(TEXT("FinalRunTimeText"), FText::FromString(TEXT("00:00")), 38, FLinearColor::White, 34.0f);
	RestartRunButton = AddActionButton(Stack, FText::FromString(TEXT("RESTART RUN")), TEXT("RestartRunButton"));
	UButton* MainMenuButton = AddActionButton(Stack, FText::FromString(TEXT("MAIN MENU")), TEXT("MainMenuButton"));
	RestartRunButton->OnClicked.AddDynamic(this, &UGameOverWidget::HandleRestartRun);
	MainMenuButton->OnClicked.AddDynamic(this, &UGameOverWidget::HandleMainMenu);
}

UButton* UGameOverWidget::AddActionButton(UVerticalBox* Parent, const FText& Label, FName WidgetName)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);
	Button->SetBackgroundColor(FLinearColor(0.13f, 0.10f, 0.15f, 1.0f));
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(Label);
	Text->SetJustification(ETextJustify::Center);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 22;
	Text->SetFont(Font);
	Button->AddChild(Text);
	Parent->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 5.0f));
	return Button;
}

void UGameOverWidget::InitializeGameOver(ASurvivorPlayerController* InPlayerController, float FinalRunTimeSeconds)
{
	SurvivorPlayerController = InPlayerController;
	if (FinalRunTimeText) FinalRunTimeText->SetText(UPlayerHUDWidget::FormatRunTimeText(FinalRunTimeSeconds));
	if (RestartRunButton) RestartRunButton->SetKeyboardFocus();
}

void UGameOverWidget::NormalizeTimeForTravel()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGamePaused(World, false);
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
	}
}

void UGameOverWidget::HandleRestartRun()
{
	if (bTravelRequested) return;
	bTravelRequested = true;
	NormalizeTimeForTravel();
	UGameplayStatics::OpenLevel(this, TEXT("Lvl_B1_Lvl1"));
}

void UGameOverWidget::HandleMainMenu()
{
	if (bTravelRequested) return;
	bTravelRequested = true;
	NormalizeTimeForTravel();
	UGameplayStatics::OpenLevel(this, TEXT("Lvl_MainMenu"));
}
