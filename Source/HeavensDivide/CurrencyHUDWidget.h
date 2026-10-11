#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CurrencyHUDWidget.generated.h"

/** Uses credited run totals, including rewards waiting for the batched save. */
UCLASS()
class HEAVENSDIVIDE_API UCurrencyHUDWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Cull,
        FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool Enabled) const override;
private:
    int32 Embers = 0, Sigils = 0, GainedEmbers = 0, GainedSigils = 0;
    float UntilPoll = 0.f, GainAge = 10.f;
    bool bInitialized = false;
};
