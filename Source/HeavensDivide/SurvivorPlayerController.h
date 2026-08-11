// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SurvivorPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UCharacterManagerComponent;
class UExperienceComponent;
class UHealthComponent;
class ULevelUpWidget;
class UPlayerUpgradeComponent;
class UPlayerHUDWidget;
class USharedPlayerStatsComponent;
class ACharacterBase;
class APlayerCameraRig;
struct FInputActionValue;

UCLASS()
class HEAVENSDIVIDE_API ASurvivorPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASurvivorPlayerController();

	virtual void PlayerTick(float DeltaTime) override;

	void SetCameraFollowTarget(ACharacterBase* NewFollowTarget);
	ACharacterBase* GetCameraFollowTarget() const;
	UCharacterManagerComponent* GetCharacterManager() const;

	UFUNCTION(BlueprintPure, Category = "Player")
	UHealthComponent* GetPlayerHealthComponent() const;

	UFUNCTION(BlueprintPure, Category = "Player")
	UExperienceComponent* GetExperienceComponent() const;

	UFUNCTION(BlueprintPure, Category = "Player")
	USharedPlayerStatsComponent* GetSharedPlayerStats() const;

	UFUNCTION(BlueprintPure, Category = "Player")
	UPlayerUpgradeComponent* GetPlayerUpgrades() const;

	UFUNCTION(BlueprintPure, Category = "Player")
	bool IsPlayerDead() const;

	UFUNCTION(BlueprintCallable, Category = "Player|Debug")
	void DebugGrantXP(int32 Amount);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SwapAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AssistAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Characters")
	TObjectPtr<UCharacterManagerComponent> CharacterManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
	TObjectPtr<UHealthComponent> PlayerHealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
	TObjectPtr<UExperienceComponent> ExperienceComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
	TObjectPtr<USharedPlayerStatsComponent> SharedPlayerStatsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
	TObjectPtr<UPlayerUpgradeComponent> PlayerUpgradeComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	TSubclassOf<APlayerCameraRig> PlayerCameraRigClass;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<APlayerCameraRig> PlayerCameraRig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UPlayerHUDWidget> PlayerHUDClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<ULevelUpWidget> LevelUpWidgetClass;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UPlayerHUDWidget> PlayerHUDWidget;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<ULevelUpWidget> LevelUpWidget;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player")
	bool bIsPlayerDead = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player|Level Up")
	int32 PendingLevelUpChoices = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player|Level Up")
	bool bLevelUpSelectionActive = false;

	void Move(const FInputActionValue& Value);
	void Swap(const FInputActionValue& Value);
	void ConfigureInputMode();
	void InitializePlayerCameraRig();
	void InitializePlayerHUD();
	UFUNCTION()
	void HandleCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter);
	UFUNCTION()
	void HandlePlayerDeath();
	UFUNCTION()
	void HandlePlayerLevelUp(int32 NewLevel);
	UFUNCTION()
	void HandleLevelUpSelectionCompleted();
	UFUNCTION()
	void HandleSharedPlayerStatsChanged();
	void StartNextLevelUpSelection();
	bool EnsureLevelUpWidget();
	void PauseForLevelUpSelection();
	void ResumeAfterLevelUpSelection();
	void CloseLevelUpWidget();
	void ApplySharedMoveSpeedToParty();
	void ApplySharedHealthStats();
	void ApplySharedPlayerStats();
	void UpdateMouseFacingTarget();
	bool GetMouseWorldPosition(FVector& OutWorldPosition) const;

	float BasePlayerMaxHealth = 0.0f;
};
