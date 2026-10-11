#include "CurrencyHUDWidget.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "BloodshiftMenuStyle.h"
#include "Engine/GameInstance.h"

void UCurrencyHUDWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    GainAge += DeltaTime;
    UntilPoll -= DeltaTime;
    if (UntilPoll > 0.f) return;
    UntilPoll = .2f;
    auto* GI = GetGameInstance();
    auto* Meta = GI ? GI->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
    if (!Meta) return;
    const int32 NewEmbers = Meta->GetRunEmbers(), NewSigils = Meta->GetRunSigils();
    if (bInitialized && (NewEmbers > Embers || NewSigils > Sigils))
    {
        if (GainAge > 2.5f) GainedEmbers = GainedSigils = 0;
        GainedEmbers += FMath::Max(0, NewEmbers - Embers);
        GainedSigils += FMath::Max(0, NewSigils - Sigils);
        GainAge = 0.f;
    }
    Embers = NewEmbers; Sigils = NewSigils; bInitialized = true;
    InvalidateLayoutAndVolatility();
}
int32 UCurrencyHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Cull,
    FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool Enabled) const
{
    Layer = Super::NativePaint(Args, Geometry, Cull, Out, Layer, Style, Enabled) + 1;
    using namespace BloodshiftMenu;
    const auto* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
    FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(), Brush, ESlateDrawEffect::None, FLinearColor(.012f,.021f,.026f,.86f));
    FOrnamentPainter P{Out, Geometry, ++Layer, 1};
    P.Line({{0,0},{250,0}}, Gold, 1);
    auto Text = [&](const FString& Value, FVector2D At, int32 Size, FLinearColor Color)
    { FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(FVector2D(246,24),FSlateLayoutTransform(At)),Value,BodyFont(Size),ESlateDrawEffect::None,Color); };
    Text(TEXT("RUN SPOILS"), {10,6}, 10, Muted);
    Text(FString::Printf(TEXT("%d EMBERS   /   %d SIGILS"),Embers,Sigils), {10,22}, 14, Paper);
    if (GainAge < 2.5f)
    {
        FString Gain;
        if (GainedEmbers > 0) Gain = FString::Printf(TEXT("+%d Embers"),GainedEmbers);
        if (GainedSigils > 0) Gain += (Gain.IsEmpty()?TEXT(""):TEXT("  /  ")) + FString::Printf(TEXT("+%d Sigils"),GainedSigils);
        Text(Gain, {10,43}, 12, FLinearColor(Gold.R,Gold.G,Gold.B,FMath::Clamp((2.5f-GainAge)*2,0.f,1.f)));
    }
    return Layer + 1;
}
