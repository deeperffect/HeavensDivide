// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InactiveCharacterAssistComponent.generated.h"

class ACharacterBase;
class AEnemyBase;
class ASurvivorPlayerController;
class UPlayerUpgradeComponent;
class UUpgradeDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInactiveAssistStarted, ACharacterBase*, AssistCharacter, ACharacterBase*, ActiveCharacter, AEnemyBase*, TargetEnemy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInactiveAssistEnded, ACharacterBase*, AssistCharacter);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API UInactiveCharacterAssistComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInactiveCharacterAssistComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "Synergy|Assist")
	void RefreshAssistEffectState();

	UPROPERTY(BlueprintAssignable, Category = "Synergy|Assist")
	FOnInactiveAssistStarted OnAssistStarted;

	UPROPERTY(BlueprintAssignable, Category = "Synergy|Assist")
	FOnInactiveAssistStarted OnAssistAttackExecuted;

	UPROPERTY(BlueprintAssignable, Category = "Synergy|Assist")
	FOnInactiveAssistEnded OnAssistEnded;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synergy|Assist", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float AssistInterval = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synergy|Assist")
	FVector SamuraiAssistOffset = FVector(-120.0f, -180.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synergy|Assist")
	FVector NinjaAssistOffset = FVector(120.0f, -180.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synergy|Assist|Melee", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MeleeAssistDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synergy|Assist|Melee", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxMeleeAssistTargetDistance = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synergy|Assist", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float MinimumAssistVisibleDuration = 0.65f;

private:
	UFUNCTION()
	void HandleUpgradeAcquired(UUpgradeDefinition* Upgrade, int32 NewLevel);

	UFUNCTION()
	void HandleCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter);

	void StartAssistTimer();
	void StopAssistTimer();
	void HandleAssistTimerElapsed();
	void FinishCurrentAssist();
	bool HasAssistUpgrade() const;
	FVector GetRangedAssistLocation(const ACharacterBase* ActiveCharacter, const ACharacterBase* AssistCharacter) const;
	bool TryFindMeleeAssistLocation(ACharacterBase* AssistCharacter, const ACharacterBase* ActiveCharacter, const AEnemyBase* TargetEnemy, FVector& OutAssistLocation) const;
	bool IsAssistLocationBlocked(const ACharacterBase* AssistCharacter, const FVector& AssistLocation) const;

	UPROPERTY()
	TObjectPtr<ASurvivorPlayerController> SurvivorController;

	UPROPERTY()
	TObjectPtr<UPlayerUpgradeComponent> PlayerUpgrades;

	UPROPERTY()
	TObjectPtr<ACharacterBase> CurrentAssistCharacter;

	TWeakObjectPtr<AEnemyBase> LastDebugMeleeAssistTarget;

	FTimerHandle AssistTimerHandle;
	FTimerHandle AssistCleanupTimerHandle;
	bool bAssistActive = false;
};
