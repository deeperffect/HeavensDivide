// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameOverWidget.h"
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
    auto* Stack = BuildPromptPanel(TEXT("RunOverTitle"), FText::FromString(TEXT("RUN OVER")), FVector2D(900, 600));
    if (!Stack) return;
    AddPromptText(Stack, TEXT("TimeSurvivedLabel"), FText::FromString(TEXT("TIME SURVIVED")), false, 7);
    FinalRunTimeText = AddPromptText(Stack, TEXT("FinalRunTimeText"), FText::FromString(TEXT("00:00")), true, 24);
    if (auto* Meta = GetGameInstance() ? GetGameInstance()->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr)
        AddPromptText(Stack, TEXT("SoulEmberReward"), FText::FromString(FString::Printf(TEXT("+%d SOUL EMBERS%s"),
            Meta->GetLastSkillRunReward(), Meta->HasPendingSkillReward() ? TEXT(" (save pending)") : TEXT(""))), true, 24);
    auto* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
    Stack->AddChildToVerticalBox(Actions);
    RestartRunButton = AddPromptButton(Actions, FText::FromString(TEXT("RESTART RUN")), TEXT("RestartRunButton"));
    auto* MenuButton = AddPromptButton(Actions, FText::FromString(TEXT("MAIN MENU")), TEXT("MainMenuButton"));
    RestartRunButton->OnClicked.AddDynamic(this, &UGameOverWidget::HandleRestartRun);
    MenuButton->OnClicked.AddDynamic(this, &UGameOverWidget::HandleMainMenu);
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
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Lvl_B1_Lvl1"));
}

void UGameOverWidget::HandleMainMenu()
{
	if (bTravelRequested) return;
	bTravelRequested = true;
	NormalizeTimeForTravel();
	UGameplayStatics::OpenLevel(this, TEXT("Lvl_MainMenu"));
}
