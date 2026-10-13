// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameOverWidget.h"
#include "RunReportSubsystem.h"
#include "SynergyMetaProgressionSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Button.h"
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
    auto* Stack = BuildRunPanel(TEXT("RunOverTitle"), FText::FromString(TEXT("RUN OVER")), true);
    if (!Stack) return;
    FinalRunTimeText = AddPromptText(PromptActions, TEXT("FinalRunTimeText"), FText::FromString(TEXT("00:00")), false, 8);
    auto* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
    PromptActions->AddChildToVerticalBox(Actions);
    RestartRunButton = AddPromptButton(Actions, FText::FromString(TEXT("RESTART RUN")), TEXT("RestartRunButton"));
    auto* MenuButton = AddPromptButton(Actions, FText::FromString(TEXT("MAIN MENU")), TEXT("MainMenuButton"));
    RestartRunButton->OnClicked.AddDynamic(this, &UGameOverWidget::HandleRestartRun);
    MenuButton->OnClicked.AddDynamic(this, &UGameOverWidget::HandleMainMenu);
}

void UGameOverWidget::InitializeGameOver(ASurvivorPlayerController* InPlayerController, float FinalRunTimeSeconds)
{
	SurvivorPlayerController = InPlayerController;
	if (const auto* Report = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunReportSubsystem>() : nullptr)
		if (Report->Summary().RunId.IsValid()) FinalRunTimeSeconds = static_cast<float>(Report->Summary().Seconds);
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
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Lvl_B1_Lvl1"));
}

void UGameOverWidget::HandleMainMenu()
{
	if (bTravelRequested) return;
	bTravelRequested = true;
	NormalizeTimeForTravel();
	UGameplayStatics::OpenLevel(this, TEXT("Lvl_MainMenu"));
}
