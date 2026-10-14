#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "NinjaCharacter.h"
#include "PlayerDeathScene.h"
#include "PlayerDeathWidget.h"
#include "GameOverWidget.h"
#include "JointComboActor.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Camera/PlayerCameraManager.h"
#include "SceneView.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

bool ASurvivorPlayerController::BeginDeathPresentation()
{
    if (!bShowDeathPresentation || !IsLocalController() || !GetLocalPlayer() || !CharacterManager) return false;
    FMinimalViewInfo View = PlayerCameraManager ? PlayerCameraManager->GetCameraCacheView() : FMinimalViewInfo();
    if (PlayerCameraManager) View.FOV = PlayerCameraManager->GetFOVAngle();
    GetPlayerViewPoint(View.Location, View.Rotation);
    FSceneViewProjectionData Projection;
    auto* ViewportClient = GetLocalPlayer()->ViewportClient.Get();
    if (!ViewportClient || !GetLocalPlayer()->GetProjectionData(ViewportClient->Viewport, Projection)) {
        // Headless validation has no game viewport. Live play always uses the
        // local player's final projection rather than reconstructing its FOV.
        Projection.SetViewRectangle(FIntRect(0, 0, 1280, 720));
        Projection.ViewOrigin = View.Location;
        Projection.ViewRotationMatrix = FInverseRotationMatrix(View.Rotation) * FMatrix(
            FPlane(0,0,1,0), FPlane(1,0,0,0), FPlane(0,1,0,0), FPlane(0,0,0,1));
        FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(View,
            GetLocalPlayer()->AspectRatioAxisConstraint, Projection.GetViewRect(), Projection);
    }
    // A defeated run cannot keep a cinematic camera or time-dilation lease alive.
    for (TActorIterator<AJointComboActor> I(GetWorld()); I; ++I) I->Destroy();
    auto* ActiveHero = CharacterManager->GetActiveCharacter();
    const FLinearColor Color = ActiveHero && ActiveHero->IsA<ANinjaCharacter>() ? NinjaDeathSilhouetteColor : DeathSilhouetteColor;
    DeathScene = GetWorld()->SpawnActor<APlayerDeathScene>();
    if (!DeathScene || !DeathScene->Initialize(ActiveHero,Color,DeathAnimationPlayRate,View,Projection,DeathFadeSeconds)) {
        CancelDeathPresentation(); return false;
    }
    DeathPresentationWidget=CreateWidget<UPlayerDeathWidget>(this);
    if (!DeathPresentationWidget) { CancelDeathPresentation(); return false; }
    if (!UGameplayStatics::IsGamePaused(this)) {
        bDeathOwnsPause = SetPause(true);
        if (!bDeathOwnsPause) { CancelDeathPresentation(); return false; }
    }
    DeathPresentationElapsed=0.f;
    DeathPresentationWidget->Present(DeathScene, DeathTitleFadeSeconds);
    DeathPresentationWidget->AddToViewport(2000);
    bShowMouseCursor=false; bEnableClickEvents=false; bEnableMouseOverEvents=false;
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(DeathPresentationWidget->TakeWidget());
    SetInputMode(InputMode);
    DeathPresentationWidget->SetKeyboardFocus();
    DeathPresentationTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float Delta) {
        UpdateDeathPresentation(Delta); return DeathPresentationWidget != nullptr;
    }));
    return true;
}
void ASurvivorPlayerController::UpdateDeathPresentation(float RealDelta)
{
    if (!DeathPresentationWidget || !DeathScene) return;
    DeathPresentationElapsed+=FMath::Max(0.f,RealDelta);
    DeathScene->SetElapsed(DeathPresentationElapsed);
    DeathPresentationWidget->SetElapsed(DeathPresentationElapsed);
    // Let the montage finish and give even a short montage a fully readable title.
    const float HoldEnd = FMath::Max(DeathScene->GetDuration(),
        DeathScene->GetFadeDuration() + FMath::Max(.01f, DeathTitleFadeSeconds) + .8f);
    const float OutroSeconds = FMath::Max(.01f, DeathOutroFadeSeconds);
    const float OutroElapsed = DeathPresentationElapsed - HoldEnd;
    const auto Ease = [](float Alpha) {
        Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
        return Alpha * Alpha * (3.f - 2.f * Alpha);
    };
    const float ResultsElapsed = OutroElapsed - OutroSeconds;
    const float Reveal = Ease(ResultsElapsed / FMath::Max(.01f, DeathResultsFadeSeconds));
    DeathPresentationWidget->SetTransition(Ease(OutroElapsed / OutroSeconds), Reveal);
    if (ResultsElapsed < 0.f) return;
    if (!bDeathRevealingResults) {
        bDeathRevealingResults = true;
        PresentGameOver(GetRunTimeSeconds());
    }
    // Keep the black overlay above results until they are fully revealed. This
    // avoids an exposed gameplay frame and keeps input locked through the fade.
    if (!GameOverWidget || Reveal >= 1.f) {
        CancelDeathPresentation(true);
        FocusGameOver();
    }
}
void ASurvivorPlayerController::CancelDeathPresentation(bool bKeepResults)
{
    FTSTicker::GetCoreTicker().RemoveTicker(DeathPresentationTicker); DeathPresentationTicker.Reset();
    if (bDeathRevealingResults && !bKeepResults) {
        if (GameOverWidget) { GameOverWidget->RemoveFromParent(); GameOverWidget=nullptr; }
        bGameOverPresented = false;
        UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
    }
    bDeathRevealingResults = false;
    if (DeathPresentationWidget) { DeathPresentationWidget->RemoveFromParent(); DeathPresentationWidget=nullptr; }
    if (DeathScene) { DeathScene->Destroy(); DeathScene=nullptr; }
    if (bDeathOwnsPause) { SetPause(false); bDeathOwnsPause=false; }
}

void ASurvivorPlayerController::UpdateCameraManager(float DeltaSeconds)
{
    if (DeathScene && PlayerCameraManager) {
        // Preserve the underlying gameplay view while the overlay fades in too.
        // Skipping modifiers avoids applying camera shakes to the frozen POV twice.
        PlayerCameraManager->SetCameraCachePOV(DeathScene->GetFrozenView());
        return;
    }
    Super::UpdateCameraManager(DeltaSeconds);
}
