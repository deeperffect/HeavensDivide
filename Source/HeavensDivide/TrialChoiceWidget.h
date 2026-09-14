#pragma once

#include "CoreMinimal.h"
#include "MenuPromptWidget.h"
#include "RunObjectiveDirector.h"
#include "TrialChoiceWidget.generated.h"

class ARunObjectiveDirector;
class UButton;

UCLASS()
class HEAVENSDIVIDE_API UTrialChoiceWidget : public UMenuPromptWidget
{
	GENERATED_BODY()

public:
	void InitializeTrialChoice(ARunObjectiveDirector* InObjectiveDirector);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildChoiceScreen();
	void SubmitChoice(ECharacterTrialType SelectedTrial);

	UFUNCTION() void HandleSamuraiSelected();
	UFUNCTION() void HandleNinjaSelected();

	UPROPERTY(Transient) TObjectPtr<ARunObjectiveDirector> ObjectiveDirector;
	UPROPERTY(Transient) TObjectPtr<UButton> SamuraiButton;
	bool bChoiceSubmitted = false;
};
