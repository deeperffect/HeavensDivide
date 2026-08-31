// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyStatusTypes.h"
#include "GameFramework/Character.h"
#include "EnemyBase.generated.h"

class UHealthComponent;
class UAnimMontage;
class UWidgetComponent;
class UEnemyHealthBarWidget;
class UEnemyMarkIndicatorWidget;
class USkeletalMeshComponentBudgeted;
class ACharacterBase;
class ASurvivorPlayerController;
class UCharacterManagerComponent;
class UEnemyLightweightMovementComponent;
class UExperienceComponent;
class AExperiencePickup;
class UNiagaraComponent;
class UNiagaraSystem;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UEnemyStatusEffectComponent;
class UEnemyStatusIndicatorWidget;

class AEnemyBase;

UENUM(BlueprintType)
enum class EPlayerAttackSource : uint8
{
	Other,
	Samurai,
	Ninja
};

UENUM(BlueprintType)
enum class EEnemyDropCategory : uint8
{
	Normal,
	Elite,
	Boss
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEnemyMarkStateChanged, AEnemyBase*, Enemy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEnemyDied, AEnemyBase*, Enemy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEnemyBecameBloodbound, AEnemyBase*, Enemy);

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

	UFUNCTION(BlueprintCallable, Category = "Enemy|Stress Test")
	void ConfigureForStressTest(bool bDisableCombat, bool bMakeInvulnerable);

	UFUNCTION(BlueprintPure, Category = "Enemy|Stress Test")
	bool IsStressTestEnemy() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Stress Test")
	bool IsStressTestCombatDisabled() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Stress Test")
	bool IsStressTestInvulnerable() const;

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

	UFUNCTION(BlueprintCallable, Category = "Enemy|Scaling")
	virtual void ApplySpawnInstanceModifiers(float HealthMultiplier, float DamageMultiplier, float MovementSpeedMultiplier);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Damage")
	virtual bool ApplyPlayerDamage(float DamageAmount, EPlayerAttackSource AttackSource);
	bool ApplyStatus(EEnemyStatusEffect Status, class UPlayerUpgradeComponent* SourceUpgrades, EPlayerAttackSource AttackSource);

	UFUNCTION(BlueprintPure, Category = "Enemy|Status")
	bool HasStatus(EEnemyStatusEffect Status) const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Status")
	int32 GetStatusStacks(EEnemyStatusEffect Status) const;
	UEnemyStatusEffectComponent* GetStatusEffectComponent() const { return StatusEffectComponent; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Damage")
	bool CanReceivePlayerDamage(EPlayerAttackSource AttackSource) const;
	EPlayerAttackSource GetRequiredPlayerAttackSource() const { return RequiredPlayerAttackSource; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Damage")
	static EPlayerAttackSource ResolvePlayerAttackSource(const AActor* DamageSourceActor);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Objective")
	void ConfigureObjectiveEnemy(float MaxHealth, EPlayerAttackSource RequiredSource, UMaterialInterface* OverlayMaterial, FLinearColor OverlayTint);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Objective")
	void SetGameplaySuspended(bool bSuspended);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Bloodbound")
	void MakeBloodbound(float HealthMultiplier, float DamageMultiplier, float MovementSpeedMultiplier, bool bInDropsXP);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Bloodbound")
	bool RemoveBloodbound();

	UFUNCTION(BlueprintPure, Category = "Enemy|Bloodbound")
	bool IsBloodbound() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Bloodbound")
	int32 GetBloodValue() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Rewards")
	bool ShouldDropXP() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Rewards")
	EEnemyDropCategory GetDropCategory() const { return DropCategory; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Movement")
	FVector GetEnemyMovementVelocity() const;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Mark", meta = (ToolTip = "Broadcast when this enemy becomes Marked for Death."))
	FEnemyMarkStateChanged OnMarked;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Mark", meta = (ToolTip = "Broadcast when this enemy's Mark is consumed by gameplay."))
	FEnemyMarkStateChanged OnMarkConsumed;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Mark", meta = (ToolTip = "Broadcast when this enemy's Mark is cleared without being consumed."))
	FEnemyMarkStateChanged OnMarkCleared;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events", meta = (ToolTip = "Authoritative one-shot notification emitted when health death processing begins."))
	FEnemyDied OnEnemyDied;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Bloodbound")
	FEnemyBecameBloodbound OnBecameBloodbound;

	void LogEnemyDebugState(const TCHAR* Context) const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Rewards", meta = (ToolTip = "Category used by centralized enemy-death drop systems. Set elite enemy Blueprint defaults to Elite; final bosses set this automatically."))
	EEnemyDropCategory DropCategory = EEnemyDropCategory::Normal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (ToolTip = "Health component that stores this enemy's current/max health and broadcasts death."))
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (ToolTip = "World-space widget component used for the enemy health bar."))
	TObjectPtr<UWidgetComponent> HealthBarWidgetComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (ToolTip = "World-space widget component shown while this enemy is Marked for Death."))
	TObjectPtr<UWidgetComponent> MarkIndicatorWidgetComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> BleedStatusWidgetComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> PoisonStatusWidgetComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (ToolTip = "Custom lightweight movement component used instead of CharacterMovement for enemy movement."))
	TObjectPtr<UEnemyLightweightMovementComponent> LightweightMovementComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UEnemyStatusEffectComponent> StatusEffectComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Status")
	FVector BleedStatusIndicatorRelativeLocation = FVector(-22.0f, 0.0f, 145.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Status", meta=(ToolTip="Additional offset from the Bleed/status anchor used for Poison only when both statuses are visible."))
	FVector PoisonStatusIndicatorRelativeLocation = FVector(0.0f, 0.0f, 40.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Status")
	FVector2D StatusIndicatorDrawSize = FVector2D(36.0f, 36.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Status")
	bool bShowStatusStackCountAtOne = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Status", meta=(ClampMin="6", ClampMax="32"))
	int32 StatusStackFontSize = 12;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (ToolTip = "Optional attached Niagara aura used while this enemy is Bloodbound."))
	TObjectPtr<UNiagaraComponent> BloodboundNiagaraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Bloodbound Visuals", meta = (ToolTip = "Optional lightweight persistent Niagara aura. It stays inactive until this enemy becomes Bloodbound."))
	TObjectPtr<UNiagaraSystem> BloodboundNiagaraSystem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Bloodbound Visuals", meta = (ToolTip = "Optional overlay material that adds a Bloodbound silhouette treatment without replacing the enemy's original materials."))
	TObjectPtr<UMaterialInterface> BloodboundOverlayMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Bloodbound Visuals")
	FLinearColor BloodboundTint = FLinearColor(0.35f, 0.005f, 0.01f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Bloodbound Visuals", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BloodboundEmissiveStrength = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Bloodbound Visuals", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BloodboundMaterialAmount = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Bloodbound Visuals")
	FName BloodboundMaterialScalarParameterName = TEXT("BloodboundAmount");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Bloodbound Visuals")
	FName BloodboundMaterialTintParameterName = TEXT("BloodboundTint");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Bloodbound Visuals")
	FName BloodboundMaterialEmissiveParameterName = TEXT("BloodboundEmissiveStrength");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (ToolTip = "Widget class used for this enemy's health bar. Leave empty to hide health bars for this enemy type."))
	TSubclassOf<UEnemyHealthBarWidget> HealthBarWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AdvancedDisplay, ToolTip = "Vertical offset above the enemy for the health bar widget."))
	float HealthBarHeightOffset = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AdvancedDisplay, ToolTip = "Draw size of the enemy health bar widget."))
	FVector2D HealthBarDrawSize = FVector2D(120.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Mark", meta = (ToolTip = "Widget class shown above this enemy while it is Marked for Death. Leave empty to disable the visual indicator."))
	TSubclassOf<UEnemyMarkIndicatorWidget> MarkIndicatorWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Mark", meta = (ToolTip = "Relative location of the Mark indicator widget above the enemy. Increase Z to move it higher."))
	FVector MarkIndicatorRelativeLocation = FVector(0.0f, 0.0f, 150.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Mark", meta = (ToolTip = "Draw size of the Mark indicator widget in screen-facing widget space."))
	FVector2D MarkIndicatorDrawSize = FVector2D(32.0f, 32.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Mark", meta = (ClampMin = "0.01", UIMin = "0.01", ToolTip = "Extra scale applied to the Mark indicator widget component."))
	float MarkIndicatorScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Budget", meta = (AdvancedDisplay, ToolTip = "Registers this enemy with Unreal's Animation Budget Allocator so large crowds can reduce animation cost."))
	bool bUseAnimationBudgetAllocator = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Budget", meta = (ClampMin = "0.1", UIMin = "0.1", AdvancedDisplay, ToolTip = "Per-enemy animation budget target in milliseconds used when configuring animation budgeting."))
	float AnimationBudgetMs = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Budget", meta = (ClampMin = "0.0", UIMin = "0.0", FormerlySerializedAs = "AnimationBudgetNeverSkipDistance", AdvancedDisplay, ToolTip = "Enemies within this distance from the active player are treated as high significance and should not aggressively skip animation."))
	float AnimationBudgetHighSignificanceDistance = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Budget", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay, ToolTip = "Distance at which enemy animation significance falls to its lowest value for budgeting."))
	float AnimationBudgetMaxSignificanceDistance = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Budget", meta = (AdvancedDisplay, ToolTip = "Uses fixed skeletal mesh bounds for enemies when helpful for animation budgeting/culling stability."))
	bool bUseFixedSkelBoundsForEnemies = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Enemy movement speed used by the lightweight movement system."))
	float MoveSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Distance from the target where this enemy stops moving closer."))
	float StopDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Crowd Spread", meta = (AdvancedDisplay, ToolTip = "Adds a small deterministic spread bias so enemies do not all choose the exact same direct chase line."))
	bool bUseCrowdSpread = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Crowd Spread", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0", AdvancedDisplay, ToolTip = "Strength of each enemy's stable sideways crowd spread bias. 0 disables spread; 1 is strongest."))
	float CrowdSpreadStrength = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Crowd Spread", meta = (AdvancedDisplay, ToolTip = "Draws/logs crowd spread behavior for debugging this enemy's movement direction."))
	bool bDebugCrowdSpread = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Separation", meta = (AdvancedDisplay, ToolTip = "Enables lightweight enemy separation so crowds try to keep space between nearby enemies."))
	bool bUseEnemySeparation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Separation", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay, ToolTip = "Radius used to search for nearby enemies that should push this enemy away."))
	float SeparationRadius = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Separation", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay, ToolTip = "Strength of the separation push away from nearby enemies."))
	float SeparationStrength = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Separation", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay, ToolTip = "Maximum amount the separation steering can contribute to final movement direction."))
	float MaxSeparationContribution = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Separation", meta = (ClampMin = "0.01", UIMin = "0.01", AdvancedDisplay, ToolTip = "Seconds between separation queries for this enemy. Higher values are cheaper but less responsive."))
	float SeparationUpdateInterval = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Rotation", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Maximum yaw rotation speed, in degrees per second, when turning toward the current target."))
	float EnemyRotationSpeed = 720.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.01", UIMin = "0.01", AdvancedDisplay, ToolTip = "Seconds between enemy behavior updates such as target chasing decisions. Higher values are cheaper but less responsive."))
	float BehaviorUpdateInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Path Fallback", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay, ToolTip = "How long this enemy must be blocked before trying the lightweight NavMesh path fallback. 0 allows immediate fallback."))
	float PathFallbackBlockedTime = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Path Fallback", meta = (ClampMin = "0.1", UIMin = "0.1", AdvancedDisplay, ToolTip = "Minimum seconds between path requests while this enemy is using obstacle fallback."))
	float PathFallbackRepathInterval = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Path Fallback", meta = (ClampMin = "1.0", UIMin = "1.0", AdvancedDisplay, ToolTip = "Distance from a path waypoint at which the enemy advances to the next waypoint."))
	float PathWaypointAcceptanceRadius = 125.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Path Fallback", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay, ToolTip = "Target movement distance that forces the fallback path to refresh."))
	float PathTargetRepathDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Path Fallback", meta = (ClampMin = "0.05", UIMin = "0.05", AdvancedDisplay, ToolTip = "Seconds between cheap direct-path checks while using obstacle fallback."))
	float DirectPathCheckInterval = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Path Fallback", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay, ToolTip = "Minimum target distance before this enemy is allowed to use the obstacle path fallback."))
	float MinPathFallbackTargetDistance = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Path Fallback", meta = (AdvancedDisplay, ToolTip = "NavMesh projection extent used when requesting lightweight obstacle fallback paths."))
	FVector PathFallbackProjectionExtent = FVector(250.0f, 250.0f, 500.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ToolTip = "Death animation montage played when this enemy dies. Actor cleanup still happens after Death Destroy Delay."))
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy", meta = (ToolTip = "Current actor this enemy is trying to chase or attack. Usually the active player character."))
	TObjectPtr<AActor> CurrentTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ToolTip = "Current actor this enemy is trying to chase or attack. Usually the active player character."))
	bool bIsDead = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Mark", meta = (ToolTip = "Whether this enemy currently has the Marked for Death state."))
	bool bIsMarked = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Stress Test", meta = (ToolTip = "True when this enemy was spawned by the stress-test tooling instead of normal gameplay spawning."))
	bool bIsStressTestEnemy = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Stress Test", meta = (ToolTip = "True when this stress-test enemy has combat disabled."))
	bool bStressTestDisableCombat = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Stress Test", meta = (ToolTip = "True when this stress-test enemy ignores damage/death."))
	bool bStressTestInvulnerable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay, ToolTip = "Seconds after death before the enemy actor is destroyed. Allows the death montage to remain visible."))
	float DeathDestroyDelay = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0", UIMin = "0", ToolTip = "XP value awarded through the spawned experience pickup when this enemy dies."))
	int32 XPReward = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Bloodbound", meta = (ClampMin = "0", UIMin = "0", ToolTip = "Blood Shrine progress awarded when this enemy is Bloodbound and dies."))
	int32 BloodValue = 1;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Bloodbound")
	bool bIsBloodbound = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Rewards")
	bool bDropsXP = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Bloodbound")
	float BloodboundHealthMultiplier = 1.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Bloodbound")
	float BloodboundDamageMultiplier = 1.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Bloodbound")
	float BloodboundMovementSpeedMultiplier = 1.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Objective")
	EPlayerAttackSource RequiredPlayerAttackSource = EPlayerAttackSource::Other;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Objective")
	bool bGameplaySuspended = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BloodboundDynamicMaterialInstances;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BloodboundOverlayDynamicMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PreBloodboundOverlayMaterial;

	float PreBloodboundMaxHealth = 0.0f;
	float PreBloodboundMoveSpeed = 0.0f;
	bool bPreBloodboundDropsXP = true;
	bool bHasPreBloodboundState = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards", meta = (ToolTip = "Experience pickup actor class spawned when this enemy dies. Leave empty for no XP pickup."))
	TSubclassOf<AExperiencePickup> ExperiencePickupClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay, ToolTip = "Random horizontal scatter radius for the XP pickup spawned at this enemy's death location."))
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
	virtual void CapturePreBloodboundState();
	virtual void RestorePreBloodboundState();
	void ActivateBloodboundVisuals();
	void DeactivateBloodboundVisuals();
	void InitializeHealthBar();
	void UpdateHealthBarVisibility(float HealthPercent);
	void HideHealthBar();
	void InitializeMarkIndicator();
	void UpdateMarkIndicatorVisibility();
	void HideMarkIndicator();
	void ConfigureEnemyCapsuleCollisionDefaults();
	void SnapToGroundBeforeLightweightMovement();
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
	void UpdateObstaclePathFallback();
	bool TryBuildObstaclePath();
	bool TryGetPathFallbackSteeringDirection(FVector& OutSteeringDirection);
	bool IsDirectPathToTargetClear() const;
	bool IsPathFallbackRouteClearToLocation(const FVector& Location) const;
	bool ProjectPathFallbackLocation(const FVector& Location, FVector& OutProjectedLocation) const;
	void ClearObstaclePath();
	FVector ApplyCrowdSpreadToDirection(const FVector& DirectDirection) const;
	FVector ApplyEnemySeparationToDirection(const FVector& MovementDirection) const;
	void UpdateCachedEnemySeparation();
	void RequestEnemyMovement(const FVector& WorldDirection);
	void StopEnemyMovement();
	void DisableNativeCharacterMovement();
	bool IsEnemyAnimationProfilingDisabled() const;
	void UpdateAnimationProfilingState();
	void InitializeAnimationBudgeting();
	void UpdateAnimationBudgetSignificance();
	void LogAnimationBudgetSetup(USkeletalMeshComponentBudgeted* BudgetedMesh, bool bAllocatorEnabled) const;
	virtual bool ShouldForceHighAnimationBudgetSignificance() const;
	virtual bool ShouldUseWorldHealthBar() const { return true; }
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

	UFUNCTION()
	void HandleStatusStacksChanged(EEnemyStatusEffect Status, int32 StackCount);
	void InitializeStatusIndicators();
	void UpdateStatusIndicatorLayout();

	UPROPERTY()
	TObjectPtr<UCharacterManagerComponent> ObservedCharacterManager;

	UPROPERTY()
	TObjectPtr<ASurvivorPlayerController> CachedSurvivorController;

	UPROPERTY()
	TObjectPtr<UExperienceComponent> CachedPlayerExperienceComponent;

	FTimerHandle BehaviorUpdateTimerHandle;
	FTimerHandle SeparationUpdateTimerHandle;
	FVector DesiredMovementDirection = FVector::ZeroVector;
	FVector DesiredDirectMovementDirection = FVector::ZeroVector;
	FVector CachedEnemySeparationVector = FVector::ZeroVector;
	TArray<FVector> ObstaclePathPoints;
	FVector LastPathTargetLocation = FVector::ZeroVector;
	float CrowdSpreadBias = 0.0f;
	float BlockedByWorldGeometryStartTime = -1.0f;
	float NextPathFallbackRequestTime = 0.0f;
	float NextDirectPathCheckTime = 0.0f;
	int32 CurrentObstaclePathIndex = INDEX_NONE;
	float PathFallbackRequestJitter = 0.0f;
	bool bHasDesiredMovementDirection = false;
	bool bAnimationDisabledForProfiling = false;
	bool bCachedAnimationProfilingDisabled = false;
	bool bAnimationBudgetInitialized = false;
	bool bExperiencePickupSpawned = false;
};
