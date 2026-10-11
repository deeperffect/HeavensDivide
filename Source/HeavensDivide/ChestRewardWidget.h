#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ChestRewardWidget.generated.h"
class AEliteRewardChest;

UCLASS()
class HEAVENSDIVIDE_API UChestRewardWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UChestRewardWidget(const FObjectInitializer& Init);
    void Present(AEliteRewardChest* Chest,int32 XP,float Elapsed,float Duration);
    virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Cull,
        FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event) override;
private:
    TWeakObjectPtr<AEliteRewardChest> Reward;
    int32 TotalXP=0;
    float Time=0,Length=3.6f;
    UPROPERTY() FSlateFontInfo Heading;
};
