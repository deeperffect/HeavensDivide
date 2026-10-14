#include "ComboRadialWidget.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/Layout/SBox.h"

UComboRadialWidget::UComboRadialWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
    SetVisibility(ESlateVisibility::HitTestInvisible);
    ForceVolatile(true); // Readiness pulses with real time, including cinematic time dilation.
}

TSharedRef<SWidget> UComboRadialWidget::RebuildWidget()
{
    return SNew(SBox).WidthOverride(136).HeightOverride(150);
}

void UComboRadialWidget::SetPresentation(UTexture2D* InIcon, float InPercent, bool bInReady, bool bInEquipped,
    FText InKey, const FSlateFontInfo& InFont, FLinearColor InChargingColor, FLinearColor InReadyColor,
    float InIntensity, float InPulseSpeed)
{
    Icon = InIcon;
    bAction = false; bBlocked = false;
    Percent = bInEquipped ? FMath::Clamp(InPercent, 0.f, 1.f) : 0.f;
    bEquipped = bInEquipped;
    bReady = bEquipped && bInReady && Percent >= 1.f;
    KeyLabel = MoveTemp(InKey);
    Font = InFont;
    ChargingColor = InChargingColor;
    ReadyColor = InReadyColor;
    Intensity = FMath::Clamp(InIntensity, 0.f, 2.f);
    PulseSpeed = FMath::Max(0.f, InPulseSpeed);
}

void UComboRadialWidget::SetActionPresentation(UTexture2D* InIcon, float Progress, bool bAvailable, bool InBlocked,
    FText Key, FText Status, const FSlateFontInfo& InFont, FLinearColor Charging, FLinearColor Ready,
    int32 Charges, int32 MaxCharges)
{
    SetPresentation(InIcon, Progress, false, true, Key, InFont, Charging, Ready, 0.f, 0.f);
    bAction = true; bBlocked = InBlocked; bReady = bAvailable && !bBlocked;
    StatusLabel = bBlocked ? NSLOCTEXT("ActionHUD", "Locked", "LOCKED") : Status;
    AvailableCharges = Charges; ChargeCount = MaxCharges;
}

float UComboRadialWidget::GetReadyPulse(float RealSeconds) const
{
    return bReady ? Intensity * (.5f + .5f * FMath::Sin(RealSeconds * PulseSpeed * 2.f * PI)) : 0.f;
}

