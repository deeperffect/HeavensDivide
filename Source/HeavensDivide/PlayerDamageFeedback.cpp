#include "PlayerHUDWidget.h"
#include "HealthComponent.h"
#include "SurvivorPlayerController.h"
#include "Engine/World.h"

#include "Rendering/DrawElementTypes.h"
#include "Widgets/SWidget.h"

void UPlayerHUDWidget::HandlePlayerDamaged(float DamageAmount, float CurrentHealth)
{
    // OnDamaged is emitted only for actual health loss, including a lethal hit.
    if (!bEnableDamageVignette || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
        return;
    // Repeated hits refresh the flash instead of accumulating an opaque red screen.
    DamageVignetteRemaining = FMath::Max(0.01f, DamageVignetteDuration);
    if (const auto Widget = GetCachedWidget()) Widget->Invalidate(EInvalidateWidgetReason::Paint);
}

void UPlayerHUDWidget::UpdateDamageFeedback(float DeltaSeconds)
{
    const bool bWasVisible = DamageVignetteRemaining > 0 || LowHealthBlend > 0;
    const bool bAlive = PlayerHealth && !PlayerHealth->IsDead()
        && (!SurvivorPlayerController || SurvivorPlayerController->IsRunInProgress());
    const float Percent = bAlive ? PlayerHealth->GetHealthPercent() : 1.f;
    const float Target = bEnableLowHealthWarning && bAlive && Percent <= LowHealthThreshold ? 1.f : 0.f;
    bCriticalHealth = Target > 0 && Percent <= LowHealthThreshold * .5f;
    const bool bPaused = (GetWorld() && GetWorld()->IsPaused()) || (SurvivorPlayerController && SurvivorPlayerController->IsSelectingUpgrade());
    const float Delta = bPaused ? 0.f : FMath::Max(0.f, DeltaSeconds);
    if (!bAlive || !bEnableLowHealthWarning) LowHealthBlend = LowHealthPhase = 0.f;
    else
    {
        LowHealthBlend = FMath::FInterpConstantTo(LowHealthBlend, Target, Delta, Target > 0 ? 8.f : 5.f);
        if (LowHealthBlend > 0)
            LowHealthPhase = FMath::Fmod(LowHealthPhase + Delta / (bCriticalHealth ? .70f : 1.1f), 1.f);
        else LowHealthPhase = 0.f;
    }
    DamageVignetteRemaining = bEnableDamageVignette
        ? FMath::Max(0.0f, DamageVignetteRemaining - Delta) : 0.0f;
    if (bWasVisible || LowHealthBlend > 0)
        if (const auto Widget = GetCachedWidget()) Widget->Invalidate(EInvalidateWidgetReason::Paint);
}

float UPlayerHUDWidget::GetLowHealthFeedbackOpacity() const
{
    if (!bEnableLowHealthWarning || !PlayerHealth || PlayerHealth->IsDead()
        || (SurvivorPlayerController && (!SurvivorPlayerController->IsRunInProgress() || SurvivorPlayerController->IsSelectingUpgrade()))) return 0.f;
    const float Pulse = FMath::Square(.5f + .5f * FMath::Cos(LowHealthPhase * 2.f * PI));
    return LowHealthBlend * FMath::Clamp(LowHealthEdgeOpacity, 0.f, .4f)
        * (bCriticalHealth ? .45f + .55f * Pulse : .3f + .6f * Pulse);
}

float UPlayerHUDWidget::GetDamageFeedbackOpacity() const
{
    if (!bEnableDamageVignette || DamageVignetteRemaining <= 0.0f) return 0.0f;
    const float Progress = FMath::Clamp(DamageVignetteRemaining / FMath::Max(0.01f, DamageVignetteDuration), 0.0f, 1.0f);
    return FMath::Clamp(DamageVignetteOpacity, 0.0f, 1.0f) * Progress * Progress;
}

int32 UPlayerHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
    const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    int32 LastLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
        OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    const float ParentOpacity = InWidgetStyle.GetColorAndOpacityTint().A;
    LastLayer = PaintDamageNumbers(AllottedGeometry, OutDrawElements, LastLayer, ParentOpacity);
    const bool bPaused = (GetWorld() && GetWorld()->IsPaused()) || (SurvivorPlayerController && SurvivorPlayerController->IsSelectingUpgrade());
    // Blend by maximum: incoming hits never stack with the persistent pulse into an opaque screen.
    const float Opacity = FMath::Max(GetDamageFeedbackOpacity(), bPaused ? 0.f : GetLowHealthFeedbackOpacity()) * ParentOpacity;
    if (Opacity <= KINDA_SMALL_NUMBER) return LastLayer;

    FLinearColor Edge = DamageVignetteColor;
    Edge.A *= Opacity;
    FLinearColor Clear = Edge;
    Clear.A = 0.0f;
    FLinearColor Soft = Edge;
    Soft.A *= .58f;
    FLinearColor Faint = Edge;
    Faint.A *= .15f;
    const FVector2D Size = AllottedGeometry.GetLocalSize();
    const float Extent = FMath::Clamp(DamageVignetteEdgeSize, 0.01f, 0.45f);
    // Two edge gradients leave the center transparent at every aspect ratio.
    // Native painting adds no input-blocking widget and needs no texture or material.
    TArray<FSlateGradientStop> Horizontal;
    Horizontal.Emplace(FVector2D(0, 0), Edge);
    Horizontal.Emplace(FVector2D(Size.X * Extent * .4f, 0), Soft);
    Horizontal.Emplace(FVector2D(Size.X * Extent * .75f, 0), Faint);
    Horizontal.Emplace(FVector2D(Size.X * Extent, 0), Clear);
    Horizontal.Emplace(FVector2D(Size.X * (1 - Extent), 0), Clear);
    Horizontal.Emplace(FVector2D(Size.X * (1 - Extent * .75f), 0), Faint);
    Horizontal.Emplace(FVector2D(Size.X * (1 - Extent * .4f), 0), Soft);
    Horizontal.Emplace(FVector2D(Size.X, 0), Edge);
    TArray<FSlateGradientStop> Vertical;
    Vertical.Emplace(FVector2D(0, 0), Edge);
    Vertical.Emplace(FVector2D(0, Size.Y * Extent * .4f), Soft);
    Vertical.Emplace(FVector2D(0, Size.Y * Extent * .75f), Faint);
    Vertical.Emplace(FVector2D(0, Size.Y * Extent), Clear);
    Vertical.Emplace(FVector2D(0, Size.Y * (1 - Extent)), Clear);
    Vertical.Emplace(FVector2D(0, Size.Y * (1 - Extent * .75f)), Faint);
    Vertical.Emplace(FVector2D(0, Size.Y * (1 - Extent * .4f)), Soft);
    Vertical.Emplace(FVector2D(0, Size.Y), Edge);
    FSlateDrawElement::MakeGradient(OutDrawElements, LastLayer + 1, AllottedGeometry.ToPaintGeometry(),
        MoveTemp(Horizontal), Orient_Vertical);
    FSlateDrawElement::MakeGradient(OutDrawElements, LastLayer + 2, AllottedGeometry.ToPaintGeometry(),
        MoveTemp(Vertical), Orient_Horizontal);
    return LastLayer + 2;
}
