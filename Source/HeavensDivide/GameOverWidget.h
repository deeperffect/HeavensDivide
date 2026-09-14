// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MenuPromptWidget.h"
#include "GameOverWidget.generated.h"

class ASurvivorPlayerController;
class UButton;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class HEAVENSDIVIDE_API UGameOverWidget : public UMenuPromptWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Game Over")
	void InitializeGameOver(ASurvivorPlayerController* InPlayerController, float FinalRunTimeSeconds);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildGameOverScreen();
	void NormalizeTimeForTravel();

	UFUNCTION()
	void HandleRestartRun();
	UFUNCTION()
	void HandleMainMenu();

	UPROPERTY(Transient)
	TObjectPtr<ASurvivorPlayerController> SurvivorPlayerController;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FinalRunTimeText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> RestartRunButton;
	bool bTravelRequested = false;
};
