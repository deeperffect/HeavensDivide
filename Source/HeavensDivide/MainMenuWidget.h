// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UUniformGridPanel;
class UWidgetSwitcher;
class UUpgradeDefinition;

UCLASS(BlueprintType, Blueprintable)
class HEAVENSDIVIDE_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Main Menu")
	void ShowMainPanel();

	UFUNCTION(BlueprintCallable, Category = "Main Menu")
	void ShowCollectionPanel();

	UFUNCTION(BlueprintCallable, Category = "Main Menu")
	void ShowSettingsPanel();

	UFUNCTION(BlueprintCallable, Category = "Main Menu")
	void ShowResetConfirmation();

	UFUNCTION(BlueprintCallable, Category = "Main Menu|Collection")
	void RefreshCollection();

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void BuildMenu();
	class UVerticalBox* BuildCollectionPanel();
	void AddSynergyCollectionCard(UUpgradeDefinition* Definition, bool bUnlocked, int32 CardIndex);
	UButton* AddMenuButton(class UVerticalBox* Parent, const FText& Label, FName WidgetName);
	void SetResetConfirmationVisible(bool bVisible);

	UFUNCTION()
	void HandleNewRun();
	UFUNCTION()
	void HandleCollection();
	UFUNCTION()
	void HandleSettings();
	UFUNCTION()
	void HandleResetProgress();
	UFUNCTION()
	void HandleExitGame();
	UFUNCTION()
	void HandleBack();
	UFUNCTION()
	void HandleCancelReset();
	UFUNCTION()
	void HandleConfirmReset();

	UPROPERTY(Transient)
	TObjectPtr<UWidgetSwitcher> MenuSwitcher;
	UPROPERTY(Transient)
	TObjectPtr<class UBorder> ResetConfirmationOverlay;
	UPROPERTY(Transient)
	TObjectPtr<UButton> NewRunButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CollectionDiscoveryCountText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CollectionProgressText;
	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> SynergyCollectionGrid;

	bool bResetConfirmationOpen = false;
};
