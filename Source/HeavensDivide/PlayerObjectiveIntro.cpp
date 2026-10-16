#include "SurvivorPlayerController.h"
#include "ObjectiveIntroWidget.h"
#include "Containers/Ticker.h"
#include "Kismet/GameplayStatics.h"
#include "InputKeyEventArgs.h"

bool ASurvivorPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    if (IsObjectiveIntroPlaying() && Params.Key == EKeys::SpaceBar)
    {
        if (Params.Event == IE_Pressed) SkipObjectiveIntro();
        return true; // Do not feed this press into dash or other gameplay actions.
    }
    return Super::InputKey(Params);
}

bool ASurvivorPlayerController::ShowObjectiveIntro(AActor* Objective, const FText& Message, FLinearColor Accent, FSimpleDelegate OnFinished)
{
    if (!IsValid(Objective) || GetActiveObjective() != Objective || IsObjectiveIntroPlaying()
        || !IsRunInProgress() || IsPlayerDead() || IsSelectingUpgrade()
        || UGameplayStatics::IsGamePaused(this) || UGameplayStatics::GetGlobalTimeDilation(this) < .01f) return false;

    // Headless worlds can still run objective gameplay without a presentation.
    if (!IsLocalController() || !GetLocalPlayer()) return false;
    ObjectiveIntroWidget = CreateWidget<UObjectiveIntroWidget>(this);
    if (!ObjectiveIntroWidget) return false;
    if (!SetPause(true)) { ObjectiveIntroWidget = nullptr; return false; }

    ObjectiveIntroOwner = Objective;
    ObjectiveIntroFinished = MoveTemp(OnFinished);
    ObjectiveIntroElapsed = 0.f;
    ObjectiveIntroWidget->Present(Message, Accent);
    ObjectiveIntroWidget->AddToViewport(250);
    // Core ticker runs in real time even while world timers, physics and AI are paused.
    ObjectiveIntroTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float Delta)
    {
        UpdateObjectiveIntro(Delta);
        return IsObjectiveIntroPlaying();
    }));
    return true;
}

void ASurvivorPlayerController::UpdateObjectiveIntro(float RealDelta)
{
    if (!IsObjectiveIntroPlaying()) return;
    if (!ObjectiveIntroOwner.IsValid() || ObjectiveIntroOwner->IsActorBeingDestroyed()
        || GetActiveObjective() != ObjectiveIntroOwner.Get() || !IsRunInProgress() || IsPlayerDead())
    {
        CancelObjectiveIntro();
        return;
    }
    ObjectiveIntroElapsed += FMath::Max(0.f, RealDelta);
    ObjectiveIntroWidget->SetElapsed(ObjectiveIntroElapsed);
    if (ObjectiveIntroElapsed >= UObjectiveIntroWidget::Duration)
    {
        FSimpleDelegate Finished = MoveTemp(ObjectiveIntroFinished);
        CancelObjectiveIntro();
        Finished.ExecuteIfBound();
    }
}

void ASurvivorPlayerController::CancelObjectiveIntro()
{
    FTSTicker::GetCoreTicker().RemoveTicker(ObjectiveIntroTicker);
    ObjectiveIntroTicker.Reset();
    ObjectiveIntroFinished.Unbind();
    ObjectiveIntroOwner.Reset();
    if (ObjectiveIntroWidget)
    {
        ObjectiveIntroWidget->RemoveFromParent();
        ObjectiveIntroWidget = nullptr;
        SetPause(false);
    }
}

void ASurvivorPlayerController::SkipObjectiveIntro()
{
    if (!IsObjectiveIntroPlaying() || ObjectiveIntroElapsed < UObjectiveIntroWidget::SkipDelaySeconds) return;
    // Use the normal completion path, including owner/run validation and the
    // single completion delegate that starts the objective's gameplay timer.
    UpdateObjectiveIntro(UObjectiveIntroWidget::Duration);
}
