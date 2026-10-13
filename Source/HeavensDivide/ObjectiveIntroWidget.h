#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ObjectiveIntroWidget.generated.h"

/** A real-time, left-to-right smoke reveal, readable hold, then smoke dissolve. */
UCLASS()
class HEAVENSDIVIDE_API UObjectiveIntroWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UObjectiveIntroWidget(const FObjectInitializer& Init);
    static constexpr float RevealSeconds = 1.2f;
    static constexpr float HoldSeconds = 2.0f;
    static constexpr float DissolveSeconds = 1.1f;
    static constexpr float Duration = RevealSeconds + HoldSeconds + DissolveSeconds;

    void Present(const FText& Message, FLinearColor Color);
    void SetElapsed(float Seconds);
    static float LetterOpacity(float Elapsed, float HorizontalFraction);
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Cull,
        FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool Enabled) const override;
private:
    struct FLetter { FString Text; FVector2D Position; float Fraction = 0; };
    void LayoutMessage(const FText& Message);
    TArray<FLetter> Letters;
    FVector2D TextSize = FVector2D::ZeroVector;
    float Elapsed = 0;
    FLinearColor Accent = FLinearColor::White;
    UPROPERTY() FSlateFontInfo Font;
    UPROPERTY() FSlateBrush Smoke;
};
