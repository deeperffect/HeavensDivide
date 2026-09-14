// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHUDWidget.generated.h"

class ACharacterBase;
class ANinjaCharacter;
class ASamuraiCharacter;
class UCharacterManagerComponent;
class UExperienceComponent;
class UHealthComponent;
class ASurvivorPlayerController;
class UTextBlock;
class UBorder;
class UProgressBar;
class AFinalBossBase;
class UMinimapWidget;
class UImage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerHUDHealthUpdated, float, CurrentHealth, float, MaxHealth, float, HealthPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerHUDActiveCharacterChanged, ACharacterBase*, NewActiveCharacter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerHUDXPUpdated, int32, CurrentXP, int32, XPToNextLevel, float, XPPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerHUDLevelUp, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerHUDLevelUpdated, int32, CurrentLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerHUDDashChargesUpdated, int32, CurrentCharges, int32, MaxCharges, float, RechargePercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerHUDDashRechargeUpdated, float, RechargePercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlayerHUDDashRechargeSlotUpdated, int32, RechargingSlotIndex, float, RechargePercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerHUDSwapCooldownStarted, float, CooldownDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerHUDSwapCooldownUpdated, float, RemainingCooldown, float, CooldownDuration, float, CooldownProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPlayerHUDSwapCooldownFinished);

UENUM(BlueprintType)
enum class EDashChargeSlotState : uint8
{
	Full,
	Recharging,
	Empty
};

USTRUCT(BlueprintType)
struct FDashChargeSlotState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Player HUD|Dash")
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Player HUD|Dash")
	EDashChargeSlotState State = EDashChargeSlotState::Empty;

	UPROPERTY(BlueprintReadOnly, Category = "Player HUD|Dash")
	float RechargePercent = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerHUDDashChargeSlotsUpdated, const TArray<FDashChargeSlotState>&, ChargeSlots);

UCLASS(BlueprintType, Blueprintable)
class HEAVENSDIVIDE_API UPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	friend class FPlayerDamageFeedbackTest;
	friend class FSwapPresentationTest;

