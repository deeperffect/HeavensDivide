#include "PlayerHUDWidget.h"
#include "SurvivorPlayerController.h"
#include "EnemyBase.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/World.h"
#include "Widgets/SWidget.h"

void ASurvivorPlayerController::ShowEnemyDamageNumber(AEnemyBase* Enemy, FVector Position, float ActualDamage, bool bCritical, bool bStatus)
{
    if (!bStatus && PlayerHUDWidget && IsLocalController() && IsRunInProgress())
        PlayerHUDWidget->ShowEnemyDamageNumber(Enemy, Position, ActualDamage, bCritical, bStatus);
}

void UPlayerHUDWidget::ShowEnemyDamageNumber(AActor* Enemy, FVector Position, float ActualDamage, bool bCritical, bool bStatus)
{
    if (!bShowDamageNumbers || bStatus) return;
    // Reject offscreen hits before they can consume the bounded visible-number budget.
    if (SurvivorPlayerController && SurvivorPlayerController->GetLocalPlayer())
    {
        FVector2D Screen;
        if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(SurvivorPlayerController, Position, Screen, true)) return;
        const FVector2D Size = UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(SurvivorPlayerController).GetLocalSize();
        if (Screen.X < -40 || Screen.Y < -40 || Screen.X > Size.X + 40 || Screen.Y > Size.Y + 40) return;
    }
    DamageNumbers.Add(Enemy, Position, ActualDamage, bCritical, bStatus);
    if (const auto Widget = GetCachedWidget()) Widget->Invalidate(EInvalidateWidgetReason::Paint);
}

void UPlayerHUDWidget::UpdateDamageNumbers(float DeltaSeconds)
{
    if (DamageNumbers.GetNumbers().IsEmpty()) return;
    if (!bShowDamageNumbers || (SurvivorPlayerController && !SurvivorPlayerController->IsRunInProgress())) DamageNumbers.Reset();
    else if ((!GetWorld() || !GetWorld()->IsPaused())
        && (!SurvivorPlayerController || !SurvivorPlayerController->IsSelectingUpgrade())) DamageNumbers.Advance(DeltaSeconds);
    if (const auto Widget = GetCachedWidget()) Widget->Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UPlayerHUDWidget::PaintDamageNumbers(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer, float Opacity) const
{
    if (!bShowDamageNumbers || !SurvivorPlayerController || !SurvivorPlayerController->IsRunInProgress() || SurvivorPlayerController->IsSelectingUpgrade()
        || (GetWorld() && GetWorld()->IsPaused()) || DamageNumbers.GetNumbers().IsEmpty()) return Layer;
    const FGeometry PlayerGeometry = UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(SurvivorPlayerController);
    const FVector2D Size = Geometry.GetLocalSize();
    // Draw crits last so ordinary hits cannot cover their pop.
    for (bool bCritical : {false, true})
        for (const auto& Number : DamageNumbers.GetNumbers())
        {
            if (Number.bCritical != bCritical) continue;
            FVector2D Screen;
            if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(SurvivorPlayerController, Number.WorldPosition, Screen, true)) continue;
            Screen = Geometry.AbsoluteToLocal(PlayerGeometry.LocalToAbsolute(Screen));
            if (Screen.X < -40 || Screen.Y < -40 || Screen.X > Size.X + 40 || Screen.Y > Size.Y + 100) continue;
            CombatNumbers::Draw(Number, Screen, Geometry, Out, Layer + (bCritical ? 3 : 1), Opacity);
        }
    return Layer + 4;
}
