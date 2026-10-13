#pragma once
#include "BloodshiftMenuStyle.h"

/** Vector lock and crossed chains stay crisp on cards and compact choices. */
class SMenuLockVisual final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMenuLockVisual) : _Chains(true) {}
        SLATE_ARGUMENT(bool, Chains)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) { bChains=Args._Chains; }
    virtual FVector2D ComputeDesiredSize(float) const override { return {28,28}; }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
    {
        const auto Size=G.GetLocalSize();const float Unit=float(FMath::Min(Size.X,Size.Y));
        const FVector2D Center=Size*.5;
        BloodshiftMenu::FOrnamentPainter P{Out,G,Layer,1};
        if(bChains)for(float S:{-1.f,1.f})for(int32 I=-3;I<=3;++I)
        {
            const FVector2D C=Center+FVector2D(I*Unit*.18,I*S*Unit*.18);
            const FVector2D Along(Unit*.09,S*Unit*.09),Across(-S*Unit*.035,Unit*.035);
            P.Line({C-Along,C+Across,C+Along,C-Across,C-Along},BloodshiftMenu::Muted,1.5f);
        }
        const float R=Unit*.14f;
        P.Arc(Center+FVector2D(0,-R*.65f),R,PI,2*PI,BloodshiftMenu::Gold,2.f);
        const auto A=Center+FVector2D(-R*1.4f,0),B=Center+FVector2D(R*1.4f,R*1.75f);
        P.Line({A,{B.X,A.Y},B,{A.X,B.Y},A},BloodshiftMenu::Gold,2.f);
        P.Line({Center+FVector2D(0,R*.45),Center+FVector2D(0,R*1.15)},BloodshiftMenu::Gold,2.f);
        return Layer;
    }
private:
    bool bChains=true;
};

/** Asset-independent dropdown chevron for the shared menu theme. */
class SMenuDropdownArrow final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMenuDropdownArrow) {} SLATE_END_ARGS()
    void Construct(const FArguments&) {}
    virtual FVector2D ComputeDesiredSize(float) const override { return {18,12}; }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
    {
        const auto Size=G.GetLocalSize();
        BloodshiftMenu::FOrnamentPainter P{Out,G,Layer,1};
        P.Line({{2,2},{Size.X*.5,Size.Y-2},{Size.X-2,2}},BloodshiftMenu::Gold,2.f);
        return Layer;
    }
};
