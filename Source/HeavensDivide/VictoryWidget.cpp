#include "VictoryWidget.h"
#include "SynergyMetaProgressionSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "RunTravelSubsystem.h"

void UVictoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildVictoryScreen();
}

void UVictoryWidget::BuildVictoryScreen()
{
    auto* Stack = BuildRunPanel(TEXT("VictoryTitle"), FText::FromString(TEXT("VICTORY")), true);
    if (!Stack) return;
    auto* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
    PromptActions->AddChildToVerticalBox(Actions);
    NewRunButton = AddPromptButton(Actions, FText::FromString(TEXT("NEW RUN")), TEXT("NewRunButton"));
    auto* MenuButton = AddPromptButton(Actions, FText::FromString(TEXT("MAIN MENU")), TEXT("MainMenuButton"));
    NewRunButton->OnClicked.AddDynamic(this, &UVictoryWidget::HandleNewRun);
    MenuButton->OnClicked.AddDynamic(this, &UVictoryWidget::HandleMainMenu);
}

void UVictoryWidget::FocusInitialButton()
{
	if (NewRunButton) NewRunButton->SetKeyboardFocus();
}

void UVictoryWidget::PrepareForTravel()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URunTravelSubsystem* TravelState = GameInstance->GetSubsystem<URunTravelSubsystem>()) TravelState->ClearSnapshot();
	}
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGamePaused(World, false);
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
	}
}

void UVictoryWidget::HandleNewRun()
{
	if (bTravelRequested) return;
	bTravelRequested = true;
	PrepareForTravel();
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Lvl_B1_Lvl1"));
}

void UVictoryWidget::HandleMainMenu()
{
	if (bTravelRequested) return;
	bTravelRequested = true;
	PrepareForTravel();
	UGameplayStatics::OpenLevel(this, TEXT("Lvl_MainMenu"));
}