public:
	UFUNCTION(BlueprintCallable, Category = "Player HUD")
	void InitializeFromCharacterManager(UCharacterManagerComponent* InCharacterManager);

	UFUNCTION(BlueprintCallable, Category = "Player HUD")
	void InitializeFromPlayerController(ASurvivorPlayerController* InPlayerController);

	UFUNCTION(BlueprintPure, Category = "Player HUD|Characters")
	ASamuraiCharacter* GetSamurai() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Characters")
	ANinjaCharacter* GetNinja() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Characters")
	ACharacterBase* GetActiveCharacter() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Health")
	float GetCurrentHealth() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Health")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Health")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Health")
	float GetDisplayedHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Health")
	float GetDelayedHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Experience")
	int32 GetCurrentXP() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Experience")
	int32 GetCurrentLevel() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Experience")
	int32 GetXPToNextLevel() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Experience")
	float GetXPPercent() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Dash")
	int32 GetCurrentDashCharges() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Dash")
	int32 GetMaxDashCharges() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Dash")
	float GetDashRechargePercent() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Dash")
	bool IsDashRecharging() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Dash")
	int32 GetRechargingDashSlotIndex() const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Player HUD|Dash")
	TArray<FDashChargeSlotState> GetDashChargeSlotStates() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Swap")
	float GetSwapCooldownRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Swap")
	float GetSwapCooldownDuration() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Swap")
	float GetSwapCooldownProgress() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Swap")
	bool IsSwapCoolingDown() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|State")
	bool IsSamuraiActive() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|State")
	bool IsNinjaActive() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Run")
	float GetRunTimeSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Run")
	FText FormatRunTime(float RunTimeSeconds) const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|Run")
	static FText FormatRunTimeText(float RunTimeSeconds);

	UFUNCTION(BlueprintCallable, Category="Player HUD|Boss") void ShowBossHealthBar(AFinalBossBase* Boss);
	UFUNCTION(BlueprintCallable, Category="Player HUD|Boss") void HideBossHealthBar(AFinalBossBase* Boss = nullptr);

	UFUNCTION(BlueprintImplementableEvent, Category = "Player HUD")
	void OnPlayerHealthUpdated(float CurrentHealth, float MaxHealth, float HealthPercent);

	UFUNCTION(BlueprintImplementableEvent, Category = "Player HUD")
	void OnActiveCharacterChanged(ACharacterBase* NewActiveCharacter);

	UFUNCTION(BlueprintImplementableEvent, Category = "Player HUD")
	void OnPlayerXPUpdated(int32 CurrentXP, int32 XPToNextLevel, float XPPercent);

	UFUNCTION(BlueprintImplementableEvent, Category = "Player HUD")
	void OnPlayerLevelUp(int32 NewLevel);

	UFUNCTION(BlueprintImplementableEvent, Category = "Player HUD")
	void OnPlayerLevelUpdated(int32 CurrentLevel);

	UFUNCTION(BlueprintImplementableEvent, Category = "Player HUD")
	void OnDashChargesUpdated(int32 CurrentCharges, int32 MaxCharges, float RechargePercent);

	UFUNCTION(BlueprintImplementableEvent, Category = "Player HUD")
	void OnDashRechargeUpdated(float RechargePercent);

	UFUNCTION(BlueprintImplementableEvent, Category = "Player HUD")
	void OnDashRechargeSlotUpdated(int32 RechargingSlotIndex, float RechargePercent);

	UFUNCTION(BlueprintImplementableEvent, Category = "Player HUD")
	void OnDashChargeSlotsUpdated(const TArray<FDashChargeSlotState>& ChargeSlots);

	UFUNCTION(BlueprintImplementableEvent, Category = "Player HUD")
	void OnSwapCooldownStarted(float CooldownDuration);

	UFUNCTION(BlueprintImplementableEvent, Category = "Player HUD")
	void OnSwapCooldownUpdated(float RemainingCooldown, float CooldownDuration, float CooldownProgress);

	UFUNCTION(BlueprintImplementableEvent, Category = "Player HUD")
	void OnSwapCooldownFinished();

	UPROPERTY(BlueprintAssignable, Category = "Player HUD|Events")
	FPlayerHUDHealthUpdated PlayerHealthUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Player HUD|Events")
	FPlayerHUDActiveCharacterChanged ActiveCharacterChanged;

	UPROPERTY(BlueprintAssignable, Category = "Player HUD|Events")
	FPlayerHUDXPUpdated PlayerXPUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Player HUD|Events")
	FPlayerHUDLevelUp PlayerLevelUp;

	UPROPERTY(BlueprintAssignable, Category = "Player HUD|Events")
	FPlayerHUDLevelUpdated PlayerLevelUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Player HUD|Events")
	FPlayerHUDDashChargesUpdated DashChargesUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Player HUD|Events")
	FPlayerHUDDashRechargeUpdated DashRechargeUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Player HUD|Events")
	FPlayerHUDDashRechargeSlotUpdated DashRechargeSlotUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Player HUD|Events")
	FPlayerHUDDashChargeSlotsUpdated DashChargeSlotsUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Player HUD|Events")
	FPlayerHUDSwapCooldownStarted SwapCooldownStarted;

	UPROPERTY(BlueprintAssignable, Category = "Player HUD|Events")
	FPlayerHUDSwapCooldownUpdated SwapCooldownUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Player HUD|Events")
	FPlayerHUDSwapCooldownFinished SwapCooldownFinished;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Player HUD|Swap Feedback") bool bEnableSwapPortraitPulse = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Player HUD|Swap Feedback") FName SamuraiPortraitName = TEXT("IMG_IconSamurai");
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Player HUD|Swap Feedback") FName NinjaPortraitName = TEXT("IMG_IconNinja");
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Player HUD|Swap Feedback", meta=(ClampMin="0.01")) float SwapPortraitDuration = .25f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Player HUD|Swap Feedback", meta=(ClampMin="1", ClampMax="2")) float SwapPortraitScale = 1.15f;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Player HUD|Damage Feedback")
	bool bEnableDamageVignette = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Player HUD|Damage Feedback", meta=(ClampMin="0.01", Units="s"))
	float DamageVignetteDuration = 0.45f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Player HUD|Damage Feedback", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DamageVignetteOpacity = 0.45f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Player HUD|Damage Feedback", meta=(ClampMin="0.01", ClampMax="0.45", ToolTip="Fraction of screen width/height covered by each edge fade."))
	float DamageVignetteEdgeSize = 0.20f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Player HUD|Damage Feedback")
	FLinearColor DamageVignetteColor = FLinearColor(0.8f, 0.015f, 0.025f, 1.0f);
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player HUD|Health Animation", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HealthChipDelay = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player HUD|Health Animation", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float HealthChipChaseSpeed = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player HUD|Health Animation", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HealthChipSnapTolerance = 0.002f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player HUD|References")
	TObjectPtr<UCharacterManagerComponent> CharacterManager;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player HUD|References")
	TObjectPtr<ASurvivorPlayerController> SurvivorPlayerController;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player HUD|References")
	TObjectPtr<ASamuraiCharacter> Samurai;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player HUD|References")
	TObjectPtr<ANinjaCharacter> Ninja;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player HUD|References")
	TObjectPtr<UHealthComponent> PlayerHealth;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player HUD|References")
	TObjectPtr<UExperienceComponent> PlayerExperience;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player HUD|Health Animation")
	float DisplayedHealthPercent = 1.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player HUD|Health Animation")
	float DelayedHealthPercent = 1.0f;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RunTimerText;

