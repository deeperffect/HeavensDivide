// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyStatusTypes.h"
#include "EnemyStatusEffectComponent.generated.h"

class UPlayerUpgradeComponent;
enum class EPlayerAttackSource : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEnemyStatusStacksChanged, EEnemyStatusEffect, Status, int32, StackCount);

USTRUCT()
struct FEnemyDamageStatusState
{
	GENERATED_BODY()

	int32 Stacks = 0;
	float RemainingDuration = 0.0f;
	TWeakObjectPtr<UPlayerUpgradeComponent> SourceUpgrades;
	FTimerHandle TickTimer;
};

/** Lightweight, timer-driven damage-over-time state owned by one enemy. */
UCLASS(ClassGroup=(Enemy), meta=(BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API UEnemyStatusEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyStatusEffectComponent();

	bool ApplyStatus(EEnemyStatusEffect Status, UPlayerUpgradeComponent* SourceUpgrades, EPlayerAttackSource Source);

	UFUNCTION(BlueprintPure, Category="Enemy|Status")
	bool HasStatus(EEnemyStatusEffect Status) const;

	UFUNCTION(BlueprintPure, Category="Enemy|Status")
	int32 GetStatusStacks(EEnemyStatusEffect Status) const;

	UFUNCTION(BlueprintPure, Category="Enemy|Status")
	float CalculateRemainingStatusDamage(EEnemyStatusEffect Status) const;

	UFUNCTION(BlueprintCallable, Category="Enemy|Status")
	bool ConsumeStatus(EEnemyStatusEffect Status);

	UFUNCTION(BlueprintCallable, Category="Enemy|Status")
	void ClearAllStatuses();

	UPROPERTY(BlueprintAssignable, Category="Enemy|Status")
	FEnemyStatusStacksChanged OnStatusStacksChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Bleed", meta=(ClampMin="0.0"))
	float BaseBleedDamagePerTick = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Bleed", meta=(ClampMin="0.01"))
	float BleedTickInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Bleed", meta=(ClampMin="0.01"))
	float BleedDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Bleed", meta=(ClampMin="0.0"))
	float DeepCutsDamagePerLevel = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Poison", meta=(ClampMin="0.0"))
	float BasePoisonDamagePerTick = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Poison", meta=(ClampMin="0.01"))
	float PoisonTickInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Poison", meta=(ClampMin="0.01"))
	float PoisonDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Poison", meta=(ClampMin="0.0"))
	float PotentVenomDamagePerLevel = 0.25f;

private:
	FEnemyDamageStatusState BleedState;
	FEnemyDamageStatusState PoisonState;

	FEnemyDamageStatusState& GetState(EEnemyStatusEffect Status);
	const FEnemyDamageStatusState& GetState(EEnemyStatusEffect Status) const;
	float GetTickInterval(EEnemyStatusEffect Status) const;
	float GetDuration(EEnemyStatusEffect Status) const;
	float CalculateStatusDamagePerTick(EEnemyStatusEffect Status, const FEnemyDamageStatusState& State) const;
	int32 CalculateRemainingTickCount(EEnemyStatusEffect Status, const FEnemyDamageStatusState& State) const;
	void TickBleed();
	void TickPoison();
	void TickStatus(EEnemyStatusEffect Status);
	void ClearStatus(EEnemyStatusEffect Status);
	void GrantPoisonKillHealing(const UPlayerUpgradeComponent* Upgrades) const;
};
