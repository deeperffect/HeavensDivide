#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerDeathWidget.generated.h"
UCLASS()
class HEAVENSDIVIDE_API UPlayerDeathWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPlayerDeathWidget(const FObjectInitializer& Initializer);
    void Present(class APlayerDeathScene* InScene, float InTitleFadeSeconds = 1.2f);
    void SetElapsed(float Seconds) { Elapsed = Seconds; }
    void SetTransition(float FadeToBlack, float RevealResults);
    float GetFadeOpacity() const;
    float GetTitleOpacity() const;
protected:
    virtual FReply NativeOnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override { return FReply::Handled(); }
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Cull,
        FSlateWindowElementList& Draw, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
private:
    UPROPERTY() TObjectPtr<class APlayerDeathScene> Scene;
    UPROPERTY() FSlateFontInfo TitleFont;
    float Elapsed = 0.f;
    float TitleFadeSeconds = 1.2f;
    float OutroAlpha = 0.f;
    float ResultsAlpha = 0.f;
};
