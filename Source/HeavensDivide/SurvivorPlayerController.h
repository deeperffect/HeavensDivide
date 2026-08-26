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
class UGameOverWidget;
class ULevelUpWidget;
class UPlayerUpgradeComponent;
class UPlayerHUDWidget;
class USharedPlayerStatsComponent;
class ACharacterBase;
class APlayerCameraRig;
class ABloodShrine;
class AEnemySpawner;
class AEnemyBase;
class AShadowClone;
class AFinalBossBase;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDash);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDashChargesChanged, int32, CurrentCharges, int32, MaxCharges);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwapCooldownStarted, float, CooldownDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSwapCooldownFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDamageDodged, float, IncomingDamage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTwinSoulRewardCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSamuraiTrialRewardCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNinjaTrialRewardCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnHemotoxicReactionTriggered, AEnemyBase*, Enemy, FVector, Location, float, Damage, int32, ConsumedBleedStacks, int32, ConsumedPoisonStacks);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerShadowCloneSpawned, AShadowClone*, ShadowClone);

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

	UFUNCTION(BlueprintPure, Category = "Player|Targeting")
	bool IsAutoTargetingEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Player|Targeting")
	void SetAutoTargetingEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Player|Targeting")
	bool GetCursorAttackDirection(const FVector& AttackOrigin, FVector& OutDirection) const;

	UFUNCTION(BlueprintPure, Category = "Run")
	float GetRunTimeSeconds() const;

	UFUNCTION(BlueprintCallable, Category = "Objectives")
	bool TryBeginObjective(AActor* ObjectiveActor);

	UFUNCTION(BlueprintCallable, Category = "Objectives")
	void EndObjective(AActor* ObjectiveActor);

	UFUNCTION(BlueprintPure, Category = "Objectives")
	bool IsAnyObjectiveActive() const;

	UFUNCTION(BlueprintPure, Category = "Objectives")
	AActor* GetActiveObjective() const;

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

	UFUNCTION(BlueprintCallable, Category = "Player|Swap")
	void SetSwapLocked(bool bLocked);

	UFUNCTION(BlueprintPure, Category = "Player|Swap")
	bool IsSwapLocked() const { return bSwapLocked; }

	UFUNCTION(BlueprintCallable, Category = "Player|Swap")
	bool ForceSamuraiActive();
	UFUNCTION(BlueprintCallable, Category = "Player|Swap")
	bool ForceNinjaActive();

	UFUNCTION(BlueprintCallable, Category = "Player|Dash")
	bool TryDash();

	void ShowBossHealthBar(AFinalBossBase* Boss);
	void HideBossHealthBar(AFinalBossBase* Boss);

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

	UFUNCTION(BlueprintCallable, Category = "Player|Rewards")
	void RequestBloodShrineUpgradeReward(int32 UpgradeChoiceCount = 3);

	UFUNCTION(BlueprintCallable, Category = "Player|Rewards")
	void RequestTwinSoulSynergyReward(int32 UpgradeChoiceCount = 3);

	UFUNCTION(BlueprintCallable, Category = "Player|Rewards")
	void RequestTwinSoulCompletionRewards(int32 NormalChoiceCount = 3, int32 DiscoveryChoiceCount = 3);

	UFUNCTION(BlueprintCallable, Category = "Player|Rewards")
	void RequestSamuraiTrialUpgradeReward(int32 UpgradeChoiceCount = 3);
	UFUNCTION(BlueprintCallable, Category = "Player|Rewards")
	void RequestNinjaTrialUpgradeReward(int32 UpgradeChoiceCount = 3);

	UPROPERTY(BlueprintAssignable, Category = "Player|Rewards")
	FOnTwinSoulRewardCompleted OnTwinSoulRewardCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Player|Rewards")
	FOnSamuraiTrialRewardCompleted OnSamuraiTrialRewardCompleted;
	UPROPERTY(BlueprintAssignable, Category = "Player|Rewards")
	FOnNinjaTrialRewardCompleted OnNinjaTrialRewardCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Player|Synergy|Hemotoxic Reaction")
	FOnHemotoxicReactionTriggered OnHemotoxicReactionTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Player|Dash", meta = (ToolTip = "Broadcast when a player dash successfully starts."))
	FOnPlayerDash OnDashStarted;

	UPROPERTY(BlueprintAssignable, Category = "Player|Dash", meta = (ToolTip = "Broadcast when the current player dash ends."))
	FOnPlayerDash OnDashEnded;

	UPROPERTY(BlueprintAssignable, Category = "Player|Dash", meta = (ToolTip = "Broadcast whenever current or maximum shared dash charges change."))
	FOnDashChargesChanged OnDashChargesChanged;

	UPROPERTY(BlueprintAssignable, Category = "Player|Dash", meta = (ToolTip = "Broadcast when dash charge recharge starts."))
	FOnPlayerDash OnDashRechargeStarted;

	UPROPERTY(BlueprintAssignable, Category = "Player|Dash", meta = (ToolTip = "Broadcast when one dash charge finishes recharging."))
	FOnPlayerDash OnDashRechargeCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Player|Swap", meta = (ToolTip = "Broadcast when a successful character swap starts the shared swap cooldown."))
	FOnSwapCooldownStarted OnSwapCooldownStarted;

	UPROPERTY(BlueprintAssignable, Category = "Player|Swap", meta = (ToolTip = "Broadcast when the shared swap cooldown finishes or is reset."))
	FOnSwapCooldownFinished OnSwapCooldownFinished;

	UPROPERTY(BlueprintAssignable, Category = "Player|Health", meta = (ToolTip = "Broadcast when incoming player damage is avoided by dodge chance."))
	FOnPlayerDamageDodged OnDamageDodged;

	UPROPERTY(BlueprintAssignable, Category = "Player|Shadow Clone")
	FOnPlayerShadowCloneSpawned OnShadowCloneSpawned;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "Enhanced Input mapping context added for player controls at BeginPlay."))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "Enhanced Input action used for player movement."))
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "Enhanced Input action used to swap between Samurai and Ninja."))
	TObjectPtr<UInputAction> SwapAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "Enhanced Input action reserved for assist-related input."))
	TObjectPtr<UInputAction> AssistAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "Enhanced Input action used to spend a shared dash charge and dash."))
	TObjectPtr<UInputAction> DashAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Targeting", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", ToolTip = "Right-stick magnitude required before controller aim replaces mouse aim."))
	float ControllerAimDeadzone = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Distance traveled during a player dash. Shared by Samurai and Ninja."))
	float DashDistance = 550.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.01", UIMin = "0.01", ToolTip = "How long the dash movement lasts in seconds."))
	float DashDuration = 0.18f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.01", UIMin = "0.01", ToolTip = "Seconds required to restore one shared dash charge. Recharges one charge at a time."))
	float DashRechargeTime = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow Clone")
	TSubclassOf<AShadowClone> ShadowCloneClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow Clone", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxActiveShadowClones = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Swap", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Seconds after a successful character swap before another swap is allowed. 0 disables the cooldown."))
	float SwapCooldown = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Health", meta = (ClampMin = "0.05", UIMin = "0.05", ToolTip = "Seconds between passive HP regeneration ticks. Regen amount still comes from character stats."))
	float HPRegenTickInterval = 0.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Characters", meta = (ToolTip = "Component that owns Samurai/Ninja instances and handles active character swaps."))
	TObjectPtr<UCharacterManagerComponent> CharacterManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player", meta = (ToolTip = "Shared player health component used by both Samurai and Ninja."))
	TObjectPtr<UHealthComponent> PlayerHealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player", meta = (ToolTip = "Shared experience component that stores current XP and player level."))
	TObjectPtr<UExperienceComponent> ExperienceComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player", meta = (ToolTip = "Shared/global player stats that apply across Samurai and Ninja."))
	TObjectPtr<USharedPlayerStatsComponent> SharedPlayerStatsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player", meta = (ToolTip = "Player upgrade component that owns upgrade pool, acquired levels, and upgrade selection flow."))
	TObjectPtr<UPlayerUpgradeComponent> PlayerUpgradeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Synergy", meta = (ToolTip = "Component that handles inactive-character assist and Tag Team style synergy behavior."))
	TObjectPtr<UInactiveCharacterAssistComponent> InactiveCharacterAssistComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ToolTip = "Camera rig actor class spawned to follow the active player character."))
	TSubclassOf<APlayerCameraRig> PlayerCameraRigClass;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Camera", meta = (ToolTip = "Runtime camera rig instance currently following the active character."))
	TObjectPtr<APlayerCameraRig> PlayerCameraRig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (ToolTip = "Main player HUD widget class used for health, XP, dash charges, and swap cooldown display."))
	TSubclassOf<UPlayerHUDWidget> PlayerHUDClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (ToolTip = "Level-up upgrade selection widget class shown when the player gains a level."))
	TSubclassOf<ULevelUpWidget> LevelUpWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (ToolTip = "Run-over screen shown once after shared player death."))
	TSubclassOf<UGameOverWidget> GameOverWidgetClass;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "UI", meta = (ToolTip = "Runtime instance of the player HUD widget."))
	TObjectPtr<UPlayerHUDWidget> PlayerHUDWidget;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "UI", meta = (ToolTip = "Runtime instance of the currently displayed level-up widget, if any."))
	TObjectPtr<ULevelUpWidget> LevelUpWidget;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UGameOverWidget> GameOverWidget;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run", meta = (ToolTip = "Authoritative elapsed gameplay-time source shared by enemy scaling and the HUD."))
	TObjectPtr<AEnemySpawner> RunTimeSource;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player", meta = (ToolTip = "True after the shared player health reaches zero and death flow has started."))
	bool bIsPlayerDead = false;
	bool bGameOverPresented = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player|Level Up", meta = (ToolTip = "Number of queued upgrade selections waiting to be resolved. Can be more than one if multiple levels are gained at once."))
	int32 PendingLevelUpChoices = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player|Rewards", meta = (ToolTip = "Queued Blood Shrine reward selections waiting for the shared upgrade UI."))
	int32 PendingBloodShrineRewards = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player|Rewards")
	int32 PendingTwinSoulRewards = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player|Rewards")
	int32 PendingTwinSoulDiscoveries = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player|Rewards")
	int32 PendingSamuraiTrialRewards = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player|Rewards")
	int32 PendingNinjaTrialRewards = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player|Level Up", meta = (ToolTip = "True while the level-up upgrade selection UI is open and gameplay is frozen."))
	bool bLevelUpSelectionActive = false;

	float PreviousLevelUpGlobalTimeDilation = 1.0f;
	bool bLevelUpTimeDilationApplied = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Swap", meta = (ToolTip = "Whether the shared character swap cooldown currently allows swapping."))
	bool bCanSwap = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Swap", meta = (ToolTip = "Reusable gameplay lock which blocks swap input without changing the normal cooldown."))
	bool bSwapLocked = false;
	bool bSuppressSwapEffects = false;
	bool bPlayerInitiatedSwapPending = false;

	void Move(const FInputActionValue& Value);
	void StopMoveInput(const FInputActionValue& Value);
	void Swap(const FInputActionValue& Value);
	void Dash(const FInputActionValue& Value);
	void Aim(const FInputActionValue& Value);
	void Interact();
	void ConfigureInputMode();
	void InitializePlayerCameraRig();
	void InitializePlayerHUD();
	void ResolveRunTimeSource();
	UFUNCTION()
	void HandleCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter);
	void TryTriggerHemotoxicReaction(ACharacterBase* NewCharacter);
	UFUNCTION()
	void HandlePlayerDeath();
	UFUNCTION()
	void HandlePlayerLevelUp(int32 NewLevel);
	UFUNCTION()
	void HandleLevelUpSelectionCompleted();
	UFUNCTION()
	void HandleSharedPlayerStatsChanged();
	void StartNextLevelUpSelection();
	void StartNextUpgradeSelection();
	bool EnsureLevelUpWidget();
	void PauseForLevelUpSelection();
	void ResumeAfterLevelUpSelection();
	void CloseLevelUpWidget();
	void PresentGameOver(float FinalRunTimeSeconds);
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
	void UpdateManualTargetingInput();
	void ApplyControllerAimInput(const FVector2D& StickInput);
	bool GetMouseWorldPosition(FVector& OutWorldPosition) const;
	FVector GetDashDirection(const ACharacterBase* ActiveCharacter) const;
	void HandleDashStep(float DeltaTime);
	void FinishDash();
	void SpawnShadowCloneForCompletedDash();
	void DestroyAllShadowClones();
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
	FVector LastValidControllerAimDirection = FVector::ForwardVector;
	bool bControllerIsActiveTargetingDevice = false;
	FVector ActiveDashDirection = FVector::ZeroVector;
	float DashElapsedTime = 0.0f;
	int32 CurrentDashCharges = 1;
	int32 MaxDashCharges = 1;
	bool bIsDashing = false;
	bool bPendingNinjaShadowClone = false;
	FTransform PendingShadowCloneTransform;
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AShadowClone>> ActiveShadowClones;
	bool bCurrentSelectionIsBloodShrineReward = false;
	int32 BloodShrineRewardChoiceCount = 3;
	bool bCurrentSelectionIsTwinSoulReward = false;
	bool bCurrentSelectionIsTwinSoulDiscovery = false;
	bool bCurrentSelectionIsSamuraiTrialReward = false;
	bool bCurrentSelectionIsNinjaTrialReward = false;
	int32 TwinSoulRewardChoiceCount = 3;
	int32 TwinSoulDiscoveryChoiceCount = 3;
	int32 SamuraiTrialRewardChoiceCount = 3;
	int32 NinjaTrialRewardChoiceCount = 3;
	TWeakObjectPtr<AActor> ActiveObjective;
	TWeakObjectPtr<AFinalBossBase> ActiveBossHealthBar;
};
