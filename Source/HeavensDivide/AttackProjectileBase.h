// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AttackProjectileBase.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class AEnemyBase;

UENUM(BlueprintType)
enum class EProjectileTargetType : uint8
{
	Enemies,
	ActivePlayer
};

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AAttackProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	AAttackProjectileBase();

	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void InitializeProjectile(
		AActor* InGameplayOwner,
		FVector Direction,
		float Damage,
		float Speed,
		EProjectileTargetType InTargetType = EProjectileTargetType::Enemies,
		float InTargetingRange = 0.0f,
		bool bInCanTriggerExecutionersKunai = true,
		AActor* InIgnoredOverlapActor = nullptr,
		bool bFlattenLaunchDirection = true,
		int32 InAdditionalPierceCount = 0);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ProjectileSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ProjectileDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ProjectileLifetime = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Impact", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "How long the projectile actor remains alive after impact so attached trails can fade naturally. Collision and movement are disabled during this time."))
	float ImpactTrailFadeDuration = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Marked for Death", meta = (ClampMin = "1.0", UIMin = "1.0", ToolTip = "Damage multiplier used when a Ninja projectile consumes a Marked enemy's Mark. 2.0 means double damage."))
	float MarkedTargetDamageMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Marked for Death", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Maximum number of nearby unmarked enemies that receive Mark when Chain Execution triggers."))
	int32 ChainExecutionTargetCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Marked for Death", meta = (ClampMin = "1.0", UIMin = "1.0", ToolTip = "Search radius around an executed enemy for Chain Execution mark spread."))
	float ChainExecutionRadius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Marked for Death", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Small forward offset from the consumed Mark location used when spawning Executioner's Kunai."))
	float ExecutionersKunaiSpawnForwardOffset = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Marked for Death", meta = (ToolTip = "World-space offset from the consumed Mark location used for Executioner's Kunai. Increase Z for a higher rain-from-above spawn."))
	FVector ExecutionersKunaiSpawnOffset = FVector(0.0f, 0.0f, 450.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Marked for Death", meta = (ClampMin = "0.01", UIMin = "0.01", ToolTip = "Speed multiplier applied only to Executioner's Kunai bonus projectiles."))
	float ExecutionersKunaiSpeedMultiplier = 2.5f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Projectile")
	EProjectileTargetType TargetType = EProjectileTargetType::Enemies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Debug", meta = (ToolTip = "Logs why projectile overlaps are accepted or ignored."))
	bool bDebugProjectileFiltering = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Debug", meta = (ToolTip = "Logs Marked for Death damage checks, mark consumption, and final damage values."))
	bool bDebugMarkedDamage = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Debug", meta = (ToolTip = "Logs Chain Execution candidate search and mark spread results."))
	bool bDebugChainExecution = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Debug", meta = (ToolTip = "Logs Executioner's Kunai target selection and spawn direction."))
	bool bDebugExecutionersKunai = false;

private:
	UFUNCTION()
	void HandleProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void LogProjectileFilterResult(AActor* OtherActor, bool bValidDamageTarget) const;
	void BeginImpactTrailFade();
	bool ConsumeEnemyHit(AEnemyBase* HitEnemy);
	void TryTriggerChainExecution(AEnemyBase* ExecutedEnemy, const FVector& ExecutionLocation);
	void TryTriggerExecutionersKunai(AEnemyBase* ConsumedMarkEnemy, const FVector& MarkConsumedLocation);
	AEnemyBase* FindExecutionersKunaiTarget(AEnemyBase* ConsumedMarkEnemy, const FVector& SearchLocation) const;
	void SpawnExecutionersKunai(AEnemyBase* TargetEnemy, AEnemyBase* ConsumedMarkEnemy, const FVector& SpawnOrigin);
	int32 GetSafeChainExecutionTargetCount() const;
	float GetSafeChainExecutionRadius() const;

	UPROPERTY()
	TObjectPtr<AActor> GameplayOwner;

	UPROPERTY()
	TObjectPtr<AActor> IgnoredOverlapActor;

	float SourceTargetingRange = 0.0f;
	int32 AdditionalPierceCount = 0;
	int32 RemainingEnemyHits = 1;
	bool bIsProjectileInitialized = false;
	bool bCanTriggerExecutionersKunai = true;
	bool bImpactResolved = false;

	UPROPERTY()
	TSet<TObjectPtr<AEnemyBase>> DamagedEnemies;
};
