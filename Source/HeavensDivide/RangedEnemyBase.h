// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "RangedEnemyBase.generated.h"

class AAttackProjectileBase;
class UAnimMontage;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ARangedEnemyBase : public AEnemyBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Enemy|Attack")
	void SpawnAttackProjectile();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UpdateEnemyBehavior(float DeltaSeconds) override;
	virtual bool ShouldSkipMovement() const override;
	virtual void StopEnemyBehavior() override;
	virtual void HandlePlayerCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter) override;
	virtual void HandleDeath() override;
	virtual bool ShouldForceHighAnimationBudgetSignificance() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackRange = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float AttackInterval = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Projectile")
	TSubclassOf<AAttackProjectileBase> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Projectile")
	FName ProjectileSpawnSocket = TEXT("FireballSocket");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Projectile", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ProjectileSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Projectile")
	FVector ProjectileSpawnOffset = FVector(80.0f, 0.0f, 60.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attack")
	bool bIsAttacking = false;

	bool IsTargetInAttackRange() const;
	void StartAttackTimer();
	void StopAttackTimer();
	void HandleAttackTimer();
	void StartAttack();
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	FVector GetProjectileSpawnLocation() const;

	FTimerHandle AttackTimerHandle;
};
