#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimEnums.h"
#include "EnemyBase.h"
#include "FinalBossBase.generated.h"

class ABossGroundTelegraph;
class ASurvivorPlayerController;
class UAnimMontage;
class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EFinalBossAttack : uint8 { None, ForwardCleave, PointBlankAoE, LongDash, GroundPursuit };

UENUM(BlueprintType)
enum class EFinalBossState : uint8 { Idle, Windup, AttackActive, Recovery, Cooldown, PhaseTransition, Dead };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBossDefeated, AFinalBossBase*, Boss);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBossAttackEvent, EFinalBossAttack, Attack);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBossPhase2Event);

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AFinalBossBase : public AEnemyBase
{
	GENERATED_BODY()
public:
	AFinalBossBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	UFUNCTION(BlueprintCallable, Category="Boss|Testing") void StartBossCombat();
	UFUNCTION(BlueprintCallable, Category="Boss|Testing") void StopBossCombat();
	UFUNCTION(BlueprintCallable, Category="Boss|Testing") void DebugForceAttack(EFinalBossAttack Attack = EFinalBossAttack::ForwardCleave);
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Boss|Testing") void DebugForceForwardCleave();
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Boss|Testing") void ForcePhase2();
	UFUNCTION(BlueprintPure, Category="Boss") EFinalBossState GetBossState() const { return BossState; }
	UFUNCTION(BlueprintPure, Category="Boss") EFinalBossAttack GetCurrentAttack() const { return CurrentAttack; }
	UFUNCTION(BlueprintPure, Category="Boss|Phase 2") bool IsPhase2Active() const { return bPhase2Active; }
	UFUNCTION(BlueprintPure, Category="Boss") FText GetBossDisplayName() const { return BossDisplayName; }
	UFUNCTION(BlueprintCallable, Category="Boss|Animation") void HandleBossTelegraphStart();
	UFUNCTION(BlueprintCallable, Category="Boss|Animation") void HandleBossAttackExecute();
	UFUNCTION(BlueprintCallable, Category="Boss|Animation") void HandleBossTelegraphEnd();
	UFUNCTION(BlueprintCallable, Category="Boss|Animation") void HandleBossSpawnGroundCircle();

	UPROPERTY(BlueprintAssignable, Category="Boss|Events") FBossDefeated OnBossDefeated;
	UPROPERTY(BlueprintAssignable, Category="Boss|Events") FBossAttackEvent OnBossAttackStarted;
	UPROPERTY(BlueprintAssignable, Category="Boss|Events") FBossAttackEvent OnBossAttackImpact;
	UPROPERTY(BlueprintAssignable, Category="Boss|Events") FBossPhase2Event OnBossPhase2Started;
	UFUNCTION(BlueprintImplementableEvent, Category="Boss|Phase 2") void BP_OnBossPhase2TransitionStarted();
	UFUNCTION(BlueprintImplementableEvent, Category="Boss|Phase 2") void BP_OnBossPhase2Started();

protected:
	virtual void UpdateEnemyBehavior(float DeltaSeconds) override;
	virtual bool ShouldSkipMovement() const override;
	virtual bool ShouldUseWorldHealthBar() const override { return false; }
	virtual void HandleDeath() override;
	virtual bool ApplyPlayerDamage(float DamageAmount, EPlayerAttackSource AttackSource) override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|General", meta=(ClampMin="1.0")) float BossMaxHealth = 5000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|General", meta=(ClampMin="0.0")) float BossBaseDamage = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|General", meta=(ClampMin="0.0")) float BossMoveSpeed = 350.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|General", meta=(ClampMin="0.0")) float AttackCooldown = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|General") FText BossDisplayName = FText::FromString(TEXT("FINAL BOSS"));
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Testing") bool bAutoStartCombat = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Testing") bool bDebugAttackTelegraphs = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2", meta=(ClampMin="0.0", ClampMax="1.0")) float Phase2HealthThreshold = 0.50f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2", meta=(ClampMin="0.0")) float Phase2FallbackTransitionDuration = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2") bool bPhase2InvulnerableDuringTransition = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2") TObjectPtr<UAnimMontage> Phase2TransitionMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2", meta=(ClampMin="0.0")) float Phase2AttackCooldown = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Forward Cleave", meta=(ClampMin="1.0")) float ForwardAttackLength = 900.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Forward Cleave", meta=(ClampMin="1.0")) float ForwardAttackWidth = 250.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Forward Cleave", meta=(ClampMin="0.01")) float ForwardAttackTelegraphDuration = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Forward Cleave", meta=(ClampMin="0.0")) float ForwardAttackDamage = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Forward Cleave|Selection", meta=(ClampMin="0.0")) float ForwardCleaveMinStartDistance = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Forward Cleave|Selection", meta=(ClampMin="0.0")) float ForwardCleaveMaxStartDistance = 1200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Forward Cleave|Timing", meta=(ClampMin="0.0")) float ForwardCleaveRecoveryDuration = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2|Forward Cleave") TObjectPtr<UAnimMontage> Phase2ForwardCleaveMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2|Forward Cleave", meta=(ClampMin="0.0", ClampMax="180.0")) float Phase2ForwardCleaveSideAngle = 35.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2|Forward Cleave", meta=(ClampMin="0.0")) float Phase2ForwardCleaveDamageMultiplier = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2|Forward Cleave", meta=(ClampMin="0.0")) float Phase2ForwardCleaveRecoveryDuration = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Point Blank AoE", meta=(ClampMin="1.0")) float AoERadius = 600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Point Blank AoE", meta=(ClampMin="0.0")) float AoEDamage = 25.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Point Blank AoE|Selection", meta=(ClampMin="0.0")) float PointBlankAOEMinStartDistance = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Point Blank AoE|Selection", meta=(ClampMin="0.0")) float PointBlankAOEMaxStartDistance = 700.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Point Blank AoE|Timing", meta=(ClampMin="0.0")) float PointBlankAOERecoveryDuration = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2|Point Blank AoE") TObjectPtr<UAnimMontage> Phase2AOEMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2|Point Blank AoE", meta=(ClampMin="1.0")) float Phase2AoERadius = 425.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2|Point Blank AoE", meta=(ClampMin="0.01")) float Phase2AOEMontagePlayRate = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2|Point Blank AoE", meta=(ClampMin="0.0")) float Phase2AOEDamageMultiplier = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2|Point Blank AoE", meta=(ClampMin="0.0")) float Phase2PointBlankAOERecoveryDuration = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Long Dash", meta=(ClampMin="1.0")) float DashDistance = 1400.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Long Dash", meta=(ClampMin="1.0")) float DashWidth = 220.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Long Dash", meta=(ClampMin="0.01")) float DashTravelDuration = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Long Dash", meta=(ClampMin="0.0")) float DashDamage = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Long Dash|Selection", meta=(ClampMin="0.0")) float LongDashMinStartDistance = 350.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Long Dash|Selection", meta=(ClampMin="0.0")) float LongDashMaxStartDistance = 1800.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Long Dash|Timing", meta=(ClampMin="0.0")) float LongDashRecoveryDuration = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2|Long Dash") TObjectPtr<UAnimMontage> Phase2LongDashMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2|Long Dash", meta=(ClampMin="0.0")) float Phase2LongDashRecoveryDuration = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit", meta=(ClampMin="0.01")) float GroundCircleTelegraphDuration = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit", meta=(ClampMin="1.0")) float GroundCircleRadius = 180.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit", meta=(ClampMin="0.0")) float GroundCircleDamage = 18.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit|Placement", meta=(ClampMin="0.0")) float GroundPursuitMinSpawnRadius = 150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit|Placement", meta=(ClampMin="0.0")) float GroundPursuitMaxSpawnRadius = 500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit|Placement", meta=(ClampMin="0.0")) float GroundPursuitMinCircleSeparation = 180.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit|Placement", meta=(ClampMin="1", UIMin="1")) int32 GroundPursuitMaxSpawnAttempts = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit|Selection", meta=(ClampMin="0.0")) float GroundPursuitMinStartDistance = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit|Selection", meta=(ClampMin="0.0")) float GroundPursuitMaxStartDistance = 3000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit|Timing", meta=(ClampMin="0.0")) float GroundPursuitRecoveryDuration = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2|Ground Pursuit", meta=(ClampMin="1", UIMin="1")) int32 Phase2GroundPursuitCirclesPerNotify = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase 2|Ground Pursuit", meta=(ClampMin="0.0")) float Phase2GroundPursuitRecoveryDuration = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit") TSubclassOf<ABossGroundTelegraph> GroundTelegraphClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Telegraph") TObjectPtr<UMaterialInterface> RectangleTelegraphMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Telegraph") TObjectPtr<UMaterialInterface> CircleTelegraphMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Animation") TObjectPtr<UAnimMontage> ForwardCleaveMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Animation") TObjectPtr<UAnimMontage> PointBlankAOEMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Animation") TObjectPtr<UAnimMontage> LongDashMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Animation") TObjectPtr<UAnimMontage> GroundPursuitMontage;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Telegraph") TObjectPtr<UDecalComponent> RectangleTelegraph;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Telegraph") TObjectPtr<UStaticMeshComponent> CircleTelegraph;

