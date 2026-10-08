// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CharacterBase.h"
#include "Components/ActorComponent.h"
#include "ImpactFeedback.h"
#include "AutoAttackComponent.generated.h"

class UAnimMontage;
class AAttackProjectileBase;
class AEnemyBase;
class ANinjaCharacter;
class ASamuraiCharacter;
class ASamuraiBladeWave;
class USoundBase;
class UNiagaraSystem;
class UMaterialInterface;
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

UENUM(BlueprintType)
enum class ESamuraiTechnique : uint8
{
	None,
	Cleaver,
	Duelist,
	Deathblow
};

USTRUCT()
struct FAutoAttackRunState
{
	GENERATED_BODY()
	UPROPERTY() int32 DoubleCutCounter = 0;
	UPROPERTY() bool bDoubleCutReady = false;
	UPROPERTY() int32 FanOfBladesCounter = 0;
	UPROPERTY() int32 BladeCascadeProgress = 0;
	UPROPERTY() int32 CrossingBladesCounter = 0;
    UPROPERTY() int32 IaijutsuCounter = 0;
	UPROPERTY() bool bBladeCascadeReady = false;
	UPROPERTY() bool bExtraProjectileOnRight = true;
	UPROPERTY() bool bGrandEntranceReady = false;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API UAutoAttackComponent : public UActorComponent
{
	friend class USwapPresentationComponent;
	friend class FSwapFreezeTest;
	friend class FNinjaAlternatingThrowTest;
	GENERATED_BODY()
	friend class FImpactFeedbackTest;
	friend class FEnemyPushbackTest;
	friend class FDoubleCut360Test;
	friend class FGrandEntranceTest;
	friend class FSamuraiBuildsTest;
    friend class FCrescentBuildsTest;
	friend class FTagTeamRegressionTest;
 friend class UNinjaBuildComponent;
 friend class ANinjaBuildProjectile;
	friend class FNinjaBuildsTest;
    friend class FFangBuildsTest;
 friend class FBarrageBuildsTest;
 friend class FShurikenBuildsTest;

public:
	UAutoAttackComponent();
    void SpawnIaijutsuDash(FVector Origin, FVector Destination);
    bool SpawnIaijutsuSlashes(FVector Origin, FVector Direction, float Distance, float ChargeDuration, bool bNormalAttack, TSharedPtr<int32> ChainBudget = nullptr, float DamageMultiplier = 1.f);

	void CaptureRunState(FAutoAttackRunState& OutState) const;
	void RestoreRunState(const FAutoAttackRunState& State);
	void ArmGrandEntranceAfterSwap();

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

	UFUNCTION(BlueprintCallable, Category = "Samurai|Melee Trace")
	void PerformAttackTrace();

	UFUNCTION(BlueprintCallable, Category = "Ninja|Projectiles")
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
	int32 GetEffectiveProjectileCount() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	int32 GetEffectiveProjectilePierceBonus() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	int32 GetEffectiveProjectileBounceBonus() const;

	UFUNCTION(BlueprintPure, Category = "Auto Attack|Stats")
	int32 GetEffectiveProjectileSplitBonus() const;

	// Fires one Ninja projectile volley from an external origin without advancing
	// player volley counters, assists, or swap synergies.
	bool SpawnShadowCloneVolley(const FVector& SpawnLocation, float SearchRange, bool& bExtraProjectileOnRight, int32* CloneVolley=nullptr);

	UAnimMontage* GetAttackMontageForShadowClone() const { return AttackMontage; }

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Animation", meta = (ToolTip = "When enabled, attack montages can play faster for very short attack intervals. Montages are never slowed below normal speed by attack interval."))
	bool bScaleMontageWithAttackInterval = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Animation", meta = (ClampMin = "0.01", UIMin = "0.01", ToolTip = "Maximum montage play rate allowed when attack interval scaling speeds up an attack animation."))
	float MaxAttackMontagePlayRate = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack")
	bool bAutoAttackEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Ninja alternates this left-hand throw with AttackMontage; empty preserves a single montage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Animation")
	TObjectPtr<UAnimMontage> AlternateAttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Samurai|Double Cut", meta = (ToolTip = "Optional Samurai follow-up montage used when Double Cut triggers. If unset, the normal attack montage is used."))
	TObjectPtr<UAnimMontage> DoubleCutMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Samurai|Melee Damage", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", ToolTip = "Damage multiplier applied to every valid Samurai melee target other than the single best-aligned primary target."))
	float SecondaryTargetDamageMultiplier = 0.30f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ninja|Projectiles")
	TSubclassOf<AAttackProjectileBase> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ninja|Projectiles", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ProjectileSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ninja|Projectiles", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "deg", ToolTip = "Angular spacing between neighboring kunai in a normal centered volley."))
	float KunaiSpreadAngle = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ninja|Projectiles")
	FName ProjectileSpawnSocket = TEXT("ProjectileSocket");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ninja|Projectiles", meta = (ToolTip = "Release socket for the alternate left-hand montage."))
	FName AlternateProjectileSpawnSocket = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ninja|Projectiles")
	FVector ProjectileSpawnOffset = FVector(80.0f, 0.0f, 60.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Samurai|Melee Trace", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackRange = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Samurai|Melee Trace", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackRadius = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Samurai|Melee Trace")
	float AttackForwardOffset = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Samurai|Melee Trace", meta = (ToolTip = "Draws the melee attack trace shape for debugging."))
	bool bDebugAttackTrace = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto Attack|Audio", meta = (ToolTip = "2D feedback sound played once when this melee attack trace damages at least one valid enemy. Leave empty for no impact sound."))
	TObjectPtr<USoundBase> ImpactSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Samurai|Iaijutsu", meta=(ToolTip="Montage played by the real Samurai once per normal Iaijutsu attack, fitted to the charge duration. Instant casts use authored animation speed without delaying damage. Root motion and melee damage notifies cannot move or attack for the player. Empty disables this presentation. Spectral lanes use Attack Montage; dash and cascade lanes do not replay this montage."))
    TObjectPtr<UAnimMontage> IaijutsuMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Samurai|Iaijutsu", meta=(ToolTip="Spawns once at charge completion, centered on the slash lane. Optional Niagara parameters: User.StartPosition, User.EndPosition, User.Length, User.Width."))
    TObjectPtr<UNiagaraSystem> IaijutsuHitVFX;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Samurai|Iaijutsu|Path Slashes", meta=(ToolTip="Cosmetic bursts distributed along the final lane when damage resolves, never during charging. Also used by Double Cut, Dash Draw and Death Cascade. Empty disables path slashes without changing the separate Hit VFX."))
    TObjectPtr<UNiagaraSystem> IaijutsuPathSlashVFX;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Samurai|Iaijutsu|Path Slashes", meta=(ClampMin="0", ClampMax="12", ToolTip="Evenly spaced bursts per lane at release. Zero disables them."))
    int32 IaijutsuPathSlashCount = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Samurai|Iaijutsu|Path Slashes", meta=(ClampMin="0", ToolTip="Size relative to the authored effect, multiplied by lane half-width / 100 cm. Uses User.Scale when exposed, otherwise component scale."))
    float IaijutsuPathSlashScale = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Samurai|Iaijutsu|Path Slashes", meta=(ToolTip="Local rotation of each burst relative to the lane direction."))
    FRotator IaijutsuPathSlashRotation = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Samurai|Iaijutsu|Path Slashes", meta=(ClampMin="0", ClampMax="1", Units="s", ToolTip="Delay between cosmetic bursts from the start to the end of the resolved lane. The first burst and all damage occur immediately at release. Zero fires all bursts together."))
    float IaijutsuPathSlashDelay = .05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Samurai|Iaijutsu|Path Slashes", meta=(ClampMin="0", ClampMax="180", ToolTip="Independent random +/- Pitch, Yaw and Roll added to each burst's local Rotation. Zero on an axis disables its randomness. Cosmetic randomness does not affect combat rolls."))
    FRotator IaijutsuPathSlashRotationRandomness = FRotator(20.f, 35.f, 60.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Samurai|Iaijutsu", meta=(ToolTip="Spectral montage at the end of the lane when Endpoint Burst triggers. Presentation only; animation notifies do not deal damage."))
    TObjectPtr<UAnimMontage> IaijutsuEndpointMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Samurai|Iaijutsu", meta=(ToolTip="Empty uses the enemy rectangle indicator with Iaijutsu's color."))
    TObjectPtr<UMaterialInterface> IaijutsuIndicatorMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Samurai|Iaijutsu")
    FLinearColor IaijutsuIndicatorColor = FLinearColor(0.f, .8f, 1.f);
	/** Per-enemy VFX/audio; camera shake is requested once after a successful melee swing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto Attack|Impact")
	FImpactFeedbackData ImpactFeedback;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Samurai|Pushback", meta=(ClampMin="0.0", Units="cm")) float SamuraiPushbackDistance = 25.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Samurai|Pushback", meta=(ClampMin="0.01", Units="s")) float SamuraiPushbackDuration = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Samurai|Double Cut", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Number of normal Samurai melee attacks required before Double Cut triggers."))
	int32 DoubleCutPrimaryAttackCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Samurai|Double Cut", meta = (ClampMin = "0.01", UIMin = "0.01", ToolTip = "Animation-only multiplier applied to the normal Samurai primary montage when that primary is expected to earn or use a stored Double Cut proc."))
	float DoubleCutPrimarySpeedMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Samurai|Weapon Visual Scaling", meta = (ToolTip = "Name of the weapon component scaled around its mesh origin during Samurai melee attacks."))
	FName WeaponVisualComponentName = TEXT("Weapon");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Samurai|Blade Wave")
	TSubclassOf<ASamuraiBladeWave> BladeWaveClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Samurai|Blade Wave", meta=(ToolTip="Presentation montage for normal Crescent wave attacks. Wave emission and cooldown remain timer-driven; melee damage and root motion are disabled."))
	TObjectPtr<UAnimMontage> CrescentMontage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Samurai|Blade Wave", meta=(ToolTip="Alternates with Crescent Montage on successful wave attacks. Empty uses Crescent Montage for every attack."))
	TObjectPtr<UAnimMontage> CrescentAlternateMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Samurai|Blade Wave", meta=(ClampMin="1.0"))
	float BladeWaveTravelDistance = 650.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Samurai|Blade Wave", meta=(ClampMin="1.0"))
	float BladeWaveSpeed = 1400.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Samurai|Blade Wave", meta=(ClampMin="1.0"))
	float BladeWaveBaseWidth = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Samurai|Blade Wave", meta=(ClampMin="0.0"))
	float BladeWaveDamageMultiplier = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Samurai|Blade Wave")
	float CrossingBladeSideAngle = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Samurai|Blade Wave", meta=(ClampMin="0", Units="s", ToolTip="Delay between waves on a Crossing Blades proc. Zero restores simultaneous waves."))
	float CrossingBladeWaveDelay = .12f;

public:
    /** Cosmetic only: Fang flight, rather than animation notifies, owns attack timing. */
    void PlayFangMontage(FRotator Facing);
    void PlayFangSlashMontage();
    /** Return-burst animation. Cosmetic only; has priority over Fang throw poses. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ninja|Fang")
    TObjectPtr<UAnimMontage> FangSlashMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ninja|Fang")
    TObjectPtr<UAnimMontage> FangMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ninja|Fang")
    TObjectPtr<UAnimMontage> FangAlternateMontage;
private:
    UPROPERTY(Transient)
    TObjectPtr<UAnimMontage> ActiveFangMontage;
    bool bNextFangUsesAlternate = false;
    friend class FIaijutsuTest;
    int32 IaijutsuAttackCounter = 0;
    FTimerHandle IaijutsuAssistTimer;
    void RollIaijutsuAssist();
    float GetIaijutsuChargeDuration() const;
    double IaijutsuChargeEndTime = 0;
    const UUpgradeDefinition* GetIaijutsuUpgrade() const;
    bool StartIaijutsuAttack();
    bool ResolveIaijutsuAttackDirection(float Distance, FVector& Direction) const;
    void PlayIaijutsuMontage(float ChargeDuration);
	TArray<FTimerHandle> PendingBladeWaveTimers;
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
	void PlayCrescentMontage();
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
    int32 GetDoubleCutThreshold() const;
    void ExecuteBloodEcho(FVector Origin, FVector Direction, float Damage, float Radius, bool bCircular);
    void RollBloodAssist();
public:
    /** Cascade anticipation/explosion effect; starts when capped Bleed is consumed. */
    UPROPERTY(EditAnywhere, Category="Samurai|Blood Stance|Detonation") TObjectPtr<class UParticleSystem> BloodDetonationVFX;
    /** Seconds from the effect starting until damage resolves at its original location. */
    UPROPERTY(EditAnywhere, Category="Samurai|Blood Stance|Detonation", meta=(ClampMin="0", Units="s")) float BloodDetonationExplosionDelay = 0.f;
    /** Visual radius of the Cascade asset at scale 1. Does not change the damage radius. */
    UPROPERTY(EditAnywhere, Category="Samurai|Blood Stance|Detonation", meta=(ClampMin="1", Units="cm")) float BloodDetonationVFXReferenceRadius = 300.f;
private:
    UPROPERTY(EditAnywhere, Category="Samurai|Blood Stance") TObjectPtr<UNiagaraSystem> BloodEchoVFX;
    UPROPERTY(EditAnywhere, Category="Samurai|Blood Stance", meta=(ClampMin="0.01")) float BloodEchoDelay = .15f;
    bool bBloodCircularAttack = false;

	void RegisterDoubleCutPrimaryAttack();
	bool HasDoubleCutUpgrade() const;

	void SpawnBladeWavesForAttack(float ResolvedPrimaryDamage);

	const UUpgradeDefinition* GetReadyGrandEntranceUpgrade() const;
	float GetGrandEntranceRadius(const UUpgradeDefinition* Upgrade) const;

	bool WillNextSamuraiAttackTriggerDoubleCut() const;
	bool ShouldApplySamuraiPushback() const;
	bool AcquireDoubleCutFollowUpTarget();
	bool StartDoubleCutFollowUp();
	bool ConsumePendingDoubleCutFollowUp();
	void HandleDoubleCutMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	TArray<FTimerHandle> PendingBarrageTimers;
 void SpawnBarrageSequence(FVector Origin,FVector Direction,int32 Count,float Damage,float Speed,int32 Pierce);
 void SpawnProjectileInstance(const FVector& SpawnLocation, const FVector& ProjectileDirection, float Damage, float Speed, int32 AdditionalPierceCount);
	AEnemyBase* FindNearestEnemyTarget() const;
	AEnemyBase* FindBestMeleeTarget(const FVector& SearchLocation, float SearchRadius) const;
	float ScoreMeleeTarget(AEnemyBase* Candidate, const TArray<AEnemyBase*>& Candidates, const FVector& SearchLocation, float SearchRadius, int32& OutClusterCount, float& OutDistancePenalty, float& OutImmediateThreatBonus) const;
	void FindEnemyTargetsSorted(TArray<AEnemyBase*>& OutTargets) const;
	void FindEnemyTargetsSortedFromLocation(const FVector& SearchLocation, float SearchRadius, TArray<AEnemyBase*>& OutTargets) const;
	static void BuildCenteredProjectileSpreadDirections(const FVector& BaseDirection, int32 ProjectileCount, float SpreadAngleDegrees, bool bExtraProjectileOnRight, TArray<FVector>& OutDirections);
	FVector GetProjectileSpawnLocation() const;
	FVector GetEnemyAimLocation(const AEnemyBase* Enemy) const;
	bool CanAutoAttack() const;
	USceneComponent* ResolveWeaponVisualComponent();
	void ApplyAttackWeaponVisualScale();
	void RestoreAttackWeaponVisualScale();

	UPROPERTY()
	TObjectPtr<ACharacterBase> OwnerCharacter;

	TWeakObjectPtr<AEnemyBase> CurrentAttackTarget;
	FVector ActiveAttackDirection = FVector::ZeroVector;

	FTimerHandle AttackTimerHandle;
	double LastAttackStartTime = -DBL_MAX;
	double NextAttackReadyTime = 0.0;
	float AttackIntervalAtLastAttackStart = 1.0f;
	int32 AttackSequence = 0;
	int32 ActiveAttackSequence = 0;
	int32 DoubleCutPrimaryAttackCounter = 0;
	int32 CrossingBladesAttackCounter = 0;
	float LastResolvedPrimaryAttackDamage = 0.0f;
	bool bIsAttacking = false;
	bool bAttackNotifyConsumed = false;
	bool bActiveAttackIsAssist = false;
	bool bGrandEntranceReady = false;
	bool bDoubleCutReady = false;
	bool bDoubleCutFollowUpActive = false;
	bool bDoubleCutFollowUpPending = false;
	bool bNormalVolleyExtraProjectileOnRight = true;

	UPROPERTY()
	TObjectPtr<UAnimMontage> ActiveAttackMontage;
	bool bNextAttackUsesAlternate = false;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveCrescentMontage;
	bool bNextCrescentUsesAlternate = false;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveIaijutsuMontage;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> WeaponVisualComponent;

	FVector OriginalWeaponVisualComponentRelativeScale = FVector::OneVector;
	bool bAttackWeaponVisualScaleApplied = false;
};
