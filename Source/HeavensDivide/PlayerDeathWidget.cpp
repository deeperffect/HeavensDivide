#include "PlayerDeathWidget.h"
#include "PlayerDeathScene.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Font.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "UObject/ConstructorHelpers.h"
UPlayerDeathWidget::UPlayerDeathWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
    static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Game/Assets/Fonts/Cinzel-Medium_Font"));
    TitleFont = FSlateFontInfo(Font.Object, 62);
    SetIsFocusable(true);
    SetVisibility(ESlateVisibility::Visible); ForceVolatile(true);
}
TSharedRef<SWidget> UPlayerDeathWidget::RebuildWidget() { return SNew(SBox); }
void UPlayerDeathWidget::Present(APlayerDeathScene* InScene, float InTitleFadeSeconds)
{
    Scene = InScene;
    TitleFadeSeconds = FMath::Max(.01f, InTitleFadeSeconds);
    Elapsed = OutroAlpha = ResultsAlpha = 0.f;
    SetAllNavigationRules(EUINavigationRule::Stop, NAME_None);
}
void UPlayerDeathWidget::SetTransition(float FadeToBlack, float RevealResults)
{
    OutroAlpha = FMath::Clamp(FadeToBlack, 0.f, 1.f);
    ResultsAlpha = FMath::Clamp(RevealResults, 0.f, 1.f);
}
int32 UPlayerDeathWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull,
    FSlateWindowElementList& Draw, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
    Layer = Super::NativePaint(Args,Geo,Cull,Draw,Layer,Style,bEnabled);
    const FVector2D Size = Geo.GetLocalSize();
    const float Opacity = GetFadeOpacity() * (1.f - ResultsAlpha);
    const auto BlackBox = [&](FVector2D Position, FVector2D Extent) {
        if (Extent.X > 0 && Extent.Y > 0)
            FSlateDrawElement::MakeBox(Draw, ++Layer, Geo.ToPaintGeometry(Extent, FSlateLayoutTransform(Position)),
                FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0,0,0,Opacity));
    };
    if (Scene && Scene->GetTexture()) {
        const FBox2D Rect = Scene->GetViewRect();
        const FVector2D Min = Rect.Min * Size, Max = Rect.Max * Size;
        // The captured black background and red character fade together in one
        // layer. Paint only the letterbox margins separately to avoid doubling
        // the opacity over gameplay inside the capture rectangle.
        BlackBox(FVector2D::ZeroVector, FVector2D(Size.X, Min.Y));
        BlackBox(FVector2D(0, Max.Y), FVector2D(Size.X, Size.Y-Max.Y));
        BlackBox(FVector2D(0, Min.Y), FVector2D(Min.X, Max.Y-Min.Y));
        BlackBox(FVector2D(Max.X, Min.Y), FVector2D(Size.X-Max.X, Max.Y-Min.Y));
        FSlateBrush Brush; Brush.SetResourceObject(Scene->GetTexture()); Brush.DrawAs=ESlateBrushDrawType::Image;
        FSlateDrawElement::MakeBox(Draw, ++Layer, Geo.ToPaintGeometry(Max-Min, FSlateLayoutTransform(Min)),
            &Brush, ESlateDrawEffect::IgnoreTextureAlpha, FLinearColor(1.f-OutroAlpha,1.f-OutroAlpha,1.f-OutroAlpha,Opacity));
    } else BlackBox(FVector2D::ZeroVector, Size);
    const FText Title=NSLOCTEXT("Death","Title","YOU DIED");
    const FVector2D TextSize=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Title,TitleFont);
    const float Scale=FMath::Min(Size.Y/1080.f,Size.X*.8f/TextSize.X);
    const FVector2D Position((Size.X-TextSize.X*Scale)*.5f,Size.Y*.08f);
    FSlateDrawElement::MakeText(Draw,++Layer,Geo.ToPaintGeometry(TextSize,FSlateLayoutTransform(Scale,Position)),Title,TitleFont,ESlateDrawEffect::None,FLinearColor(.85f,.025f,.045f,GetTitleOpacity()));
    return Layer;
}

float UPlayerDeathWidget::GetTitleOpacity() const
{
    const float Alpha = FMath::Clamp((Elapsed - (Scene ? Scene->GetFadeDuration() : .28f)) / TitleFadeSeconds, 0.f, 1.f);
    return Alpha * Alpha * (3.f - 2.f * Alpha) * (1.f - OutroAlpha) * (1.f - ResultsAlpha);
}

float UPlayerDeathWidget::GetFadeOpacity() const
{
    const float Alpha = FMath::Clamp(Elapsed / (Scene ? Scene->GetFadeDuration() : .28f), 0.f, 1.f);
    return Alpha * Alpha * (3.f - 2.f * Alpha);
}
