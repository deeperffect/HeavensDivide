#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MenuFeedbackWidget.generated.h"

class SButton;
class UButton;
class USoundBase;

/** Shared interaction feedback for UMG and native Slate menus. Slate time keeps it alive while paused. */
UCLASS()
class HEAVENSDIVIDE_API UMenuFeedbackWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, Category="Menu|Feedback", meta=(ClampMin="1", ClampMax="1.2"))
    float HoverScale = 1.05f;
    UPROPERTY(EditDefaultsOnly, Category="Menu|Feedback", meta=(ClampMin="1", ClampMax="1.2"))
    float PressScale = 1.08f;
    UPROPERTY(EditDefaultsOnly, Category="Menu|Feedback", meta=(ClampMin="0.01", ClampMax="1"))
    float ResponseSeconds = 0.12f;
    UPROPERTY(EditDefaultsOnly, Category="Menu|Feedback")
    TSoftObjectPtr<USoundBase> HoverSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/HeavensDivide/Audio/Menu/MS_MenuHover.MS_MenuHover")));
    UPROPERTY(EditDefaultsOnly, Category="Menu|Feedback")
    TSoftObjectPtr<USoundBase> PressSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/HeavensDivide/Audio/Menu/MS_MenuPress.MS_MenuPress")));

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    void SetMenuFocusedButton(UButton* Button);
    void PressMenuFocusedButton();

private:
    friend class FMenuFeedbackTest;
    void DiscoverButtons();
    void UpdateFeedback(float DeltaTime);
    UFUNCTION() void HandleMenuPressed();
    struct FButtonFeedback
    {
        TWeakPtr<SButton> Button;
        TOptional<FSlateRenderTransform> OriginalTransform;
        FVector2D OriginalPivot = FVector2D::ZeroVector;
        float Scale = 1.f;
        float PressRemaining = 0.f;
        bool bWasHot = false;
        bool bInitialized = false;
        bool bSeenThisFrame = false;
    };
    TArray<FButtonFeedback> Buttons;
    TWeakPtr<SWidget> ManualFocus;
    bool bShowNavigationFocus = true;
    UPROPERTY(Transient) TObjectPtr<USoundBase> LoadedHoverSound;
    UPROPERTY(Transient) TObjectPtr<USoundBase> LoadedPressSound;
};
