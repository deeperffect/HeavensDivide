#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ComboRadialWidget.generated.h"

/** Small, resolution-independent run HUD badge. Fill begins at noon and advances clockwise. */
UCLASS()
class HEAVENSDIVIDE_API UComboRadialWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UComboRadialWidget(const FObjectInitializer& Initializer);
    void SetPresentation(class UTexture2D* InIcon, float InPercent, bool bInReady, bool bInEquipped,
        FText InKey, const FSlateFontInfo& InFont, FLinearColor InChargingColor, FLinearColor InReadyColor,
        float InIntensity, float InPulseSpeed);
    // The same ring vocabulary for dash charges and swap cooldowns.
    void SetActionPresentation(UTexture2D* InIcon, float Progress, bool bAvailable, bool bBlocked,
        FText Key, FText Status, const FSlateFontInfo& InFont, FLinearColor Charging, FLinearColor Ready,
        int32 Charges = -1, int32 MaxCharges = 0);
    const FText& GetStatusLabel() const { return StatusLabel; }
    int32 GetAvailableCharges() const { return AvailableCharges; }
    float GetPercent() const { return Percent; }
    bool IsReady() const { return bReady; }
    UTexture2D* GetIcon() const { return Icon; }
    const FText& GetKeyLabel() const { return KeyLabel; }
    float GetReadyPulse(float RealSeconds) const;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Cull,
        FSlateWindowElementList& Draw, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
private:
    UPROPERTY(Transient) TObjectPtr<UTexture2D> Icon;
    bool bAction = false;
    bool bBlocked = false;
    FText StatusLabel;
    int32 AvailableCharges = -1;
    int32 ChargeCount = 0;
    FSlateFontInfo Font;
    FText KeyLabel;
    float Percent = 0.f;
    float Intensity = 1.f;
    float PulseSpeed = 1.1f;
    bool bReady = false;
    bool bEquipped = false;
    FLinearColor ChargingColor;
    FLinearColor ReadyColor;
};
