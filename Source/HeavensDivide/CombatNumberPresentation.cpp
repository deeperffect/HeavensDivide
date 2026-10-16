#include "CombatNumberPresentation.h"
#include "GameFramework/Actor.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "Rendering/DrawElementTypes.h"
#include "Styling/CoreStyle.h"

namespace
{
    FString NumberText(double Amount, bool bCritical)
    {
        // Preserve meaningful fractional ticks instead of displaying zero damage.
        FString Text = Amount < 1.0 ? FString::Printf(TEXT("%.1f"), FMath::Max(.1, Amount))
            : FString::Printf(TEXT("%.0f"), Amount);
        if (bCritical) Text += TEXT("!");
        return Text;
    }

    const FSlateFontInfo& NumberFont(bool bCritical, bool bStatus)
    {
        static const auto Make = [](int32 Size)
        {
            FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", Size);
            Font.OutlineSettings.OutlineSize = 2;
            Font.OutlineSettings.OutlineColor = FLinearColor(.025f, .012f, .008f, 1.f);
            return Font;
        };
        static const FSlateFontInfo Normal = Make(20), Critical = Make(28), Status = Make(16);
        return bCritical ? Critical : bStatus ? Status : Normal;
    }
}

void FCombatNumberBuffer::Add(AActor* Target, FVector Position, float Amount, bool bCritical, bool bStatus)
{
    if (bStatus || !FMath::IsFinite(Amount) || Amount <= 0 || Position.ContainsNaN()) return;
    // A short merge window tames simultaneous pellet bursts. Never extend lifetime,
    // merge different enemies, or let a normal hit inherit critical styling.
    for (auto& Number : Numbers)
        if (Target && Number.Target == Target && Number.bCritical == bCritical && Number.bStatus == bStatus
            && Number.Age < (bStatus ? .10f : .035f))
        {
            Number.Amount += Amount;
            Number.Text = NumberText(Number.Amount, bCritical);
            return;
        }
    if (Numbers.Num() >= Capacity)
    {
        int32 Victim = Numbers.IndexOfByPredicate([](const FCombatNumber& N) { return N.bStatus; });
        if (Victim == INDEX_NONE && !bStatus)
            Victim = Numbers.IndexOfByPredicate([](const FCombatNumber& N) { return !N.bCritical; });
        if (Victim == INDEX_NONE)
        {
            if (!bCritical) return; // Preserve critical feedback under extreme swarm load.
            Victim = 0;
        }
        Numbers.RemoveAt(Victim, 1, EAllowShrinking::No);
    }
    if (Numbers.Max() < Capacity) Numbers.Reserve(Capacity);
    auto& Number = Numbers.AddDefaulted_GetRef();
    Number.Target = Target;
    Number.WorldPosition = Position;
    Number.Amount = Amount;
    Number.bCritical = bCritical;
    Number.bStatus = bStatus;
    // Deterministic staggering must not consume gameplay's random stream.
    Number.Side = (float((Sequence++ * 7) % 11) - 5.f) / 5.f;
    Number.Text = NumberText(Amount, bCritical);
}

void FCombatNumberBuffer::Advance(float Delta)
{
    if (!FMath::IsFinite(Delta) || Delta <= 0) return;
    for (auto& Number : Numbers) Number.Age += Delta;
    Numbers.RemoveAll([](const FCombatNumber& N) { return N.Age >= N.Lifetime(); });
}

FCombatNumberAnimation CombatNumbers::Animate(const FCombatNumber& Number)
{
    FCombatNumberAnimation Result;
    const float Age = FMath::Max(0.f, Number.Age), T = FMath::Clamp(Age / Number.Lifetime(), 0.f, 1.f);
    const float Pop = Number.bCritical ? 1.8f : 1.18f;
    if (Age < .055f) Result.Scale = FMath::Lerp(Number.bCritical ? 1.65f : .65f, Pop, Age / .055f);
    else if (Age < .16f) Result.Scale = FMath::Lerp(Pop, .96f, (Age - .055f) / .105f);
    else Result.Scale = FMath::Lerp(.96f, 1.f, FMath::Clamp((Age - .16f) / .06f, 0.f, 1.f));
    Result.Offset = FVector2D(Number.Side * (9.f + T * 20.f), -8.f - (Number.bCritical ? 66.f : 46.f) * FMath::Sqrt(T));
    if (Number.bCritical && Age < .13f)
        Result.Offset.X += FMath::Sin(Age * 105.f) * 3.f * (1.f - Age / .13f);
    Result.Opacity = 1.f - FMath::Square(FMath::Clamp((T - .52f) / .48f, 0.f, 1.f));
    Result.Burst = Number.bCritical ? FMath::Clamp(1.f - Age / .21f, 0.f, 1.f) : 0.f;
    return Result;
}

void CombatNumbers::Draw(const FCombatNumber& Number, FVector2D Anchor, const FGeometry& Geometry,
    FSlateWindowElementList& Out, int32 Layer, float Opacity)
{
    const auto Animation = Animate(Number);
    const auto& Font = NumberFont(Number.bCritical, Number.bStatus);
    const FVector2D Size = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Number.Text, Font);
    const FVector2D Center = Anchor + Animation.Offset;
    FLinearColor Color = Number.bCritical ? FLinearColor(1.f, .64f, .075f)
        : Number.bStatus ? FLinearColor(.72f, .83f, .69f) : FLinearColor(1.f, .96f, .84f);
    if (Number.bCritical) Color = FMath::Lerp(Color, FLinearColor(1.f, 1.f, .85f), Animation.Burst * Animation.Burst);
    Color.A = Animation.Opacity * Opacity;
    if (Animation.Burst > 0)
    {
        FLinearColor Spark(1.f, .72f, .14f, Animation.Burst * Opacity);
        const float Travel = 1.f - Animation.Burst;
        TArray<FVector2D> Points;
        Points.SetNumUninitialized(2);
        for (int32 I = 0; I < 6; ++I)
        {
            const float Angle = I * PI / 3.f + .22f;
            const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
            const float Radius = FMath::Max(23.f, Size.X * .54f) * Animation.Scale + 20.f * Travel;
            Points[0] = Center + Direction * Radius;
            Points[1] = Center + Direction * (Radius + 10.f * Animation.Burst);
            FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Spark, true, 2.f);
        }
    }
    const auto Paint = Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Center - Size * .5f),
        FSlateRenderTransform(Animation.Scale));
    FSlateDrawElement::MakeText(Out, Layer + 1, Paint, Number.Text, Font, ESlateDrawEffect::None, Color);
}
