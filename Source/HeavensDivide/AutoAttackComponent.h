// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CharacterBase.h"
#include "Components/ActorComponent.h"
#include "AutoAttackComponent.generated.h"

class UAnimMontage;
class AAttackProjectileBase;
class AEnemyBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAutoAttack);

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

	UPROPERTY(BlueprintAssignable, Category = "Auto Attack")
	FOnAutoAttack OnAutoAttack;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float AttackInterval = 1.0f;

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

	void HandleAttackTimer();
	void ScheduleNextAttackTimer(float Delay);
	bool PlayAttackMontage();
	float CalculateAttackMontagePlayRate(const UAnimMontage* Montage) const;
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void StartTargetedAttack();
	void StartProjectileAttack();
	bool CanStartAttackNow() const;
	bool TryConsumeAttackNotify();
	AEnemyBase* FindNearestEnemyTarget() const;
	FVector GetProjectileSpawnLocation() const;
	FVector GetEnemyAimLocation(const AEnemyBase* Enemy) const;
	bool CanAutoAttack() const;

	UPROPERTY()
	TObjectPtr<ACharacterBase> OwnerCharacter;

	TWeakObjectPtr<AEnemyBase> CurrentAttackTarget;

	FTimerHandle AttackTimerHandle;
	double LastAttackStartTime = -DBL_MAX;
	int32 AttackSequence = 0;
	int32 ActiveAttackSequence = 0;
	bool bIsAttacking = false;
	bool bAttackNotifyConsumed = false;
};
