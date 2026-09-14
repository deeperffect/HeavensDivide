#include "PlayerHUDWidget.h"

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
    if (DamageVignetteRemaining <= 0.0f) return;
    DamageVignetteRemaining = bEnableDamageVignette
        ? FMath::Max(0.0f, DamageVignetteRemaining - FMath::Max(0.0f, DeltaSeconds)) : 0.0f;
    if (const auto Widget = GetCachedWidget()) Widget->Invalidate(EInvalidateWidgetReason::Paint);
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
    const int32 LastLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
        OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    const float Opacity = GetDamageFeedbackOpacity() * InWidgetStyle.GetColorAndOpacityTint().A;
    if (Opacity <= KINDA_SMALL_NUMBER) return LastLayer;

    FLinearColor Edge = DamageVignetteColor;
    Edge.A *= Opacity;
    FLinearColor Clear = Edge;
    Clear.A = 0.0f;
    const FVector2D Size = AllottedGeometry.GetLocalSize();
    const float Extent = FMath::Clamp(DamageVignetteEdgeSize, 0.01f, 0.45f);
    // Two edge gradients leave the center transparent at every aspect ratio.
    // Native painting adds no input-blocking widget and needs no texture or material.
    TArray<FSlateGradientStop> Horizontal;
    Horizontal.Emplace(FVector2D(0, 0), Edge);
    Horizontal.Emplace(FVector2D(Size.X * Extent, 0), Clear);
    Horizontal.Emplace(FVector2D(Size.X * (1 - Extent), 0), Clear);
    Horizontal.Emplace(FVector2D(Size.X, 0), Edge);
    TArray<FSlateGradientStop> Vertical;
    Vertical.Emplace(FVector2D(0, 0), Edge);
    Vertical.Emplace(FVector2D(0, Size.Y * Extent), Clear);
    Vertical.Emplace(FVector2D(0, Size.Y * (1 - Extent)), Clear);
    Vertical.Emplace(FVector2D(0, Size.Y), Edge);
    FSlateDrawElement::MakeGradient(OutDrawElements, LastLayer + 1, AllottedGeometry.ToPaintGeometry(),
        MoveTemp(Horizontal), Orient_Vertical);
    FSlateDrawElement::MakeGradient(OutDrawElements, LastLayer + 2, AllottedGeometry.ToPaintGeometry(),
        MoveTemp(Vertical), Orient_Horizontal);
    return LastLayer + 2;
}
