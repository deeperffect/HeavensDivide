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
	UFUNCTION(BlueprintCallable, Category = "Enemy|Attack")
	void PerformAttackHit();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UpdateEnemyBehavior(float DeltaSeconds) override;
	virtual bool ShouldSkipMovement() const override;
	virtual void StopEnemyBehavior() override;
	virtual void HandlePlayerCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter) override;
	virtual void HandleDeath() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackRange = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float AttackInterval = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Hit", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackHitRadius = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Hit")
	float AttackHitForwardOffset = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Hit")
	bool bDebugAttackHit = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attack")
	bool bIsAttacking = false;

	bool IsTargetInAttackRange() const;
	void StartAttackTimer();
	void StopAttackTimer();
	void HandleAttackTimer();
	void StartAttack();
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	FTimerHandle AttackTimerHandle;
};
