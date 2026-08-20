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
class UInactiveCharacterAssistComponent;
class ULevelUpWidget;
class UPlayerUpgradeComponent;
class UPlayerHUDWidget;
class USharedPlayerStatsComponent;
class ACharacterBase;
class APlayerCameraRig;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDash);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDashChargesChanged, int32, CurrentCharges, int32, MaxCharges);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwapCooldownStarted, float, CooldownDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSwapCooldownFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDamageDodged, float, IncomingDamage);

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

	UFUNCTION(BlueprintPure, Category = "Player|Swap")
	bool CanSwap() const;

	UFUNCTION(BlueprintPure, Category = "Player|Swap")
	float GetSwapCooldownRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Player|Swap")
	float GetSwapCooldownDuration() const;

	UFUNCTION(BlueprintPure, Category = "Player|Swap")
	float GetSwapCooldownProgress() const;

	UFUNCTION(BlueprintCallable, Category = "Player|Swap")
	void ResetSwapCooldown();

	UFUNCTION(BlueprintCallable, Category = "Player|Dash")
	bool TryDash();

	UFUNCTION(BlueprintPure, Category = "Player|Dash")
	bool CanDash() const;

	UFUNCTION(BlueprintPure, Category = "Player|Dash")
	float GetDashCooldownRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Player|Dash")
	int32 GetCurrentDashCharges() const;

	UFUNCTION(BlueprintPure, Category = "Player|Dash")
	int32 GetMaxDashCharges() const;

	UFUNCTION(BlueprintPure, Category = "Player|Dash")
	bool HasDashCharge() const;

	UFUNCTION(BlueprintPure, Category = "Player|Dash")
	float GetDashRechargeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Player|Dash")
	float GetDashRechargeNormalized() const;

	UFUNCTION(BlueprintCallable, Category = "Player|Debug")
	void DebugGrantXP(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Player|Health")
	void ApplyDamageToPlayer(float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "Player|Health")
	float GetActiveDamageReduction() const;

	UFUNCTION(BlueprintPure, Category = "Player|Health")
	float GetActiveHPRegenPerSecond() const;

	UFUNCTION(BlueprintPure, Category = "Player|Health")
	float GetActiveDodgeChance() const;

	UPROPERTY(BlueprintAssignable, Category = "Player|Dash")
	FOnPlayerDash OnDashStarted;

	UPROPERTY(BlueprintAssignable, Category = "Player|Dash")
	FOnPlayerDash OnDashEnded;

	UPROPERTY(BlueprintAssignable, Category = "Player|Dash")
	FOnDashChargesChanged OnDashChargesChanged;

	UPROPERTY(BlueprintAssignable, Category = "Player|Dash")
	FOnPlayerDash OnDashRechargeStarted;

	UPROPERTY(BlueprintAssignable, Category = "Player|Dash")
	FOnPlayerDash OnDashRechargeCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Player|Swap")
	FOnSwapCooldownStarted OnSwapCooldownStarted;

	UPROPERTY(BlueprintAssignable, Category = "Player|Swap")
	FOnSwapCooldownFinished OnSwapCooldownFinished;

	UPROPERTY(BlueprintAssignable, Category = "Player|Health")
	FOnPlayerDamageDodged OnDamageDodged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SwapAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AssistAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> DashAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DashDistance = 550.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float DashDuration = 0.18f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float DashRechargeTime = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Swap", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SwapCooldown = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Health", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float HPRegenTickInterval = 0.5f;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Synergy")
	TObjectPtr<UInactiveCharacterAssistComponent> InactiveCharacterAssistComponent;

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

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Swap")
	bool bCanSwap = true;

	void Move(const FInputActionValue& Value);
	void StopMoveInput(const FInputActionValue& Value);
	void Swap(const FInputActionValue& Value);
	void Dash(const FInputActionValue& Value);
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
	void StartSwapCooldown();
	void HandleSwapCooldownFinished();
	void ApplyHandoffBuff(ACharacterBase* NewActiveCharacter);
	void RemoveHandoffBuff(ACharacterBase* BuffedCharacter);
	FTimerHandle* GetHandoffTimerHandleForCharacter(const ACharacterBase* TargetCharacter);
	void ApplySharedMoveSpeedToParty();
	void ApplySharedHealthStats();
	void ApplySharedPlayerStats();
	void ApplyDashChargeStats();
	void UpdateMouseFacingTarget();
	bool GetMouseWorldPosition(FVector& OutWorldPosition) const;
	FVector GetDashDirection(const ACharacterBase* ActiveCharacter) const;
	void HandleDashStep(float DeltaTime);
	void FinishDash();
	void ConsumeDashCharge();
	void RestoreDashCharge(int32 ChargeAmount, const TCHAR* RestoreReason);
	void StartDashRechargeIfNeeded();
	void StopDashRecharge();
	void HandleDashRechargeTimerElapsed();
	void BroadcastDashChargesChanged();
	void StartHPRegeneration();
	void StopHPRegeneration();
	void HandleHPRegenerationTimerElapsed();

	float BasePlayerMaxHealth = 0.0f;
	FTimerHandle SwapCooldownTimerHandle;
	FTimerHandle SamuraiHandoffTimerHandle;
	FTimerHandle NinjaHandoffTimerHandle;
	FTimerHandle DashRechargeTimerHandle;
	FTimerHandle HPRegenTimerHandle;
	FVector2D LastMovementInput = FVector2D::ZeroVector;
	FVector ActiveDashDirection = FVector::ZeroVector;
	float DashElapsedTime = 0.0f;
	int32 CurrentDashCharges = 1;
	int32 MaxDashCharges = 1;
	bool bIsDashing = false;
};
