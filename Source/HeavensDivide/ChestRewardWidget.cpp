#include "ChestRewardWidget.h"
#include "EliteRewardChest.h"
#include "BloodshiftMenuStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "UObject/ConstructorHelpers.h"
#include "InputCoreTypes.h"

UChestRewardWidget::UChestRewardWidget(const FObjectInitializer& Init):Super(Init)
{
    SetIsFocusable(true);
    static ConstructorHelpers::FObjectFinder<UObject> Font(TEXT("/Game/Assets/Fonts/Cinzel-Medium_Font.Cinzel-Medium_Font"));
    Heading=FSlateFontInfo(Font.Object,28,TEXT("Default"));
}
void UChestRewardWidget::Present(AEliteRewardChest* Chest,int32 XP,float Elapsed,float Duration)
{ Reward=Chest;TotalXP=XP;Time=Elapsed;Length=FMath::Max(1.5f,Duration);InvalidateLayoutAndVolatility(); }
FReply UChestRewardWidget::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
    if(E.GetKey()==EKeys::SpaceBar || E.GetKey()==EKeys::Enter || E.GetKey()==EKeys::Escape || E.GetKey()==EKeys::Gamepad_FaceButton_Bottom)
    { if(Reward.IsValid())Reward->RequestSkipOpening();return FReply::Handled(); }
    return FReply::Handled();
}
FReply UChestRewardWidget::NativeOnMouseButtonDown(const FGeometry&,const FPointerEvent& E)
{ if(E.GetEffectingButton()==EKeys::LeftMouseButton && Reward.IsValid())Reward->RequestSkipOpening();return FReply::Handled(); }

