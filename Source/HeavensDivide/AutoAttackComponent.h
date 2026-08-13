// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CharacterBase.h"
#include "Components/ActorComponent.h"
#include "AutoAttackComponent.generated.h"

class UAnimMontage;
class AAttackProjectileBase;
class AEnemyBase;

UENUM(BlueprintType)
enum class EAutoAttackSource : uint8
{
	NormalAutoAttack,
	Assist,
	Other
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAutoAttack, UAutoAttackComponent*, AttackComponent, EAutoAttackSource, AttackSource);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API UAutoAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAutoAttackComponent();

	UFUNCTION(BlueprintCallable, Category = "Auto Attack")
	void StartAutoAttack();

	UFUNCTION(BlueprintCallable, Category = "Auto Attack")
	void StopAutoAttack();

	UFUNCTION(BlueprintCallable, Category = "Auto Attack")
	void SetAttackInterval(float NewInterval);

	UFUNCTION(BlueprintCallable, Category = "Auto Attack|Trace")
	void PerformAttackTrace();

	UFUNCTION(BlueprintCallable, Category = "Auto Attack|Projectile")
	void SpawnAutoAttackProjectile();

	AEnemyBase* FindAssistTarget() const;
	AEnemyBase* FindAssistTargetNearLocation(const FVector& SearchLocation, float SearchRadius) const;
	bool IsProjectileAttack() const;
	bool IsTargetInCurrentMeleeReach(const AEnemyBase* TargetEnemy) const;
	bool TryStartAssistAttack(AEnemyBase*& OutTargetEnemy, float& OutExpectedDuration);
	bool TryStartAssistAttackAtTarget(AEnemyBase* TargetEnemy, float& OutExpectedDuration);

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	float GetEffectiveAttackInterval() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	float GetEffectiveAttackDamage() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	float GetEffectiveAttackRadius() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	float GetEffectiveProjectileSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	float GetEffectiveTargetingRange() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	float GetEffectiveHomingStrengthMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	int32 GetEffectiveProjectileCount() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	float GetBaseAttackInterval() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	float GetBaseAttackDamage() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	float GetBaseAttackRadius() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	float GetBaseProjectileSpeed() const;

	UPROPERTY(BlueprintAssignable, Category = "Auto Attack")
	FOnAutoAttack OnAutoAttack;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float AttackInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float ReadyTargetCheckInterval = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Animation")
	bool bScaleMontageWithAttackInterval = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Animation", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float MaxAttackMontagePlayRate = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack")
	bool bAutoAttackEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Targeting", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TargetingRange = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Targeting", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LegacyMeleeDefaultTargetingRange = 325.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Targeting", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LegacyRangedDefaultTargetingRange = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Targeting|Melee", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MeleeClusterTargetingWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Targeting|Melee", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MeleeDistanceTargetingWeight = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Targeting|Melee", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MeleeImmediateThreatBonus = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Targeting|Melee", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MeleeImmediateThreatRangeFraction = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Targeting|Melee", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxMeleeClusterCandidates = 48;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Targeting")
	bool bDebugTargeting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Projectile")
	TSubclassOf<AAttackProjectileBase> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Projectile", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ProjectileSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Projectile")
	FName ProjectileSpawnSocket = TEXT("ProjectileSocket");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Projectile")
	FVector ProjectileSpawnOffset = FVector(80.0f, 0.0f, 60.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Projectile|Homing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SharedTargetHomingOffsetStep = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Projectile|Homing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxSharedTargetHomingOffset = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Projectile|Homing", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float OuterProjectileHomingStrengthReduction = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Trace", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackRange = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Trace", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackRadius = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Trace")
	float AttackForwardOffset = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Trace")
	bool bDebugAttackTrace = false;

private:
	UFUNCTION()
	void HandleOwnerCharacterModeChanged(ECharacterMode OldMode, ECharacterMode NewMode);

	UFUNCTION()
	void HandleCharacterStatsChanged();

	void HandleAttackTimer();
	void ScheduleNextAttackTimer(float Delay);
	void ScheduleNextAttackTimerFromCooldown();
	void ScheduleReadyTargetCheckTimer();
	void ApplyLegacyTargetingRangeDefaults();
	bool PlayAttackMontage(bool bUpdateNormalCooldown = true);
	float CalculateAttackMontagePlayRate(const UAnimMontage* Montage) const;
	float GetExpectedAttackMontageDuration() const;
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	bool StartTargetedAttack();
	bool StartProjectileAttack();
	bool CanExecuteAttackInCurrentMode() const;
	bool CanStartAttackNow() const;
	bool IsOwningPlayerDead() const;
	bool TryConsumeAttackNotify();
	void SpawnProjectileInstance(const FVector& SpawnLocation, const FVector& ProjectileDirection, AEnemyBase* TargetEnemy, float Damage, float Speed, float HomingStrengthMultiplier, const FVector& HomingTargetOffset = FVector::ZeroVector);
	AEnemyBase* FindNearestEnemyTarget() const;
	AEnemyBase* FindBestMeleeTarget(const FVector& SearchLocation, float SearchRadius) const;
	float ScoreMeleeTarget(AEnemyBase* Candidate, const TArray<AEnemyBase*>& Candidates, const FVector& SearchLocation, float SearchRadius, int32& OutClusterCount, float& OutDistancePenalty, float& OutImmediateThreatBonus) const;
	void FindEnemyTargetsSorted(TArray<AEnemyBase*>& OutTargets) const;
	void FindEnemyTargetsSortedFromLocation(const FVector& SearchLocation, float SearchRadius, TArray<AEnemyBase*>& OutTargets) const;
	FVector GetProjectileSpawnLocation() const;
	FVector GetEnemyAimLocation(const AEnemyBase* Enemy) const;
	bool CanAutoAttack() const;

	UPROPERTY()
	TObjectPtr<ACharacterBase> OwnerCharacter;

	TWeakObjectPtr<AEnemyBase> CurrentAttackTarget;

	FTimerHandle AttackTimerHandle;
	double LastAttackStartTime = -DBL_MAX;
	double NextAttackReadyTime = 0.0;
	float AttackIntervalAtLastAttackStart = 1.0f;
	int32 AttackSequence = 0;
	int32 ActiveAttackSequence = 0;
	bool bIsAttacking = false;
	bool bAttackNotifyConsumed = false;
	bool bActiveAttackIsAssist = false;
};