private:
	void ChooseAttack();
	bool IsAttackWithinStartDistance(EFinalBossAttack Attack, float PlayerDistance) const;
	float GetRecoveryDurationForAttack(EFinalBossAttack Attack) const;
	float GetCurrentAttackCooldown() const;
	void QueuePhase2Transition();
	void BeginPhase2Transition();
	void CompletePhase2Transition();
	void HandlePhase2TransitionMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	UFUNCTION() void HandleBossHealthChangedForPhase(float CurrentHealth, float MaxHealth, float HealthPercent);
	void ConfigurePhase2CleaveDirection();
	void ReacquireDashDirection();
	void BeginAttack(EFinalBossAttack Attack);
	void BeginRecovery();
	void ShowRectangleTelegraph(float Length, float Width, const FVector& AttackDirection);
	void ShowCircleTelegraph(float Radius);
	void HideAttackTelegraphs();
	void StartRectangleFill();
	void StopRectangleFill();
	void UpdateRectangleFill();
	void SetRectangleFill(float Alpha);
	float GetSecondsUntilExecuteNotify() const;
	void DamagePlayerInRectangle(float Length, float Width, float Damage, const FVector& AttackDirection);
	void DamagePlayerInCircle(float Radius, float Damage);
	void StartDash();
	void StopDash();
	void BeginDashCollisionOverride();
	void EndDashCollisionOverride();
	void BeginDashRootMotionOverride();
	void EndDashRootMotionOverride();
	void UpdateDash(float DeltaSeconds);
	void SpawnGroundCircle();
	UAnimMontage* GetMontageForAttack(EFinalBossAttack Attack) const;
	bool PlayCurrentAttackMontage();
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void CleanupCurrentAttack(bool bCancelPendingGroundTelegraphs);
	ASurvivorPlayerController* ResolvePlayerController();

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RectangleMaterialInstance;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CircleMaterialInstance;
	UPROPERTY(Transient) TObjectPtr<ASurvivorPlayerController> PlayerController;
	UPROPERTY(Transient) TArray<TObjectPtr<ABossGroundTelegraph>> ActiveGroundTelegraphs;
	UPROPERTY(Transient) TObjectPtr<ABossGroundTelegraph> ActiveAttackTelegraph;
	EFinalBossState BossState = EFinalBossState::Idle;
	EFinalBossAttack CurrentAttack = EFinalBossAttack::None;
	EFinalBossAttack PreviousAttack = EFinalBossAttack::None;
	FVector LockedAttackOrigin = FVector::ZeroVector;
	FVector LockedAttackDirection = FVector::ForwardVector;
	FVector DashStart = FVector::ZeroVector;
	FVector DashEnd = FVector::ZeroVector;
	float StateElapsed = 0.0f;
	float DashElapsed = 0.0f;
	float RectangleFillDuration = 1.0f;
	double RectangleFillStartTime = 0.0;
	bool bDashHitPlayer = false;
	bool bDashCollisionOverrideActive = false;
	TEnumAsByte<ECollisionResponse> PreDashPawnCollisionResponse = ECR_Block;
	bool bDashRootMotionOverrideActive = false;
	TEnumAsByte<ERootMotionMode::Type> PreDashRootMotionMode = ERootMotionMode::NoRootMotionExtraction;
	bool bCombatEnabled = false;
	bool bAttackExecuted = false;
	bool bGroundPursuitWindowActive = false;
	bool bPhase2TransitionQueued = false;
	bool bPhase2Active = false;
	bool bPhase2TransitionMontagePlaying = false;
	int32 Phase2CleaveTelegraphIndex = 0;
	int32 Phase2DashTelegraphIndex = 0;
	FVector Phase2BaseAttackDirection = FVector::ForwardVector;
	FVector Phase2CleaveBaseDirection = FVector::ForwardVector;
	FVector ActivePhase2CleaveDirection = FVector::ForwardVector;
	FTimerHandle RectangleFillTimerHandle;
	UPROPERTY(EditAnywhere, Category="Boss|Telegraph", meta=(ClampMin="0.01")) float TelegraphFillUpdateInterval = 0.025f;
};
