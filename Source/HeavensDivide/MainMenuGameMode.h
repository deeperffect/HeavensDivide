// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainMenuGameMode.generated.h"

class UMainMenuWidget;

UCLASS()
class HEAVENSDIVIDE_API AMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMainMenuGameMode();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Main Menu")
	TSubclassOf<UMainMenuWidget> MainMenuWidgetClass;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMainMenuWidget> MainMenuWidget;
};
