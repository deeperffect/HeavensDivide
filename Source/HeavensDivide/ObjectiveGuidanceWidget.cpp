#include "ObjectiveGuidanceWidget.h"
#include "SurvivorPlayerController.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"

UObjectiveGuidanceWidget::UObjectiveGuidanceWidget(const FObjectInitializer& Init) : Super(Init)
{
    static ConstructorHelpers::FObjectFinder<UTexture2D> Texture(TEXT("/Game/Assets/VFX/MarkersPointersV1/Textures/Sprites/T_Arrow_01.T_Arrow_01"));
    Arrow.SetResourceObject(Texture.Object);Arrow.DrawAs=ESlateBrushDrawType::Image;Arrow.ImageSize=FVector2D(64);
    static ConstructorHelpers::FObjectFinder<UObject> Heading(TEXT("/Game/Assets/Fonts/Cinzel-Medium_Font.Cinzel-Medium_Font"));
    Font=FSlateFontInfo(Heading.Object,22,TEXT("Default"));
    SetVisibility(ESlateVisibility::HitTestInvisible);
    ForceVolatile(true); // Re-evaluate pause/intro visibility even while world ticking is stopped.
}

void UObjectiveGuidanceWidget::Present(const TArray<FObjectiveArrow>& InArrows,const FText& InPopup,float InPopupAge)
{
    Arrows=InArrows;Popup=InPopup;PopupAge=InPopupAge;InvalidateLayoutAndVolatility();
}

int32 UObjectiveGuidanceWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Cull,
    FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const
{
    const auto* PC=Cast<ASurvivorPlayerController>(GetOwningPlayer());
    if(PC && (PC->IsObjectiveIntroPlaying() || PC->IsSelectingUpgrade() || !PC->IsRunInProgress() || UGameplayStatics::IsGamePaused(PC))) return Layer;
    const float Scale=FMath::Clamp(float(G.GetLocalSize().Y/900),.6f,1.6f);
    for(const auto& Indicator:Arrows)
    {
        const FVector2D Center=Indicator.Origin+Indicator.Direction*90.f*Scale;
        const FVector2D Size(52.f*Scale);
        // T_Arrow_01 points downward in texture space.
        const float Angle=FMath::Atan2(Indicator.Direction.Y,Indicator.Direction.X)-UE_HALF_PI;
        FSlateDrawElement::MakeRotatedBox(Out,Layer,G.ToPaintGeometry(Size+FVector2D(8*Scale),FSlateLayoutTransform(Center-(Size+FVector2D(8*Scale))*.5)),
            &Arrow,ESlateDrawEffect::None,Angle,TOptional<FVector2f>(),FSlateDrawElement::RelativeToElement,FLinearColor(0,0,0,.8f));
        FSlateDrawElement::MakeRotatedBox(Out,Layer+1,G.ToPaintGeometry(Size,FSlateLayoutTransform(Center-Size*.5)),
            &Arrow,ESlateDrawEffect::None,Angle,TOptional<FVector2f>(),FSlateDrawElement::RelativeToElement,Indicator.Color);
    }
    Layer+=2;
    if(Popup.IsEmpty() || PopupAge>=PopupDuration) return Layer;
    const float Alpha=FMath::SmoothStep(0.f,.25f,PopupAge)*(1-FMath::SmoothStep(PopupDuration-.65f,PopupDuration,PopupAge));
    const FString Message=FText::Format(NSLOCTEXT("Objectives","SpawnPopup","Objective spawned: {0}"),Popup).ToString();
    const FVector2D Extent=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Message,Font);
    const float TextScale=FMath::Min(Scale,float(G.GetLocalSize().X*.85/FMath::Max(1.0,Extent.X)));
    const FVector2D Position((G.GetLocalSize().X-Extent.X*TextScale)*.5,G.GetLocalSize().Y*.19-8.f*(1-Alpha));
    const FGeometry Canvas=G.MakeChild(Extent,FSlateLayoutTransform(TextScale,Position));
    const FSlateBrush* White=FCoreStyle::Get().GetBrush("WhiteBrush");
    FSlateDrawElement::MakeBox(Out,Layer++,Canvas.ToPaintGeometry(Extent+FVector2D(44,24),FSlateLayoutTransform(FVector2D(-22,-12))),White,
        ESlateDrawEffect::None,FLinearColor(.008f,.012f,.018f,Alpha*.82f));
    FSlateDrawElement::MakeText(Out,Layer++,Canvas.ToPaintGeometry(),Message,Font,ESlateDrawEffect::None,FLinearColor(1,.88f,.59f,Alpha));
    return Layer;
}
