#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElementTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SLeafWidget.h"

// Shared, resolution-independent presentation for the UMG and Slate menu pages.
namespace BloodshiftMenu
{
inline const FLinearColor Paper(.94f, .86f, .69f);
inline const FLinearColor Muted(.48f, .60f, .61f);
inline const FLinearColor Gold(.68f, .46f, .20f);
inline const FLinearColor Crimson(.52f, .055f, .035f);
inline const FLinearColor Ink(.009f, .024f, .031f);
inline const FLinearColor Surface(.010f, .030f, .038f);
inline const FLinearColor Rule(.14f, .23f, .24f);

inline FSlateFontInfo BodyFont(int32 Size)
{
    return FCoreStyle::GetDefaultFontStyle("Regular", Size);
}

inline FSlateBrush Solid(const FLinearColor& Color)
{
    FSlateBrush Brush = *FCoreStyle::Get().GetBrush("WhiteBrush");
    Brush.TintColor = Color;
    return Brush;
}

inline FButtonStyle ActionStyle(bool bDanger = false)
{
    const FSlateBrush Normal = FSlateRoundedBoxBrush(bDanger ? FLinearColor(.27f, .028f, .022f) : Surface,
        2.0f, bDanger ? Crimson : FLinearColor(.29f, .28f, .18f), 1.0f);
    const FSlateBrush Hover = FSlateRoundedBoxBrush(bDanger ? Crimson : Gold, 2.0f, Paper, 1.0f);
    const FSlateBrush Disabled = FSlateRoundedBoxBrush(Ink, 2.0f, Rule, 1.0f);
    return FButtonStyle().SetNormal(Normal).SetHovered(Hover).SetPressed(Hover).SetDisabled(Disabled)
        .SetNormalForeground(Paper).SetHoveredForeground(bDanger ? Paper : Ink)
        .SetPressedForeground(bDanger ? Paper : Ink).SetDisabledForeground(Muted)
        .SetNormalPadding(FMargin(20, 12)).SetPressedPadding(FMargin(20, 12));
}

// The ornaments share one geometry vocabulary at every viewport size.
struct FOrnamentPainter
{
    FSlateWindowElementList& Out;
    const FGeometry& Geometry;
    int32 Layer;
    float Opacity;

    void Line(const TArray<FVector2D>& Points, FLinearColor Color, float Width = 1.0f) const
    {
        Color.A *= Opacity;
        FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Points,
            ESlateDrawEffect::None, Color, true, Width);
    }
    void Diamond(FVector2D Center, float Radius, FLinearColor Color, float Width = 1.0f) const
    {
        Line({Center + FVector2D(0, -Radius), Center + FVector2D(Radius, 0),
            Center + FVector2D(0, Radius), Center + FVector2D(-Radius, 0), Center + FVector2D(0, -Radius)}, Color, Width);
    }
    void Arc(FVector2D Center, float Radius, float Start, float End, FLinearColor Color, float Width = 1.0f) const
    {
        TArray<FVector2D> Points;
        const int32 Steps = FMath::Max(12, FMath::CeilToInt(FMath::Abs(End - Start) * Radius / 8.0f));
        Points.Reserve(Steps + 1);
        for (int32 I = 0; I <= Steps; ++I)
        {
            const float Angle = FMath::Lerp(Start, End, float(I) / Steps);
            Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
        }
        Line(Points, Color, Width);
    }
};

class SOrnamentDivider final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SOrnamentDivider) {} SLATE_END_ARGS()
    void Construct(const FArguments&) {}
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1, 1); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&,
        FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool) const override
    {
        const float W = G.GetLocalSize().X;
        if (W < 40) return Layer;
        const FOrnamentPainter P{Out, G, Layer, Style.GetColorAndOpacityTint().A};
        const float X = W * .5f;
        P.Line({{0, .5f}, {X - 16, .5f}}, Rule);
        P.Line({{X + 16, .5f}, {W, .5f}}, Rule);
        P.Line({{0, .5f}, {FMath::Min(64.0f, W * .15f), .5f}}, Gold);
        P.Line({{W - FMath::Min(64.0f, W * .15f), .5f}, {W, .5f}}, Gold);
        P.Diamond({X, .5f}, 4.0f, Gold);
        P.Line({{X - 12, .5f}, {X - 8, .5f}}, Gold);
        P.Line({{X + 8, .5f}, {X + 12, .5f}}, Gold);
        return Layer;
    }
};

class SInsetSurface final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SInsetSurface) : _Fill(true), _Compact(false) {}
        SLATE_ARGUMENT(bool, Fill)
        SLATE_ARGUMENT(bool, Compact)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) { bFill = Args._Fill; bCompact = Args._Compact; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&,
        FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool) const override
    {
        const FVector2D Size = G.GetLocalSize();
        const float Alpha = Style.GetColorAndOpacityTint().A;
        if (bFill)
        {
            FLinearColor Color = Surface; Color.A *= Alpha;
            FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color);
        }
        const FOrnamentPainter P{Out, G, Layer + 1, Alpha};
        const float Inset = bCompact ? 5.0f : 8.0f;
        const float Length = bCompact ? 15.0f : 24.0f;
        if (!bCompact)
        {
            P.Line({{1, 9}, {9, 1}, {Size.X - 9, 1}, {Size.X - 1, 9}, {Size.X - 1, Size.Y - 9},
                {Size.X - 9, Size.Y - 1}, {9, Size.Y - 1}, {1, Size.Y - 9}, {1, 9}}, Rule);
            P.Diamond({Size.X * .5f, 8}, 3, Gold.CopyWithNewOpacity(.65f));
        }
        for (int32 Corner = 0; Corner < 4; ++Corner)
        {
            auto At = [&](float X, float Y)
            {
                return FVector2D((Corner & 1) ? Size.X - X : X, (Corner & 2) ? Size.Y - Y : Y);
            };
            P.Line({At(Inset, Length), At(Inset, Inset), At(Length, Inset)}, Gold.CopyWithNewOpacity(.65f));
        }
        return Layer + 1;
    }
