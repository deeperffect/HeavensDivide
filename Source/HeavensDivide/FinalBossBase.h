#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "FinalBossBase.generated.h"

class ABossGroundTelegraph;
class ASurvivorPlayerController;
class UAnimMontage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
class UDecalComponent;

UENUM(BlueprintType)
enum class EFinalBossAttack : uint8
{
	None,
	ForwardCleave,
	PointBlankAoE,
	LongDash,
	GroundPursuit
};

UENUM(BlueprintType)
enum class EFinalBossState : uint8
{
	Idle,
	Windup,
	AttackActive,
	Recovery,
	Cooldown,
	Dead
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBossDefeated, AFinalBossBase*, Boss);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBossAttackEvent, EFinalBossAttack, Attack);

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
	UFUNCTION(BlueprintPure, Category="Boss") EFinalBossState GetBossState() const { return BossState; }
	UFUNCTION(BlueprintPure, Category="Boss") EFinalBossAttack GetCurrentAttack() const { return CurrentAttack; }
	UFUNCTION(BlueprintPure, Category="Boss") FText GetBossDisplayName() const { return BossDisplayName; }

	UPROPERTY(BlueprintAssignable, Category="Boss|Events") FBossDefeated OnBossDefeated;
	UPROPERTY(BlueprintAssignable, Category="Boss|Events") FBossAttackEvent OnBossAttackStarted;
	UPROPERTY(BlueprintAssignable, Category="Boss|Events") FBossAttackEvent OnBossAttackImpact;

protected:
	virtual void UpdateEnemyBehavior(float DeltaSeconds) override;
	virtual bool ShouldSkipMovement() const override;
	virtual bool ShouldUseWorldHealthBar() const override { return false; }
	virtual void HandleDeath() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|General", meta=(ClampMin="1.0")) float BossMaxHealth = 5000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|General", meta=(ClampMin="0.0")) float BossBaseDamage = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|General", meta=(ClampMin="0.0")) float BossMoveSpeed = 350.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|General", meta=(ClampMin="0.0")) float AttackCooldown = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|General", meta=(ClampMin="0.0")) float RecoveryDuration = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|General") FText BossDisplayName = FText::FromString(TEXT("FINAL BOSS"));
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Testing") bool bAutoStartCombat = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Testing") bool bDebugAttackTelegraphs = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Forward Cleave", meta=(ClampMin="1.0")) float ForwardAttackLength = 900.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Forward Cleave", meta=(ClampMin="1.0")) float ForwardAttackWidth = 250.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Forward Cleave", meta=(ClampMin="0.01")) float ForwardAttackTelegraphDuration = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Forward Cleave", meta=(ClampMin="0.0")) float ForwardAttackDamage = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Point Blank AoE", meta=(ClampMin="1.0")) float AoERadius = 600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Point Blank AoE", meta=(ClampMin="0.01")) float AoETelegraphDuration = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Point Blank AoE", meta=(ClampMin="0.0")) float AoEDamage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Long Dash", meta=(ClampMin="1.0")) float DashDistance = 1400.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Long Dash", meta=(ClampMin="1.0")) float DashWidth = 220.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Long Dash", meta=(ClampMin="0.01")) float DashTelegraphDuration = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Long Dash", meta=(ClampMin="0.01")) float DashTravelDuration = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Long Dash", meta=(ClampMin="0.0")) float DashDamage = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit", meta=(ClampMin="1")) int32 GroundCircleCount = 6;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit", meta=(ClampMin="0.01")) float GroundCircleSpawnInterval = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit", meta=(ClampMin="0.01")) float GroundCircleTelegraphDuration = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit", meta=(ClampMin="1.0")) float GroundCircleRadius = 180.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit", meta=(ClampMin="0.0")) float GroundCircleDamage = 18.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Ground Pursuit") TSubclassOf<ABossGroundTelegraph> GroundTelegraphClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Telegraph") TObjectPtr<UMaterialInterface> TelegraphMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Telegraph") TObjectPtr<UMaterialInterface> RectangleTelegraphMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Animation") TObjectPtr<UAnimMontage> Attack1Montage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Animation") TObjectPtr<UAnimMontage> Attack2Montage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Animation") TObjectPtr<UAnimMontage> Attack3Montage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Animation") TObjectPtr<UAnimMontage> Attack4Montage;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Telegraph") TObjectPtr<UDecalComponent> BossAttackTelegraphDecal;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Telegraph") TObjectPtr<UStaticMeshComponent> CircleTelegraph;

private:
	void ChooseAttack();
	void BeginAttack(EFinalBossAttack Attack);
	void ResolveWindup();
	void BeginRecovery();
	void UpdateTelegraph(float Alpha);
	void ConfigureRectangle(float Length, float Width);
	void ConfigureCircle(float Radius);
	void DamagePlayerInRectangle(float Length, float Width, float Damage);
	void DamagePlayerInCircle(float Radius, float Damage);
	void UpdateDash(float DeltaSeconds);
	void UpdateGroundPursuit(float DeltaSeconds);
	void SpawnGroundCircle();
	void PlayAttackMontage(UAnimMontage* Montage);
	void CleanupAttacks();
	ASurvivorPlayerController* ResolvePlayerController();

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RectangleMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CircleMaterial;
	UPROPERTY(Transient) TObjectPtr<ASurvivorPlayerController> PlayerController;
	UPROPERTY(Transient) TArray<TObjectPtr<ABossGroundTelegraph>> ActiveGroundTelegraphs;
	EFinalBossState BossState = EFinalBossState::Idle;
	EFinalBossAttack CurrentAttack = EFinalBossAttack::None;
	EFinalBossAttack PreviousAttack = EFinalBossAttack::None;
	FVector LockedAttackOrigin = FVector::ZeroVector;
	FVector LockedAttackDirection = FVector::ForwardVector;
	FVector DashStart = FVector::ZeroVector;
	FVector DashEnd = FVector::ZeroVector;
	float StateElapsed = 0.0f;
	float CurrentWindupDuration = 1.0f;
	float GroundSpawnElapsed = 0.0f;
	int32 GroundCirclesSpawned = 0;
	bool bDashHitPlayer = false;
	bool bCombatEnabled = false;
};
