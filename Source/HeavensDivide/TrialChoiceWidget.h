#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RunObjectiveDirector.h"
#include "TrialChoiceWidget.generated.h"

class ARunObjectiveDirector;
class UButton;

UCLASS()
class HEAVENSDIVIDE_API UTrialChoiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTrialChoiceWidget(const FObjectInitializer& ObjectInitializer);
	void InitializeTrialChoice(ARunObjectiveDirector* InObjectiveDirector);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void BuildChoiceScreen();
	UButton* AddTrialButton(class UHorizontalBox* Parent, const FText& Label, FName WidgetName);
	void SubmitChoice(ECharacterTrialType SelectedTrial);

	UFUNCTION() void HandleSamuraiSelected();
	UFUNCTION() void HandleNinjaSelected();

	UPROPERTY(Transient) TObjectPtr<ARunObjectiveDirector> ObjectiveDirector;
	UPROPERTY(Transient) TObjectPtr<UButton> SamuraiButton;
	// Retain the Blueprint class so its shared menu art/fonts are included in packaged builds.
	UPROPERTY() TSubclassOf<class UMainMenuWidget> MenuStyleClass;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> ChoiceButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<class UImage>> ChoiceInk;
	UPROPERTY(Transient) TArray<TObjectPtr<class UTextBlock>> ChoiceLabels;
	TArray<float> InkReveal;
	bool bShowFocusHighlight = false;
	bool bChoiceSubmitted = false;
};