private:
	void StartSwapPortraitPulse(ACharacterBase* Character);
	void UpdateSwapPortraitPulse(float Delta);
	void ClearSwapPortraitPulse();
	TWeakObjectPtr<UImage> PulsingPortrait;
	FVector2D OriginalPortraitScale = FVector2D(1,1);
	FLinearColor OriginalPortraitColor = FLinearColor::White;
	float PortraitPulseRemaining = 0;
	UFUNCTION() void HandlePlayerDamaged(float DamageAmount, float CurrentHealth);
	void UpdateDamageFeedback(float DeltaSeconds);
	float GetDamageFeedbackOpacity() const;
	float DamageVignetteRemaining = 0.0f;
	void EnsureBossHealthPresentation();
	UFUNCTION() void HandleBossHealthChanged(float CurrentHealth, float MaxHealth, float HealthPercent);
	UFUNCTION()
	void HandlePlayerHealthChanged(float CurrentHealth, float MaxHealth, float HealthPercent);

	UFUNCTION()
	void HandlePlayerXPChanged(int32 CurrentXP, int32 XPToNextLevel, float XPPercent);

	UFUNCTION()
	void HandlePlayerLevelUp(int32 NewLevel);

	UFUNCTION()
	void HandleCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter);

	UFUNCTION()
	void HandleDashChargesChanged(int32 CurrentCharges, int32 MaxCharges);

	UFUNCTION()
	void HandleDashRechargeStarted();

	UFUNCTION()
	void HandleDashRechargeCompleted();

	UFUNCTION()
	void HandleSwapCooldownStarted(float CooldownDuration);

	UFUNCTION()
	void HandleSwapCooldownFinished();

	void BindCharacterHealth();
	void UnbindCharacterHealth();
	void BindPlayerExperience();
	void UnbindPlayerExperience();
	void BindDashState();
	void UnbindDashState();
	void BindSwapCooldownState();
	void UnbindSwapCooldownState();
	void BroadcastInitialState();
	void UpdateHealthAnimation(float NewHealthPercent);
	void StopHealthChipChase();
	void BroadcastDashState();
	void BroadcastDashRechargeProgress();
	void BroadcastDashChargeSlots();
	void BroadcastSwapCooldownProgress();
	void BroadcastSwapCooldownFinished();
	void InitializeRunTimerDisplay();
	void UpdateRunTimerDisplay();
	void EnsureMinimapPresentation();

	FTimerHandle HealthChipDelayTimerHandle;
	float TargetHealthPercent = 1.0f;
	bool bHealthChipChasing = false;
	bool bDashRechargeProgressActive = false;
	bool bSwapCooldownProgressActive = false;
	int64 LastDisplayedRunTimeSecond = INDEX_NONE;
	TWeakObjectPtr<AFinalBossBase> DisplayedBoss;
	UPROPERTY(Transient) TObjectPtr<UBorder> BossHealthContainer;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BossNameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BossHealthValueText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> BossHealthProgressBar;
	UPROPERTY(Transient) TObjectPtr<UMinimapWidget> MinimapWidget;
};