int32 UComboRadialWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull,
    FSlateWindowElementList& Draw, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
    Layer = Super::NativePaint(Args, Geo, Cull, Draw, Layer, Style, bEnabled);
    const FVector2D Size = Geo.GetLocalSize();
    const float Scale = FMath::Max(0.f, FMath::Min(Size.X / 136.f, Size.Y / 150.f));
    if (Scale <= 0.f) return Layer;
    const FVector2D Origin((Size.X - 136.f * Scale) * .5f, (Size.Y - 150.f * Scale) * .5f);
    const FVector2D Center(68, 65);
    const FLinearColor Tint = Style.GetColorAndOpacityTint() * (bBlocked ? FLinearColor(.42f,.42f,.42f,1.f) : FLinearColor::White);
    const float Pulse = GetReadyPulse(GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f);
    const FLinearColor StateColor = bReady ? FMath::Lerp(ReadyColor, FLinearColor(1.f,.94f,.73f), .18f * Pulse) : ChargingColor;
    const auto Arc = [&](float Radius, float Fraction, FLinearColor Color, float Width)
    {
        if (Fraction <= 0.f) return;
        TArray<FVector2D> Points;
        const int32 Steps = FMath::Max(2, FMath::CeilToInt(128.f * Fraction));
        Points.Reserve(Steps + 1);
        for (int32 I = 0; I <= Steps; ++I)
        {
            const float Angle = -HALF_PI + 2.f * PI * Fraction * I / Steps;
            Points.Add(Origin + (Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius) * Scale);
        }
        // Continue past noon so anti-aliased end caps cannot split a completed ring.
        if (Fraction >= 1.f)
        {
            const FVector2D Continuation = Points[1];
            Points.Add(Continuation);
        }
        FSlateDrawElement::MakeLines(Draw, ++Layer, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color * Tint, true, Width * Scale);
    };
    const auto RoundedBox = [&](FVector2D Pos, FVector2D Extent, float Radius, FLinearColor Color)
    {
        const FSlateRoundedBoxBrush Brush(FLinearColor::White, Radius * Scale);
        FSlateDrawElement::MakeBox(Draw, ++Layer, Geo.ToPaintGeometry(Extent * Scale, FSlateLayoutTransform(Origin + Pos * Scale)), &Brush, ESlateDrawEffect::None, Color * Tint);
    };
    const auto Text = [&](const FText& Value, float Y, int32 FontSize, FLinearColor Color)
    {
        FSlateFontInfo Face = Font; Face.Size = FontSize;
        const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Value, Face);
        // Fit long rebound keys without distorting the badge or clipping the text.
        const float TextScale = Scale * FMath::Min(1.f, 112.f / FMath::Max(1.f, TextSize.X));
        const FVector2D Pos = Origin + FVector2D(68.f * Scale - TextSize.X * TextScale * .5f, Y * Scale);
        FSlateDrawElement::MakeText(Draw, ++Layer, Geo.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextScale, Pos)), Value, Face, ESlateDrawEffect::None, Color * Tint);
    };

    // Opaque backing keeps the icon legible against bright combat effects.
    RoundedBox(Center - FVector2D(58), FVector2D(116), 58, FLinearColor(.008f,.018f,.022f,.94f));
    if (bReady)
    {
        Arc(57, 1.f, ReadyColor.CopyWithNewOpacity(.035f + .025f * Pulse), 17.f);
        Arc(57, 1.f, ReadyColor.CopyWithNewOpacity(.10f + .07f * Pulse), 10.f);
    }
    Arc(61, 1.f, StateColor.CopyWithNewOpacity(bEquipped ? .36f : .16f), 1.f);
    Arc(55, 1.f, FLinearColor(.075f,.115f,.125f), 5.f);
    Arc(55, Percent, StateColor, 5.f);
    Arc(48, 1.f, StateColor.CopyWithNewOpacity(bReady ? .7f : .23f), 1.f);
    if (Icon)
    {
        const float Aspect = float(FMath::Max(1, Icon->GetSizeX())) / FMath::Max(1, Icon->GetSizeY());
        const FVector2D IconSize = Aspect >= 1.f ? FVector2D(76, 76 / Aspect) : FVector2D(76 * Aspect, 76);
        const FVector2D Pos = Center - IconSize * .5f - FVector2D(0, 7);
        FSlateRoundedBoxBrush MaskedIcon(FLinearColor::White, FMath::Min(IconSize.X, IconSize.Y) * Scale * .5f);
        MaskedIcon.SetResourceObject(Icon);
        FSlateDrawElement::MakeBox(Draw, ++Layer, Geo.ToPaintGeometry(IconSize * Scale, FSlateLayoutTransform(Origin + Pos * Scale)), &MaskedIcon,
            ESlateDrawEffect::None, FLinearColor::White.CopyWithNewOpacity(bEquipped ? 1.f : .3f) * Tint);
    }
    if (bAction && !Icon)
    {
        // A crisp dash glyph stays legible in the smaller badge at every resolution.
        for (float X : {49.f, 72.f})
        {
            TArray<FVector2D> Points;
            for (FVector2D P : {FVector2D(X,39), FVector2D(X+17,57), FVector2D(X,75)}) Points.Add(Origin+P*Scale);
            FSlateDrawElement::MakeLines(Draw, ++Layer, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None, StateColor*Tint, true, 4.f*Scale);
        }
    }
    if (bAction && ChargeCount > 0)
    {
        const int32 Count = FMath::Min(ChargeCount, 12);
        const float Step = FMath::Min(12.f, 78.f / Count);
        for (int32 I=0; I<Count; ++I)
            RoundedBox(FVector2D(68.f+(I-(Count-1)*.5f)*Step-2.5f,108), FVector2D(5,3), 1.f,
                I<AvailableCharges ? ReadyColor : FLinearColor(.15f,.2f,.22f));
    }
    Text(bAction ? StatusLabel : bEquipped ? (bReady ? NSLOCTEXT("Combo", "RadialReady", "READY") : FText::FromString(FString::Printf(TEXT("%d%%"), FMath::Min(99, FMath::FloorToInt(Percent * 100.f)))))
        : NSLOCTEXT("Combo", "RadialEmpty", "--"), 88, 11, bEquipped ? StateColor : FLinearColor(.4f,.48f,.49f));
    FSlateFontInfo KeyFont = Font; KeyFont.Size = 13;
    const float KeyWidth = FMath::Clamp(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(KeyLabel, KeyFont).X + 16.f, 42.f, 128.f);
    RoundedBox(FVector2D(68 - KeyWidth * .5f,123), FVector2D(KeyWidth,25), 4, StateColor.CopyWithNewOpacity(bReady ? .9f : .5f));
    RoundedBox(FVector2D(69 - KeyWidth * .5f,124), FVector2D(KeyWidth - 2,23), 3, FLinearColor(.009f,.022f,.027f));
    Text(KeyLabel, 126, 13, bEquipped ? StateColor : FLinearColor(.4f,.48f,.49f));
    return Layer;
}
