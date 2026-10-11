// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "MeleeEnemyBase.generated.h"
class AEnemyAttackCue;


UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AMeleeEnemyBase : public AEnemyBase
{
	GENERATED_BODY()
	friend class FBasicMeleeAttackTest;

public:
	AMeleeEnemyBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void SetTarget(AActor* NewTarget) override;

	virtual void ApplySpawnDifficultyScaling(float HealthMultiplier, float DamageMultiplier) override;
	virtual void ApplySpawnInstanceModifiers(float HealthMultiplier, float DamageMultiplier, float MovementSpeedMultiplier) override;

protected:
	virtual void CapturePreBloodboundState() override;
	virtual void RestorePreBloodboundState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UpdateEnemyBehavior(float DeltaSeconds) override;
	virtual bool ShouldSkipMovement() const override;
	virtual float GetChaseStopDistance() const override { return FMath::Min(StopDistance, FMath::Max(0.0f, AttackRange - 1.0f)); }
	virtual void StopEnemyBehavior() override;
	virtual void HandlePlayerCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter) override;
	virtual void HandleDeath() override;
	virtual bool ShouldForceHighAnimationBudgetSignificance() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Attack|Melee", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackRange = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Attack|Melee", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float AttackInterval = 1.5f; // Start-to-start cooldown, independent of the windup before each hit.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Attack|Melee", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackDamage = 10.0f;
	float PreBloodboundAttackDamage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Attack|Melee", meta=(ClampMin="0"))
	float AttackWindup = 0.20f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Attack|Melee", meta=(ClampMin="0"))
	float AttackRecovery = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Attack|Melee|Presentation")
	bool bUseProceduralAttackMotion = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Attack|Melee|Presentation", meta=(ClampMin="0"))
	float AttackLungeDistance = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Attack|Melee|Presentation", meta=(ClampMin="0"))
	float AttackLungeOutDuration = 0.06f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Attack|Melee|Presentation", meta=(ClampMin="0"))
	float AttackLungeReturnDuration = 0.12f;
	// Applied around the actor's axes so differently oriented species lean toward their target.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Attack|Melee|Presentation", meta=(ClampMin="0", ClampMax="30", Units="deg"))
	float AttackWobbleAngle = 12.0f;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Attack|Melee", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay))
	float AttackHitRadius = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Attack|Melee", meta = (AdvancedDisplay))
	float AttackHitForwardOffset = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Attack|Melee", meta = (AdvancedDisplay))
	bool bDebugAttackHit = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Attack|Melee", meta = (AdvancedDisplay))
	bool bIsAttacking = false;
	TWeakObjectPtr<AEnemyAttackCue> AttackCue;

	bool IsTargetInAttackRange() const;
	bool CanStartAttackNow() const;
	float GetAttackTimerDelay() const;
	void MarkAttackStarted();
	void StartAttackTimer();
	void StopAttackTimer();
	void HandleAttackTimer();
	virtual void StartAttack();
	virtual void HandleAttackCommitted();
	virtual void HandleAttackFinished();
	virtual void ExecuteAttackHit();

	FTimerHandle AttackTimerHandle;
	double NextAttackStartTime = 0.0;
private:
	enum class EBasicAttackPhase : uint8 { None, Windup, Recovery };
	EBasicAttackPhase BasicAttackPhase = EBasicAttackPhase::None;
	TWeakObjectPtr<AActor> BasicAttackTarget;
	FTimerHandle WindupTimer;
	FTimerHandle RecoveryTimer;
	FTimerHandle LungeTimer;
	FVector PreAttackMeshLocation = FVector::ZeroVector;
	FQuat PreAttackMeshRotation = FQuat::Identity;
	FVector LungeRelativeDirection = FVector::ForwardVector;
	FVector WobbleRelativeAxis = FVector::RightVector;
	double LungeStartTime = 0.0;
	bool bLungeActive = false;
	bool CanContinueBasicAttack() const;
	void FinishAttackWindup();
	void FinishAttackRecovery();
	void StartAttackLunge();
	void UpdateAttackLunge();
	void ResetAttackLunge();
	void CancelBasicAttack();
};
