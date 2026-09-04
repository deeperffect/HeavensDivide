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
	void InitializeTrialChoice(ARunObjectiveDirector* InObjectiveDirector);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildChoiceScreen();
	UButton* AddTrialButton(class UHorizontalBox* Parent, const FText& Label, FName WidgetName);
	void SubmitChoice(ECharacterTrialType SelectedTrial);

	UFUNCTION() void HandleSamuraiSelected();
	UFUNCTION() void HandleNinjaSelected();

	UPROPERTY(Transient) TObjectPtr<ARunObjectiveDirector> ObjectiveDirector;
	UPROPERTY(Transient) TObjectPtr<UButton> SamuraiButton;
	bool bChoiceSubmitted = false;
};
