#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VictoryWidget.generated.h"

class UButton;
class UVerticalBox;

UCLASS(BlueprintType, Blueprintable)
class HEAVENSDIVIDE_API UVictoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void FocusInitialButton();

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildVictoryScreen();
	UButton* AddActionButton(UVerticalBox* Parent, const FText& Label, FName WidgetName);
	void PrepareForTravel();

	UFUNCTION() void HandleNewRun();
	UFUNCTION() void HandleMainMenu();

	UPROPERTY(Transient) TObjectPtr<UButton> NewRunButton;
	bool bTravelRequested = false;
};