int32 UChestRewardWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Cull,
    FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const
{
    using namespace BloodshiftMenu;
    const FVector2D View=G.GetLocalSize();
    const float S=FMath::Min(float(View.X/1280),float(View.Y/800));
    const FVector2D O=(View-FVector2D(1280,800)*S)*.5;
    const FGeometry Canvas=G.MakeChild(FVector2D(1280,800),FSlateLayoutTransform(S,O));
    const FSlateBrush* White=FCoreStyle::Get().GetBrush("WhiteBrush");
    auto Box=[&](FVector2D P,FVector2D Size,FLinearColor C)
    { FSlateDrawElement::MakeBox(Out,Layer,Canvas.ToPaintGeometry(Size,FSlateLayoutTransform(P)),White,ESlateDrawEffect::None,C); };
    auto Text=[&](const FString& T,FVector2D P,int32 Size,FLinearColor C,bool Title=false)
    {
        auto Font=Title?Heading:BodyFont(Size);Font.Size=Size;
        const FVector2D Extent=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T,Font);
        FSlateDrawElement::MakeText(Out,Layer,Canvas.ToPaintGeometry(Extent,FSlateLayoutTransform(P-FVector2D(Extent.X*.5,0))),T,Font,ESlateDrawEffect::None,C);
    };
    auto Outline=[&](FVector2D P,FVector2D Size,FLinearColor C,float Width=1)
    { FOrnamentPainter{Out,Canvas,Layer,1}.Line({P,P+FVector2D(Size.X,0),P+Size,P+FVector2D(0,Size.Y),P},C,Width); };
    FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(),White,ESlateDrawEffect::None,FLinearColor(.004f,.012f,.019f,.96f));
    ++Layer;
    const float T=Time*3.6f/Length,Open=FMath::SmoothStep(.55f,1.1f,T);
    const FVector2D Center(640,318);
    // Rotating gold rays and rising sparks replace the abrupt camera cut.
    FOrnamentPainter P{Out,Canvas,Layer,1};
    for(int32 I=0;I<28;++I)
    {
        const float A=I*2*PI/28+T*.09f;
        const FVector2D D(FMath::Cos(A),FMath::Sin(A));
        const float R=220+FMath::Sin(I*3.1f)*35;
        P.Line({Center+D*120,Center+D*R},FLinearColor(.88f,.53f,.14f,Open*.12f),I%3==0?8:3);
    }
    P.Arc(Center,198,0,2*PI,FLinearColor(.7f,.47f,.18f,.18f),1);
    P.Arc(Center,205,T*.2f,T*.2f+PI*.7f,FLinearColor(.8f,.56f,.24f,.5f),2);
    ++Layer;
    P.Layer=Layer;
    Text(TEXT("B L O O D S H I F T"),{640,74},14,Muted);
    Text(T<1.1f?TEXT("A SEALED TREASURE"):TEXT("TREASURE UNSEALED"),{640,108},34,Paper,true);
    P.Line({{410,167},{611,167}},Gold);P.Diamond({640,167},6,Gold);P.Line({{669,167},{870,167}},Gold);
    const float Shake=T<.6f?FMath::Sin(T*72)*T*9:0;
    const float Lift=Open*82;
    const FVector2D B(535+Shake,309),L(528+Shake,282-Lift);
    // Lacquered chest with brass bands, raised lid and the Bloodshift seal.
    Box(B,{210,104},FLinearColor(.028f,.043f,.048f));Outline(B,{210,104},Gold,3);
    Box(B+FVector2D(0,4),{210,12},FLinearColor(.37f,.20f,.055f));
    for(float X:{22.f,171.f}) { Box(B+FVector2D(X,0),{17,104},Gold);Box(B+FVector2D(X+5,8),{3,88},Paper*.65f); }
    Outline(B+FVector2D(8,24),{194,70},FLinearColor(.16f,.19f,.16f));
    Box(L,{224,31},FLinearColor(.05f,.065f,.067f));Outline(L,{224,31},Gold,3);
    Box(L+FVector2D(7,5),{210,6},Gold);
    if(Open>.05f)
    {
        P.Line({B+FVector2D(8,0),L+FVector2D(8,31)},Gold,3);P.Line({B+FVector2D(202,0),L+FVector2D(216,31)},Gold,3);
        Box(B+FVector2D(5,-6),{200,8},FLinearColor(1,.76f,.31f,Open));
    }
    Box(B+FVector2D(86,25),{38,44},Crimson);P.Diamond(B+FVector2D(105,47),12,Paper,2);
    for(int32 I=0;I<20;++I)
    {
        const float Flight=FMath::Frac(T*.45f+I*.137f);
        const FVector2D Spark=Center+FVector2D(FMath::Sin(I*4.7f)*180*Flight,-Flight*160);
        P.Diamond(Spark,2+I%3,FLinearColor(1,.72f,.26f,Open*(1-Flight)),1.5f);
    }
    ++Layer;
    P.Layer=Layer;
    for(int32 I=0;I<3;++I)
    {
        const float X=460+I*180,Lock=1.75f+I*.23f;
        const bool Settled=T>=Lock;
        const int32 Share=TotalXP/3+(I<TotalXP%3?1:0);
        const FVector2D R(X-76,477),Size(152,126);
        Box(R,Size,Surface);Outline(R,Size,Settled?Gold:Rule,Settled?2:1);
        Out.PushClip(FSlateClippingZone(Canvas.MakeChild(Size,FSlateLayoutTransform(R))));
        const float Scroll=Settled?0:FMath::Fmod(FMath::Max(0.f,T-.75f)*700,86.f);
        for(int32 Row=-1;Row<=1;++Row)
        {
            const float Y=511+Row*86+Scroll;
            P.Diamond({X-43,Y+13},11,Settled?Paper:Gold,2);
            Text(FString::Printf(TEXT("%d"),Settled?Share:1+(int32(T*47)+Row*13+I*29+100)%199),{X+12,Y},29,Paper);
        }
        Out.PopClip();
        Box({X-74,575},{148,25},Surface);Text(TEXT("EXPERIENCE"),{X,580},10,Muted);
        if(Settled)P.Diamond({X,477},4,Paper);
    }
    ++Layer;
    const float Reveal=FMath::SmoothStep(2.25f,2.55f,T);
    Text(FString::Printf(TEXT("+%d XP"),FMath::RoundToInt(TotalXP*Reveal)),{640,627},38,FLinearColor(Paper.R,Paper.G,Paper.B,Reveal),true);
    Text(TEXT("+5 EMBERS  /  +1 SOUL SIGIL"),{640,679},15,FLinearColor(Gold.R,Gold.G,Gold.B,Reveal));
    Text(TEXT("CHALLENGE AND CURRENCY BONUSES APPLY"),{640,702},9,FLinearColor(Muted.R,Muted.G,Muted.B,Reveal));
    Text(T<2.45f?TEXT("SPACE / A / CLICK TO REVEAL"):TEXT("RELEASING EXPERIENCE"),{640,725},12,Muted);
    return Layer+1;
}
