#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MenuPromptWidget.generated.h"

class UButton;
class UVerticalBox;
class UHorizontalBox;
class UTextBlock;

/** Shared menu presentation for in-run choices and run result screens. */
UCLASS(Abstract)
class HEAVENSDIVIDE_API UMenuPromptWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UMenuPromptWidget(const FObjectInitializer& ObjectInitializer);
protected:
    UVerticalBox* BuildPromptPanel(FName TitleName, const FText& Heading, FVector2D PanelSize = FVector2D(900, 500));
    UButton* AddPromptButton(UHorizontalBox* Parent, const FText& Label, FName Name);
    UTextBlock* AddPromptText(UVerticalBox* Parent, FName Name, const FText& Text, bool bEmphasis = false, float BottomPadding = 20);
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
private:
    UPROPERTY() TSubclassOf<class UMainMenuWidget> MenuStyleClass;
    UPROPERTY(Transient) TArray<TObjectPtr<UButton>> ChoiceButtons;
    UPROPERTY(Transient) TArray<TObjectPtr<class UImage>> ChoiceInk;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ChoiceLabels;
    TArray<float> InkReveal;
    bool bShowFocusHighlight = false;
};
