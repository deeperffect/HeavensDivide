// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyBase.generated.h"

class UHealthComponent;
class UAnimMontage;
class UWidgetComponent;
class UEnemyHealthBarWidget;
class USkeletalMeshComponentBudgeted;
class ACharacterBase;
class UCharacterManagerComponent;
class UEnemyLightweightMovementComponent;
class UExperienceComponent;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AEnemyBase : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FVector GetVelocity() const override;

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void SetTarget(AActor* NewTarget);

	UFUNCTION(BlueprintPure, Category = "Enemy")
	AActor* GetTarget() const;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UHealthComponent* GetHealthComponent() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Movement")
	FVector GetEnemyMovementVelocity() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> HealthBarWidgetComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UEnemyLightweightMovementComponent> LightweightMovementComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UEnemyHealthBarWidget> HealthBarWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	float HealthBarHeightOffset = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FVector2D HealthBarDrawSize = FVector2D(120.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Budget")
	bool bUseAnimationBudgetAllocator = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Budget", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float AnimationBudgetMs = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Budget", meta = (ClampMin = "0.0", UIMin = "0.0", FormerlySerializedAs = "AnimationBudgetNeverSkipDistance"))
	float AnimationBudgetHighSignificanceDistance = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Budget", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AnimationBudgetMaxSignificanceDistance = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Budget")
	bool bUseFixedSkelBoundsForEnemies = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StopDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bUseLightweightMovement = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Crowd Spread")
	bool bUseCrowdSpread = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Crowd Spread", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float CrowdSpreadStrength = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Crowd Spread")
	bool bDebugCrowdSpread = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Rotation", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EnemyRotationSpeed = 720.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float BehaviorUpdateInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<AActor> CurrentTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	bool bIsDead = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DeathDestroyDelay = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0", UIMin = "0"))
	int32 XPReward = 1;

	UFUNCTION()
	virtual void HandleDeath();

	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth, float MaxHealth, float HealthPercent);

	UFUNCTION()
	virtual void HandlePlayerCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter);

	void InitializeTargetFromCharacterManager();
	bool EnsureTargetFromCharacterManager();
	void CachePlayerExperienceComponent();
	void AwardXPReward();
	void InitializeHealthBar();
	void UpdateHealthBarVisibility(float HealthPercent);
	void StartBehaviorUpdates();
	void StopBehaviorUpdates();
	void HandleBehaviorUpdateTimer();
	void ApplyDesiredMovementInput(float DeltaSeconds);
	void StopDesiredMovement();
	void InitializeEnemyMovementMode();
	void InitializeCrowdSpread();
	FVector ApplyCrowdSpreadToDirection(const FVector& DirectDirection) const;
	void RequestEnemyMovement(const FVector& WorldDirection);
	void StopEnemyMovement();
	bool IsUsingLightweightMovement() const;
	bool IsEnemyCharacterMovementProfilingDisabled() const;
	void UpdateCharacterMovementProfilingState();
	bool IsEnemyAnimationProfilingDisabled() const;
	void UpdateAnimationProfilingState();
	void InitializeAnimationBudgeting();
	void UpdateAnimationBudgetSignificance();
	void LogAnimationBudgetSetup(USkeletalMeshComponentBudgeted* BudgetedMesh, bool bAllocatorEnabled) const;
	virtual bool ShouldForceHighAnimationBudgetSignificance() const;
	virtual void UpdateEnemyBehavior(float DeltaSeconds);
	virtual bool ShouldSkipMovement() const;
	virtual void StopEnemyBehavior();
	void MoveTowardCurrentTarget();
	void HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void DestroyAfterDeath();
	void FaceTarget();
	void SmoothFaceTarget(float DeltaSeconds);
	bool IsPlayerTargetDead() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy")
	void OnEnemyDeath();

	UPROPERTY()
	TObjectPtr<UCharacterManagerComponent> ObservedCharacterManager;

	UPROPERTY()
	TObjectPtr<UExperienceComponent> CachedPlayerExperienceComponent;

	FTimerHandle BehaviorUpdateTimerHandle;
	FVector DesiredMovementDirection = FVector::ZeroVector;
	FVector DesiredDirectMovementDirection = FVector::ZeroVector;
	float CrowdSpreadBias = 0.0f;
	bool bHasDesiredMovementDirection = false;
	bool bCharacterMovementDisabledForProfiling = false;
	bool bAnimationDisabledForProfiling = false;
	bool bAnimationBudgetInitialized = false;
	bool bXPRewardAwarded = false;
};
