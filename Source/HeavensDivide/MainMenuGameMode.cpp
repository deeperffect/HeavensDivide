// Copyright Epic Games, Inc. All Rights Reserved.

#include "MainMenuGameMode.h"

#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "MainMenuWidget.h"

AMainMenuGameMode::AMainMenuGameMode()
{
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	PlayerControllerClass = APlayerController::StaticClass();
	MainMenuWidgetClass = UMainMenuWidget::StaticClass();
	static ConstructorHelpers::FClassFinder<UMainMenuWidget> MainMenuWidgetBlueprint(
		TEXT("/Game/HeavensDivide/Blueprints/UI/MainMenu/WBP_MainMenu"));
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

	// The menu UI and cinematic background should use the complete window on wide
	// displays. A constrained camera component otherwise shrinks the scene viewport
	// to its authored aspect ratio and leaves uncovered black bars beside the UMG layer.
	if (PlayerController->PlayerCameraManager)
	{
		PlayerController->PlayerCameraManager->bDefaultConstrainAspectRatio = false;
	}
	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		TInlineComponentArray<UCameraComponent*> CameraComponents(*ActorIt);
		for (UCameraComponent* CameraComponent : CameraComponents)
		{
			if (CameraComponent)
			{
				CameraComponent->SetConstraintAspectRatio(false);
			}
		}
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
