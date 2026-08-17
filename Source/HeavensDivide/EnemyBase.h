// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyBase.generated.h"

class UHealthComponent;
class UAnimMontage;
class UWidgetComponent;
class UEnemyHealthBarWidget;
class UEnemyMarkIndicatorWidget;
class USkeletalMeshComponentBudgeted;
class ACharacterBase;
class UCharacterManagerComponent;
class UEnemyLightweightMovementComponent;
class UExperienceComponent;
class AExperiencePickup;

class AEnemyBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEnemyMarkStateChanged, AEnemyBase*, Enemy);

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

	UFUNCTION(BlueprintPure, Category = "Enemy|Mark")
	bool IsMarked() const;

	UFUNCTION(BlueprintCallable, Category = "Enemy|Mark")
	bool ApplyMark();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Mark")
	bool ConsumeMark();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Mark")
	void ClearMark();

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UHealthComponent* GetHealthComponent() const;

	UFUNCTION(BlueprintCallable, Category = "Enemy|Scaling")
	virtual void ApplySpawnDifficultyScaling(float HealthMultiplier, float DamageMultiplier);

	UFUNCTION(BlueprintPure, Category = "Enemy|Movement")
	FVector GetEnemyMovementVelocity() const;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Mark")
	FEnemyMarkStateChanged OnMarked;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Mark")
	FEnemyMarkStateChanged OnMarkConsumed;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Mark")
	FEnemyMarkStateChanged OnMarkCleared;

	void LogEnemyDebugState(const TCHAR* Context) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> HealthBarWidgetComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> MarkIndicatorWidgetComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UEnemyLightweightMovementComponent> LightweightMovementComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UEnemyHealthBarWidget> HealthBarWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AdvancedDisplay))
	float HealthBarHeightOffset = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AdvancedDisplay))
	FVector2D HealthBarDrawSize = FVector2D(120.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Mark")
	TSubclassOf<UEnemyMarkIndicatorWidget> MarkIndicatorWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Mark")
	FVector MarkIndicatorRelativeLocation = FVector(0.0f, 0.0f, 150.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Mark")
	FVector2D MarkIndicatorDrawSize = FVector2D(32.0f, 32.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Mark", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float MarkIndicatorScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Budget", meta = (AdvancedDisplay))
	bool bUseAnimationBudgetAllocator = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Budget", meta = (ClampMin = "0.1", UIMin = "0.1", AdvancedDisplay))
	float AnimationBudgetMs = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Budget", meta = (ClampMin = "0.0", UIMin = "0.0", FormerlySerializedAs = "AnimationBudgetNeverSkipDistance", AdvancedDisplay))
	float AnimationBudgetHighSignificanceDistance = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Budget", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay))
	float AnimationBudgetMaxSignificanceDistance = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Budget", meta = (AdvancedDisplay))
	bool bUseFixedSkelBoundsForEnemies = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StopDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AdvancedDisplay))
	bool bUseLightweightMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Crowd Spread", meta = (AdvancedDisplay))
	bool bUseCrowdSpread = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Crowd Spread", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0", AdvancedDisplay))
	float CrowdSpreadStrength = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Crowd Spread", meta = (AdvancedDisplay))
	bool bDebugCrowdSpread = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Separation", meta = (AdvancedDisplay))
	bool bUseEnemySeparation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Separation", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay))
	float SeparationRadius = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Separation", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay))
	float SeparationStrength = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Separation", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay))
	float MaxSeparationContribution = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Separation", meta = (ClampMin = "0.01", UIMin = "0.01", AdvancedDisplay))
	float SeparationUpdateInterval = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Rotation", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EnemyRotationSpeed = 720.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.01", UIMin = "0.01", AdvancedDisplay))
	float BehaviorUpdateInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Navigation", meta = (AdvancedDisplay))
	bool bUseLightweightNavSteering = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Navigation", meta = (ClampMin = "0.05", UIMin = "0.05", AdvancedDisplay))
	float NavSteeringUpdateInterval = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Navigation", meta = (ClampMin = "1", UIMin = "1", AdvancedDisplay))
	int32 NavSteeringPathPointLookAhead = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Navigation", meta = (AdvancedDisplay))
	bool bDebugLightweightNavSteering = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<AActor> CurrentTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	bool bIsDead = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Mark")
	bool bIsMarked = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay))
	float DeathDestroyDelay = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0", UIMin = "0"))
	int32 XPReward = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards")
	TSubclassOf<AExperiencePickup> ExperiencePickupClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay))
	float ExperiencePickupSpawnScatterRadius = 35.0f;

	UFUNCTION()
	virtual void HandleDeath();

	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth, float MaxHealth, float HealthPercent);

	UFUNCTION()
	virtual void HandlePlayerCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter);

	void InitializeTargetFromCharacterManager();
	bool EnsureTargetFromCharacterManager();
	void CachePlayerExperienceComponent();
	void SpawnExperiencePickup();
	void InitializeHealthBar();
	void UpdateHealthBarVisibility(float HealthPercent);
	void HideHealthBar();
	void InitializeMarkIndicator();
	void UpdateMarkIndicatorVisibility();
	void HideMarkIndicator();
	void ConfigureEnemyCapsuleCollisionDefaults();
	void ConfigureEnemyMovementDefaults();
	void StartBehaviorUpdates();
	void StopBehaviorUpdates();
	void HandleBehaviorUpdateTimer();
	void StartSeparationUpdates();
	void StopSeparationUpdates();
	void HandleSeparationUpdateTimer();
	void ApplyDesiredMovementInput(float DeltaSeconds);
	void StopDesiredMovement();
	void InitializeEnemyMovementMode();
	void InitializeCrowdSpread();
	FVector ApplyCrowdSpreadToDirection(const FVector& DirectDirection) const;
	FVector ApplyEnemySeparationToDirection(const FVector& MovementDirection) const;
	FVector ApplyLightweightNavSteeringToDirection(const FVector& DirectDirection);
	void UpdateLightweightNavSteeringDirection(const FVector& DirectDirection);
	void UpdateCachedEnemySeparation();
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
	FTimerHandle SeparationUpdateTimerHandle;
	FVector DesiredMovementDirection = FVector::ZeroVector;
	FVector DesiredDirectMovementDirection = FVector::ZeroVector;
	FVector CachedEnemySeparationVector = FVector::ZeroVector;
	FVector CachedNavSteeringDirection = FVector::ZeroVector;
	float CrowdSpreadBias = 0.0f;
	float NextNavSteeringUpdateTime = 0.0f;
	bool bHasDesiredMovementDirection = false;
	bool bCharacterMovementDisabledForProfiling = false;
	bool bAnimationDisabledForProfiling = false;
	bool bAnimationBudgetInitialized = false;
	bool bExperiencePickupSpawned = false;
};
