#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ObjectiveGuidanceWidget.generated.h"

struct FObjectiveArrow
{
    FVector2D Origin, Direction;
    FLinearColor Color = FLinearColor::White;
};

UCLASS()
class HEAVENSDIVIDE_API UObjectiveGuidanceWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UObjectiveGuidanceWidget(const FObjectInitializer& Init);
    void Present(const TArray<FObjectiveArrow>& InArrows, const FText& InPopup, float InPopupAge);
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull,
        FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool Enabled) const override;
    static constexpr float PopupDuration = 3.f;
private:
    TArray<FObjectiveArrow> Arrows;
    FText Popup;
    float PopupAge = PopupDuration;
    UPROPERTY() FSlateBrush Arrow;
    UPROPERTY() FSlateFontInfo Font;
};
