#pragma once

#include "CoreMinimal.h"
#include "MenuPromptWidget.h"
#include "VictoryWidget.generated.h"

class UButton;
class UVerticalBox;

UCLASS(BlueprintType, Blueprintable)
class HEAVENSDIVIDE_API UVictoryWidget : public UMenuPromptWidget
{
	GENERATED_BODY()

public:
	void FocusInitialButton();

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildVictoryScreen();
	void PrepareForTravel();

	UFUNCTION() void HandleNewRun();
	UFUNCTION() void HandleMainMenu();

	UPROPERTY(Transient) TObjectPtr<UButton> NewRunButton;
	bool bTravelRequested = false;
};
