// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "MeleeEnemyBase.generated.h"

class UAnimMontage;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AMeleeEnemyBase : public AEnemyBase
{
	GENERATED_BODY()

public:
	AMeleeEnemyBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "Enemy|Attack")
	virtual void PerformAttackHit();

	virtual void ApplySpawnDifficultyScaling(float HealthMultiplier, float DamageMultiplier) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UpdateEnemyBehavior(float DeltaSeconds) override;
	virtual bool ShouldSkipMovement() const override;
	virtual void StopEnemyBehavior() override;
	virtual void HandlePlayerCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter) override;
	virtual void HandleDeath() override;
	virtual bool ShouldForceHighAnimationBudgetSignificance() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackRange = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float AttackInterval = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Hit", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay))
	float AttackHitRadius = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Hit", meta = (AdvancedDisplay))
	float AttackHitForwardOffset = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Hit", meta = (AdvancedDisplay))
	bool bDebugAttackHit = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attack", meta = (AdvancedDisplay))
	bool bIsAttacking = false;

	bool IsTargetInAttackRange() const;
	void StartAttackTimer();
	void StopAttackTimer();
	void HandleAttackTimer();
	void StartAttack();
	virtual void HandleAttackCommitted();
	virtual void HandleAttackFinished();
	virtual void ExecuteAttackHit();
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	FTimerHandle AttackTimerHandle;
};
