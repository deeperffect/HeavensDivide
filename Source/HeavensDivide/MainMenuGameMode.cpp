// Copyright Epic Games, Inc. All Rights Reserved.

#include "MainMenuGameMode.h"

#include "UObject/ConstructorHelpers.h"
#include "MainMenuWidget.h"

AMainMenuGameMode::AMainMenuGameMode()
{
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	PlayerControllerClass = APlayerController::StaticClass();
	MainMenuWidgetClass = UMainMenuWidget::StaticClass();
	static ConstructorHelpers::FClassFinder<UMainMenuWidget> MainMenuWidgetBlueprint(
		TEXT("/Game/HeavensDivide/Blueprints/UI/WBP_MainMenu"));
	if (MainMenuWidgetBlueprint.Succeeded())
	{
		MainMenuWidgetClass = MainMenuWidgetBlueprint.Class;
	}
}

void AMainMenuGameMode::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PlayerController || !MainMenuWidgetClass)
	{
		return;
	}

	MainMenuWidget = CreateWidget<UMainMenuWidget>(PlayerController, MainMenuWidgetClass);
	if (!MainMenuWidget)
	{
		return;
	}

	MainMenuWidget->AddToViewport(100);
	PlayerController->bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
	MainMenuWidget->ShowMainPanel();
}
