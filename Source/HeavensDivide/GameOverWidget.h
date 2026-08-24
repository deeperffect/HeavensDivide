// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameOverWidget.generated.h"

class ASurvivorPlayerController;
class UButton;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class HEAVENSDIVIDE_API UGameOverWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Game Over")
	void InitializeGameOver(ASurvivorPlayerController* InPlayerController, float FinalRunTimeSeconds);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildGameOverScreen();
	UButton* AddActionButton(class UVerticalBox* Parent, const FText& Label, FName WidgetName);
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
