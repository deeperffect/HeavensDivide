// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShadowClone.generated.h"

class ANinjaCharacter;
class ASurvivorPlayerController;
class UAutoAttackComponent;
class USceneComponent;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShadowCloneEvent, AShadowClone*, ShadowClone);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShadowCloneAttackEvent, AShadowClone*, ShadowClone, int32, RemainingAttacks);

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AShadowClone : public AActor
{
	GENERATED_BODY()

public:
	AShadowClone();
	void InitializeShadowClone(ANinjaCharacter* InSourceNinja, ASurvivorPlayerController* InController, int32 InAttackQuota);

	// Called by the normal Ninja projectile montage notify when that montage is playing on this clone.
	void HandleAttackProjectileNotify();

	UPROPERTY(BlueprintAssignable, Category = "Shadow Clone|Events")
	FOnShadowCloneEvent OnShadowCloneSpawned;
	UPROPERTY(BlueprintAssignable, Category = "Shadow Clone|Events")
	FOnShadowCloneAttackEvent OnShadowCloneAttack;
	UPROPERTY(BlueprintAssignable, Category = "Shadow Clone|Events")
	FOnShadowCloneEvent OnShadowCloneFinished;
	UPROPERTY(BlueprintAssignable, Category = "Shadow Clone|Events")
	FOnShadowCloneEvent OnShadowCloneBeginDisappear;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shadow Clone")
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shadow Clone")
	TObjectPtr<USkeletalMeshComponent> CloneMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadow Clone|Timing", meta = (ClampMin = "0.0"))
	float InitialAttackDelay = 0.20f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadow Clone|Timing", meta = (ClampMin = "0.01"))
	float BaseAttackInterval = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadow Clone|Timing", meta = (ClampMin = "0.01"))
	float TargetRetryInterval = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadow Clone|Timing", meta = (ClampMin = "0.1"))
	float SafetyLifetime = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadow Clone|Timing", meta = (ClampMin = "0.0"))
	float PostAttackLingerDuration = 0.60f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadow Clone|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 1200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadow Clone|Combat")
	FVector ProjectileOriginOffset = FVector(0.0f, 0.0f, 60.0f);

private:
	void BeginAttack();
	void BeginFinalLinger();
	void FinishClone();
	float GetAttackInterval() const;
	float GetProjectileNotifyDelay(const class UAnimMontage* Montage, float PlayRate) const;

	TWeakObjectPtr<ANinjaCharacter> SourceNinja;
	TWeakObjectPtr<ASurvivorPlayerController> SourceController;
	TWeakObjectPtr<UAutoAttackComponent> SourceAttack;
	int32 RemainingAttacks = 0;
	bool bFinished = false;
	bool bAttackInProgress = false;
	bool bAttackQuotaFinished = false;
	float ActiveMontagePlayRate = 1.0f;
	FTimerHandle AttackTimer;
	FTimerHandle LingerTimer;
	FTimerHandle SafetyTimer;
};
