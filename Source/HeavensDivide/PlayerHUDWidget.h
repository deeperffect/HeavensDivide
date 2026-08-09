// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHUDWidget.generated.h"

class ACharacterBase;
class ANinjaCharacter;
class ASamuraiCharacter;
class UCharacterManagerComponent;
class UHealthComponent;
class ASurvivorPlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerHUDHealthUpdated, float, CurrentHealth, float, MaxHealth, float, HealthPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerHUDActiveCharacterChanged, ACharacterBase*, NewActiveCharacter);

UCLASS(BlueprintType, Blueprintable)
class HEAVENSDIVIDE_API UPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

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

	UFUNCTION(BlueprintPure, Category = "Player HUD|State")
	bool IsSamuraiActive() const;

	UFUNCTION(BlueprintPure, Category = "Player HUD|State")
	bool IsNinjaActive() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Player HUD")
	void OnPlayerHealthUpdated(float CurrentHealth, float MaxHealth, float HealthPercent);

	UFUNCTION(BlueprintImplementableEvent, Category = "Player HUD")
	void OnActiveCharacterChanged(ACharacterBase* NewActiveCharacter);

	UPROPERTY(BlueprintAssignable, Category = "Player HUD|Events")
	FPlayerHUDHealthUpdated PlayerHealthUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Player HUD|Events")
	FPlayerHUDActiveCharacterChanged ActiveCharacterChanged;

protected:
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

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player HUD|Health Animation")
	float DisplayedHealthPercent = 1.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player HUD|Health Animation")
	float DelayedHealthPercent = 1.0f;

private:
	UFUNCTION()
	void HandlePlayerHealthChanged(float CurrentHealth, float MaxHealth, float HealthPercent);

	UFUNCTION()
	void HandleCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter);

	void BindCharacterHealth();
	void UnbindCharacterHealth();
	void BroadcastInitialState();
	void UpdateHealthAnimation(float NewHealthPercent);
	void StopHealthChipChase();

	FTimerHandle HealthChipDelayTimerHandle;
	float TargetHealthPercent = 1.0f;
	bool bHealthChipChasing = false;
};
