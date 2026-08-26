// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CharacterBase.h"
#include "Components/ActorComponent.h"
#include "AutoAttackComponent.generated.h"

class UAnimMontage;
class AAttackProjectileBase;
class AEnemyBase;
class ANinjaCharacter;
class ASamuraiCharacter;
class ASamuraiBladeWave;
class USoundBase;
class UUpgradeDefinition;
enum class EPlayerAttackSource : uint8;

UENUM(BlueprintType)
enum class EAutoAttackSource : uint8
{
	NormalAutoAttack,
	DoubleCut,
	Assist,
	Other
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAutoAttack, UAutoAttackComponent*, AttackComponent, EAutoAttackSource, AttackSource);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCleaverTransfer, FVector, FromLocation, FVector, ToLocation, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDuelistTargetChanged, AEnemyBase*, OldTarget, AEnemyBase*, NewTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDuelistStackChanged, int32, NewStackCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDeathblowTriggered, FVector, Location, float, Radius, float, Damage);

UENUM(BlueprintType)
enum class ESamuraiTechnique : uint8
{
	None,
	Cleaver,
	Duelist,
	Deathblow
};

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

	UFUNCTION(BlueprintCallable, Category = "Auto Attack")
	void SetAutoAttackEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Auto Attack")
	bool IsAutoAttackEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Auto Attack|Trace")
	void PerformAttackTrace();

	UFUNCTION(BlueprintCallable, Category = "Auto Attack|Projectile")
	void SpawnAutoAttackProjectile();

	UPROPERTY(BlueprintAssignable, Category = "Auto Attack|Samurai Technique")
	FOnCleaverTransfer OnCleaverTransfer;

	UPROPERTY(BlueprintAssignable, Category = "Auto Attack|Samurai Technique")
	FOnDuelistTargetChanged OnDuelistTargetChanged;

	UPROPERTY(BlueprintAssignable, Category = "Auto Attack|Samurai Technique")
	FOnDuelistStackChanged OnDuelistStackChanged;

	UPROPERTY(BlueprintAssignable, Category = "Auto Attack|Samurai Technique")
	FOnDeathblowTriggered OnDeathblowTriggered;

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
	int32 GetEffectiveProjectileCount() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	int32 GetEffectiveProjectilePierceBonus() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	int32 GetEffectiveProjectileBounceBonus() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	int32 GetEffectiveProjectileSplitBonus() const;

	// Fires one Ninja projectile volley from an external origin without advancing
	// normal attack-cycle systems (Fan of Blades, Blade Cascade, assists, or swap synergies).
	bool SpawnShadowCloneVolley(const FVector& SpawnLocation, float SearchRange, bool& bExtraProjectileOnRight);

	UAnimMontage* GetAttackMontageForShadowClone() const { return AttackMontage; }

	void RegisterKunaiFired();

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	float GetBaseAttackInterval() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	float GetBaseAttackDamage() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	float GetBaseAttackRadius() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	float GetBaseProjectileSpeed() const;

	UFUNCTION(BlueprintCallable, Category = "Auto Attack|Cooldown")
	bool ReduceRemainingAttackCooldown(float Percent);

	UPROPERTY(BlueprintAssignable, Category = "Auto Attack")
	FOnAutoAttack OnAutoAttack;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float AttackInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float ReadyTargetCheckInterval = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Animation", meta = (ToolTip = "When enabled, attack montages can play faster for very short attack intervals. Montages are never slowed below normal speed by attack interval."))
	bool bScaleMontageWithAttackInterval = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Animation", meta = (ClampMin = "0.01", UIMin = "0.01", ToolTip = "Maximum montage play rate allowed when attack interval scaling speeds up an attack animation."))
	float MaxAttackMontagePlayRate = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack")
	bool bAutoAttackEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Fan of Blades", meta = (ToolTip = "Optional montage used on the Ninja attack that triggers Fan of Blades. If unset, the normal attack montage is used."))
	TObjectPtr<UAnimMontage> FanOfBladesAttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Double Cut", meta = (ToolTip = "Optional Samurai follow-up montage used when Double Cut triggers. If unset, the normal attack montage is used."))
	TObjectPtr<UAnimMontage> DoubleCutMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Samurai Attack", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", ToolTip = "Damage multiplier applied to every valid Samurai melee target other than the single best-aligned primary target."))
	float SecondaryTargetDamageMultiplier = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Samurai Technique|Cleaver", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float CleaverChainRadius = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Samurai Technique|Cleaver", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxCleaverChainTargets = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Samurai Technique|Duelist", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DuelistDamagePerStack = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Samurai Technique|Deathblow", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DeathblowDamageMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Samurai Technique|Deathblow", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DeathblowBaseRadius = 450.0f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Projectile", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "deg", ToolTip = "Angular spacing between neighboring kunai in a normal centered volley."))
	float KunaiSpreadAngle = 10.0f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack|Trace", meta = (ToolTip = "Draws the melee attack trace shape for debugging."))
	bool bDebugAttackTrace = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Audio", meta = (ToolTip = "2D feedback sound played once when this melee attack trace damages at least one valid enemy. Leave empty for no impact sound."))
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Double Cut", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Number of normal Samurai melee attacks required before Double Cut triggers."))
	int32 DoubleCutPrimaryAttackCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Fan of Blades", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Number of Ninja attacks required before Fan of Blades triggers."))
	int32 FanOfBladesAttackInterval = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Fan of Blades", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Number of multi-target kunai spawned when Fan of Blades triggers."))
	int32 FanOfBladesProjectileCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Fan of Blades", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm", ToolTip = "Lateral spacing between Fan of Blades kunai when all projectiles converge on one target."))
	float FanSingleTargetSpreadSpacing = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Blade Wave")
	TSubclassOf<ASamuraiBladeWave> BladeWaveClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Blade Wave", meta=(ClampMin="1.0"))
	float BladeWaveTravelDistance = 650.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Blade Wave", meta=(ClampMin="1.0"))
	float BladeWaveSpeed = 1400.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Blade Wave", meta=(ClampMin="1.0"))
	float BladeWaveBaseWidth = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Blade Wave", meta=(ClampMin="0.0"))
	float BladeWaveDamageMultiplier = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Blade Wave")
	float CrossingBladeSideAngle = 30.0f;

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
	UAnimMontage* GetMontageForNextAttack() const;
	float CalculateAttackMontagePlayRate(const UAnimMontage* Montage) const;
	float GetExpectedAttackMontageDuration() const;
	float GetExpectedDoubleCutFollowUpDuration() const;
	void HandleAttackMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	bool StartTargetedAttack();
	bool CanExecuteAttackInCurrentMode() const;
	bool CanStartAttackNow() const;
	bool IsOwningPlayerDead() const;
	bool IsCursorTargetingEnabledForNormalAttack() const;
	bool ResolveCursorAttackDirection(FVector& OutDirection) const;
	bool TryConsumeAttackNotify();
	bool ExecuteMeleeAttackTrace();
	void HandleSamuraiMomentum(int32 KilledEnemyCount);
	ESamuraiTechnique GetActiveSamuraiTechnique() const;
	float ResolveDuelistPrimaryDamage(AEnemyBase* PrimaryTarget, float BasePrimaryDamage);
	void ResetDuelistState();
	void ExecuteCleaverChain(AEnemyBase* OriginalPrimaryTarget, const FVector& OriginLocation, float RemainingDamage, EPlayerAttackSource AttackSource);
	AEnemyBase* FindCleaverTarget(const FVector& SearchLocation, const TSet<AEnemyBase*>& VisitedTargets, EPlayerAttackSource AttackSource) const;
	void ExecuteDeathblow(AEnemyBase* DeadPrimaryTarget, const FVector& OriginLocation, float ResolvedPrimaryDamage, EPlayerAttackSource AttackSource, bool bApplyMarkedBlade);
	void RegisterDoubleCutPrimaryAttack();
	bool HasDoubleCutUpgrade() const;
	const UUpgradeDefinition* GetMomentumUpgrade() const;
	bool HasFanOfBladesUpgrade() const;
	bool WillNextNinjaAttackTriggerFanOfBlades() const;
	void RegisterNinjaAttackForFanOfBlades(const FVector& SpawnLocation, float Damage, float Speed, int32 AdditionalPierceCount);
	void SpawnFanOfBladesVolley(const FVector& SpawnLocation, float Damage, float Speed, int32 AdditionalPierceCount);
	void SpawnFanOfBladesConvergenceGroup(AEnemyBase* Target, int32 AssignedProjectileCount, const FVector& SpawnLocation, float Damage, float Speed, int32 AdditionalPierceCount);
	void SpawnBladeWavesForAttack(float ResolvedPrimaryDamage);
	const UUpgradeDefinition* GetBladeCascadeUpgrade() const;
	int32 ConsumeBladeCascadeBonusForNormalVolley(int32 NormalProjectileCount);
	bool WillNextSamuraiAttackTriggerDoubleCut() const;
	bool AcquireDoubleCutFollowUpTarget();
	void StartDoubleCutFollowUp();
	void ConsumePendingDoubleCutFollowUp();
	void HandleDoubleCutMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void SpawnProjectileInstance(const FVector& SpawnLocation, const FVector& ProjectileDirection, float Damage, float Speed, int32 AdditionalPierceCount, bool bRegisterAttackCycle = true);
	AEnemyBase* FindNearestEnemyTarget() const;
	AEnemyBase* FindBestMeleeTarget(const FVector& SearchLocation, float SearchRadius) const;
	float ScoreMeleeTarget(AEnemyBase* Candidate, const TArray<AEnemyBase*>& Candidates, const FVector& SearchLocation, float SearchRadius, int32& OutClusterCount, float& OutDistancePenalty, float& OutImmediateThreatBonus) const;
	void FindEnemyTargetsSorted(TArray<AEnemyBase*>& OutTargets) const;
	void FindEnemyTargetsSortedFromLocation(const FVector& SearchLocation, float SearchRadius, TArray<AEnemyBase*>& OutTargets) const;
	static void BuildCenteredProjectileSpreadDirections(const FVector& BaseDirection, int32 ProjectileCount, float SpreadAngleDegrees, bool bExtraProjectileOnRight, TArray<FVector>& OutDirections);
	FVector GetProjectileSpawnLocation() const;
	FVector GetEnemyAimLocation(const AEnemyBase* Enemy) const;
	bool CanAutoAttack() const;

	UPROPERTY()
	TObjectPtr<ACharacterBase> OwnerCharacter;

	TWeakObjectPtr<AEnemyBase> CurrentAttackTarget;
	FVector ActiveAttackDirection = FVector::ZeroVector;
	TWeakObjectPtr<AEnemyBase> DuelistTarget;
	int32 DuelistStackCount = 0;

	FTimerHandle AttackTimerHandle;
	double LastAttackStartTime = -DBL_MAX;
	double NextAttackReadyTime = 0.0;
	float AttackIntervalAtLastAttackStart = 1.0f;
	int32 AttackSequence = 0;
	int32 ActiveAttackSequence = 0;
	int32 DoubleCutPrimaryAttackCounter = 0;
	int32 FanOfBladesAttackCounter = 0;
	int32 BladeCascadeKunaiProgress = 0;
	int32 CrossingBladesAttackCounter = 0;
	float LastResolvedPrimaryAttackDamage = 0.0f;
	bool bIsAttacking = false;
	bool bAttackNotifyConsumed = false;
	bool bActiveAttackIsAssist = false;
	bool bActiveAttackTriggersFanOfBlades = false;
	bool bBladeCascadeReady = false;
	bool bDoubleCutFollowUpActive = false;
	bool bDoubleCutFollowUpPending = false;
	bool bNormalVolleyExtraProjectileOnRight = true;

	UPROPERTY()
	TObjectPtr<UAnimMontage> ActiveAttackMontage;
};
