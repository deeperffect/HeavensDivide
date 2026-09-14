#include "SurvivorPlayerController.h"
#include "PauseMenuWidget.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "RunTravelSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"

void ASurvivorPlayerController::TogglePauseMenu()
{
    if (PauseMenu) { ResumePausedRun(); return; }
    if (!IsLocalController() || !IsRunInProgress() || IsPlayerDead() || bLevelUpTimeDilationApplied
        || bLevelUpSelectionActive || UGameplayStatics::IsGamePaused(this)) return;
    PauseMenu = CreateWidget<UPauseMenuWidget>(this);
    if (!PauseMenu) return;
    if (!SetPause(true)) { PauseMenu = nullptr; return; }
    if (auto* ActivePawn = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr)
    {
        ActivePawn->ConsumeMovementInputVector();
        ActivePawn->GetCharacterMovement()->StopMovementImmediately();
    }
    PauseMenu->AddToViewport(300);
    FInputModeUIOnly Input;
    Input.SetWidgetToFocus(PauseMenu->TakeWidget());
    Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Input);
    bShowMouseCursor = true;
    PauseMenu->FocusResume();
}

void ASurvivorPlayerController::ClosePauseMenu()
{
    if (!PauseMenu) return;
    PauseMenu->RemoveFromParent();
    PauseMenu = nullptr;
    SetPause(false);
}

void ASurvivorPlayerController::ResumePausedRun()
{
    if (!PauseMenu) return;
    ClosePauseMenu();
    ConfigureInputMode();
}

void ASurvivorPlayerController::ReturnToMenuFromPause()
{
    if (!PauseMenu) return;
    StopRunGameplay(false);
    if (auto* Travel = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunTravelSubsystem>() : nullptr)
        Travel->ClearSnapshot();
    UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
    UGameplayStatics::OpenLevel(this, TEXT("Lvl_MainMenu"));
}
