// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyStatusTypes.h"
#include "EnemyStatusEffectComponent.generated.h"

class UPlayerUpgradeComponent;
class UUpgradeDefinition;
class AEnemyBase;
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
	float ActiveTickInterval = 0.0f;
	float BleedHitBonusPerTick = 0.0f;
 bool bBarragePoison = false;
 bool bDirectBarragePoison = false;
 float PoisonDamagePerTick = 0.f;
};

/** Lightweight, timer-driven damage-over-time state owned by one enemy. */
UCLASS(ClassGroup=(Enemy), meta=(BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API UEnemyStatusEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	friend class FSamuraiBuildsTest;
 friend class FBarrageBuildsTest;
	UEnemyStatusEffectComponent();

	/** Intrinsic ability/assist statuses share all normal scaling and source restrictions, without requiring the basic-attack starter. */
	bool ApplyStatus(EEnemyStatusEffect Status, UPlayerUpgradeComponent* SourceUpgrades, EPlayerAttackSource Source, bool bIntrinsicStatus = false, float ApplyingHitDamage = 0.0f);
	/** Called once before death clears status state. Transfers the current Blood Stance stacks. */
	void TransferBleedOnDeath();
	void GrantBloodRushOnDeath();
 bool ApplyBarragePoison(UPlayerUpgradeComponent* U, float HitDamage, bool bFromPuddle = false);
 void BarragePoisonDeath();
	void TryBloodDetonation();
	void ReceiveBloodStacks(UPlayerUpgradeComponent* Upgrades, int32 Stacks, float DamagePerTick, float Duration);

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

	// Serialized compatibility only. Blood Stance now owns Bleed's damage and duration.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Legacy", meta=(AdvancedDisplay, DeprecatedProperty, DeprecationMessage="Bleed damage comes from the applying hit and Blood Stance."))
	float BaseBleedDamagePerTick = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Legacy", meta=(AdvancedDisplay, DeprecatedProperty, DeprecationMessage="Blood Stance ticks every 0.5 seconds."))
	float BleedTickInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Legacy", meta=(AdvancedDisplay, DeprecatedProperty, DeprecationMessage="Blood Stance and Lingering Wounds control Bleed duration."))
	float BleedDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Legacy", meta=(AdvancedDisplay, DeprecatedProperty, DeprecationMessage="Deep Cuts is retired."))
	float DeepCutsDamagePerLevel = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Poison", meta=(ClampMin="0.0"))
	float BasePoisonDamagePerTick = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Poison", meta=(ClampMin="0.01"))
	float PoisonTickInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Poison", meta=(ClampMin="0.01"))
	float PoisonDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Status|Legacy", meta=(AdvancedDisplay, DeprecatedProperty, DeprecationMessage="Potent Venom is retired."))
	float PotentVenomDamagePerLevel = 0.25f;

private:
	FEnemyDamageStatusState BleedState;
	FEnemyDamageStatusState PoisonState;

	FEnemyDamageStatusState& GetState(EEnemyStatusEffect Status);
	const FEnemyDamageStatusState& GetState(EEnemyStatusEffect Status) const;
	float GetTickInterval(EEnemyStatusEffect Status) const;
	float GetEffectiveTickInterval(EEnemyStatusEffect Status, const FEnemyDamageStatusState& State) const;
	float CalculateStatusDamagePerTick(EEnemyStatusEffect Status, const FEnemyDamageStatusState& State) const;
	int32 CalculateRemainingTickCount(EEnemyStatusEffect Status, const FEnemyDamageStatusState& State) const;
	void TickBleed();
	void TickPoison();
	void TickStatus(EEnemyStatusEffect Status);
	void ClearStatus(EEnemyStatusEffect Status);
};
