#include "ObjectiveIntroWidget.h"

#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"

UObjectiveIntroWidget::UObjectiveIntroWidget(const FObjectInitializer& Init) : Super(Init)
{
    static ConstructorHelpers::FObjectFinder<UObject> TitleFont(TEXT("/Game/Assets/Fonts/Cinzel-Medium_Font.Cinzel-Medium_Font"));
    Font = FSlateFontInfo(TitleFont.Object, 34, TEXT("Default"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> SmokeTexture(TEXT("/Game/Assets/VFX/NiagaraPortalsVol1/Textures/RealisticSmoke02_6x6.RealisticSmoke02_6x6"));
    Smoke.SetResourceObject(SmokeTexture.Object);
    Smoke.DrawAs = ESlateBrushDrawType::Image;
    Smoke.ImageSize = FVector2D(256);
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UObjectiveIntroWidget::Present(const FText& Message, FLinearColor Color)
{
    Accent = Color;
    Elapsed = 0;
    LayoutMessage(Message);
    InvalidateLayoutAndVolatility();
}

void UObjectiveIntroWidget::LayoutMessage(const FText& Message)
{
    Letters.Reset();
    TextSize = FVector2D::ZeroVector;
    if (!FSlateApplication::IsInitialized()) return;
    const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    TArray<FString> Lines;
    Message.ToString().ParseIntoArrayLines(Lines, false);
    TArray<float> Widths;
    for (const FString& Line : Lines)
    {
        const float Width = Measure->Measure(Line, Font).X;
        Widths.Add(Width);
        TextSize.X = FMath::Max(TextSize.X, double(Width));
    }
    const float LineHeight = Measure->Measure(TEXT("Ag"), Font).Y + 14.f;
    TextSize.Y = Lines.Num() * LineHeight;
    for (int32 Row = 0; Row < Lines.Num(); ++Row)
    {
        const FString& Line = Lines[Row];
        for (int32 Index = 0; Index < Line.Len(); ++Index)
        {
            // Measuring the prefix retains the font's kerning and centers both lines together.
            const float X = (TextSize.X - Widths[Row]) * .5f + Measure->Measure(Line.Left(Index), Font).X;
            const float Width = Measure->Measure(Line.Mid(Index, 1), Font).X;
            Letters.Add({Line.Mid(Index, 1), FVector2D(X, Row * LineHeight),
                float((X + Width * .5f) / FMath::Max(1.0, TextSize.X))});
        }
    }
}

void UObjectiveIntroWidget::SetElapsed(float Seconds)
{
    Elapsed = FMath::Max(0.f, Seconds);
    InvalidateLayoutAndVolatility();
}

float UObjectiveIntroWidget::LetterOpacity(float Time, float X)
{
    const float Reveal = FMath::Clamp(Time / RevealSeconds, 0.f, 1.f) * 1.3f;
    const float Dissolve = FMath::Clamp((Time - RevealSeconds - HoldSeconds) / DissolveSeconds, 0.f, 1.f) * 1.3f;
    X = FMath::Clamp(X, 0.f, 1.f);
    return FMath::SmoothStep(X, X + .3f, Reveal) * (1.f - FMath::SmoothStep(X, X + .3f, Dissolve));
}

int32 UObjectiveIntroWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull,
    FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool Enabled) const
{
    if (Elapsed >= Duration || Letters.IsEmpty()) return Layer;
    const FVector2D View = G.GetLocalSize();
    const float Scale = FMath::Min(float(View.X / 1440), float(View.Y / 900));
    // Text also fits ultrawide/localized messages without clipping at either edge.
    const float TextScale = Scale * FMath::Min(1.f, float(1260.0 / FMath::Max(1.0, TextSize.X)));
    const FVector2D Origin = (View - TextSize * TextScale) * .5;
    const FGeometry Canvas = G.MakeChild(TextSize, FSlateLayoutTransform(TextScale, Origin));
    const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
    const float Envelope = FMath::SmoothStep(0.f, .4f, Elapsed) * (1.f - FMath::SmoothStep(Duration - .5f, Duration, Elapsed));
    FSlateDrawElement::MakeBox(Out, Layer++, G.ToPaintGeometry(), White, ESlateDrawEffect::None,
        FLinearColor(.008f, .012f, .018f, Envelope * .68f));

    for (const FLetter& Letter : Letters)
    {
        const float Alpha = LetterOpacity(Elapsed, Letter.Fraction);
        if (Alpha <= 0.f) continue;
        const FVector2D Position = Letter.Position + FVector2D(0, (1.f - Alpha) * 5.f);
        const FLinearColor Ink = FMath::Lerp(Accent, FLinearColor(1.f, .94f, .82f), Alpha);
        FSlateDrawElement::MakeText(Out, Layer, Canvas.ToPaintGeometry(FVector2D(60), FSlateLayoutTransform(Position + FVector2D(0, 3))),
            Letter.Text, Font, ESlateDrawEffect::None, FLinearColor(0, 0, 0, Alpha * .9f));
        FSlateDrawElement::MakeText(Out, Layer + 1, Canvas.ToPaintGeometry(FVector2D(60), FSlateLayoutTransform(Position)),
            Letter.Text, Font, ESlateDrawEffect::None, Ink.CopyWithNewOpacity(Alpha));
    }
    Layer += 2;

    if (Elapsed >= SkipDelaySeconds)
    {
        FSlateFontInfo HintFont = Font; HintFont.Size = 16;
        const FText Hint = NSLOCTEXT("Objectives", "SkipIntro", "SPACE  Skip");
        const FVector2D Size = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Hint, HintFont);
        FSlateDrawElement::MakeText(Out, Layer++, G.ToPaintGeometry(Size, FSlateLayoutTransform(Scale,
            FVector2D((View.X-Size.X*Scale)*.5, View.Y*.72))), Hint, HintFont, ESlateDrawEffect::None,
            FLinearColor(1.f,.94f,.82f,Envelope*.75f));
    }

    // Animated smoke atlas puffs follow the same horizontal front on entry AND exit.
    const bool bDissolving = Elapsed >= RevealSeconds + HoldSeconds;
    const float Sweep = bDissolving ? (Elapsed - RevealSeconds - HoldSeconds) / DissolveSeconds : Elapsed / RevealSeconds;
    if (Sweep >= 0.f && Sweep <= 1.f && Smoke.GetResourceObject())
    {
        for (int32 I = 0; I < 36; ++I)
        {
            const float Lag = (I % 6) * .035f;
            const float Flight = Sweep - Lag;
            if (Flight < 0.f) continue;
            const float Life = FMath::Frac(Flight * 1.4f + I * .137f);
            const int32 Frame = FMath::Clamp(FMath::FloorToInt(6.f + Life * 28.f), 0, 35);
            FSlateBrush Puff = Smoke;
            const FVector2d UV((Frame % 6) / 6.0, (Frame / 6) / 6.0);
            Puff.SetUVRegion(FBox2d(UV, UV + FVector2d(1.0 / 6.0)));
            const float Size = 100.f + (I % 5) * 22.f;
            const FVector2D Center(Flight * (TextSize.X + 170.f) - 85.f,
                TextSize.Y * .5f + FMath::Sin(I * 2.399f + Elapsed * 1.6f) * (TextSize.Y * .45f + 24.f) - Life * 30.f);
            const float Alpha = FMath::Sin(PI * FMath::Clamp(Sweep, 0.f, 1.f)) * .65f;
            const FLinearColor Color = FMath::Lerp(Accent, FLinearColor(.8f, .82f, .85f), .25f + (I % 3) * .25f);
            FSlateDrawElement::MakeRotatedBox(Out, Layer, Canvas.ToPaintGeometry(FVector2D(Size), FSlateLayoutTransform(Center - FVector2D(Size * .5f))),
                &Puff, ESlateDrawEffect::None, I * .73f + Elapsed * .12f, TOptional<FVector2f>(),
                FSlateDrawElement::RelativeToElement, Color.CopyWithNewOpacity(Alpha));
        }
    }
    return Layer;
}