private:
    bool bFill = true;
    bool bCompact = false;
};

class SPanelSurface final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SPanelSurface) : _Danger(false) {}
        SLATE_ARGUMENT(bool, Danger)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) { bDanger = Args._Danger; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&,
        FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool) const override
    {
        const FVector2D Size = G.GetLocalSize();
        const float Alpha = Style.GetColorAndOpacityTint().A;
        auto Tint = [Alpha](FLinearColor C) { C.A *= Alpha; return C; };
        TArray<FSlateGradientStop> Stops;
        Stops.Emplace(FVector2D::ZeroVector, Tint(FLinearColor(.008f, .022f, .028f, 1.0f)));
        Stops.Emplace(FVector2D(0, Size.Y), Tint(FLinearColor(.004f, .011f, .016f, 1.0f)));
        FSlateDrawElement::MakeGradient(Out, Layer, G.ToPaintGeometry(), MoveTemp(Stops), Orient_Horizontal);
        const FOrnamentPainter P{Out, G, Layer + 1, Alpha};
        const FLinearColor Accent = bDanger ? Crimson : Gold;
        // A faint engraved soul seal gives open space character without competing with the text.
        const FVector2D Seal(Size.X - 152, Size.Y - 186);
        const float Radius = FMath::Min(126.0f, Size.Y * .21f);
        const FLinearColor Etching = Gold.CopyWithNewOpacity(.035f);
        P.Arc(Seal, Radius, 0, 2 * PI, Etching);
        P.Arc(Seal, Radius - 7, 0, 2 * PI, Etching);
        P.Arc(Seal + FVector2D(0, -Radius * .5f), Radius * .5f, -PI * .5f, PI * .5f, Etching);
        P.Arc(Seal + FVector2D(0, Radius * .5f), Radius * .5f, PI * .5f, PI * 1.5f, Etching);
        P.Diamond(Seal + FVector2D(0, -Radius * .5f), 9, Etching);
        P.Diamond(Seal + FVector2D(0, Radius * .5f), 9, Etching);

        // Layered clipped frame, with a quiet wave engraving confined to the bottom gutter.
        for (int32 I = 0; I < 2; ++I)
        {
            const float D = I == 0 ? 1.5f : 8.0f;
            const float Cut = I == 0 ? 17.0f : 13.0f;
            P.Line({{D, D + Cut}, {D + Cut, D}, {Size.X - D - Cut, D}, {Size.X - D, D + Cut},
                {Size.X - D, Size.Y - D - Cut}, {Size.X - D - Cut, Size.Y - D},
                {D + Cut, Size.Y - D}, {D, Size.Y - D - Cut}, {D, D + Cut}},
                I == 0 ? Accent.CopyWithNewOpacity(.75f) : Rule, I == 0 ? 1.5f : 1.0f);
        }
        for (float X = 94; X < Size.X - 80; X += 32)
        {
            for (float R : {6.0f, 11.0f, 16.0f})
                P.Arc({X, Size.Y - 12}, R, PI, 2 * PI, Gold.CopyWithNewOpacity(.10f));
        }
        for (int32 Corner = 0; Corner < 4; ++Corner)
        {
            auto At = [&](float X, float Y)
            {
                return FVector2D((Corner & 1) ? Size.X - X : X, (Corner & 2) ? Size.Y - Y : Y);
            };
            P.Line({At(17, 77), At(17, 30), At(30, 17), At(77, 17)}, Accent, 1.5f);
            P.Line({At(24, 57), At(24, 34), At(34, 24), At(57, 24)}, Accent.CopyWithNewOpacity(.55f));
            P.Line({At(10, 43), At(10, 24), At(24, 10), At(43, 10)}, Accent.CopyWithNewOpacity(.5f));
            P.Diamond(At(27, 27), 5, Accent);
            P.Diamond(At(17, 81), 2, Accent);
            P.Diamond(At(81, 17), 2, Accent);
        }
        const float Mid = Size.X * .5f;
        P.Line({{Mid - 70, 17}, {Mid - 23, 17}, {Mid - 17, 11}}, Accent.CopyWithNewOpacity(.6f));
        P.Line({{Mid + 70, 17}, {Mid + 23, 17}, {Mid + 17, 11}}, Accent.CopyWithNewOpacity(.6f));
        P.Diamond({Mid, 17}, 10, Accent);
        P.Diamond({Mid, 17}, 4, Paper.CopyWithNewOpacity(.75f));
        return Layer + 1;
    }
private:
    bool bDanger = false;
};
}
