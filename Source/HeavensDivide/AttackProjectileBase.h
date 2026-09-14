// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImpactFeedback.h"
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
	friend class FImpactFeedbackTest;
 friend class FNinjaBuildsTest;
 friend class ANinjaBuildProjectile;
 friend class UNinjaBuildComponent;

public:
	AAttackProjectileBase();
	bool bAssistProjectile = false;

	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void InitializeProjectile(
		AActor* InGameplayOwner,
		FVector Direction,
		float Damage,
		float Speed,
		EProjectileTargetType InTargetType = EProjectileTargetType::Enemies,
		float InTargetingRange = 0.0f,
		AActor* InIgnoredOverlapActor = nullptr,
		bool bFlattenLaunchDirection = true,
		int32 InAdditionalPierceCount = 0,
		int32 InAdditionalBounceCount = 0,
		int32 InSplitUpgradeLevel = 0);

protected:
	/** Basic kunai defaults to no camera shake. Heavy projectile subclasses can opt in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile|Impact") FImpactFeedbackData ImpactFeedback;
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






	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Bounce", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BounceSearchRadius = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Split", meta = (ClampMin = "1", UIMin = "1"))
	int32 SplitProjectileCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Split", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SplitAngleDegrees = 25.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Projectile")
	EProjectileTargetType TargetType = EProjectileTargetType::Enemies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Debug", meta = (ToolTip = "Logs why projectile overlaps are accepted or ignored."))
	bool bDebugProjectileFiltering = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Debug", meta = (ToolTip = "Logs Marked for Death damage checks, mark consumption, and final damage values."))
	bool bDebugMarkedDamage = false;



private:
	UFUNCTION()
	void HandleProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void LogProjectileFilterResult(AActor* OtherActor, bool bValidDamageTarget) const;
	void BeginImpactTrailFade();
	bool ConsumeEnemyHit(AEnemyBase* HitEnemy);
	bool TryBounceFromImpact(const FVector& ImpactLocation);
	AEnemyBase* FindBounceTarget(const FVector& SearchLocation) const;
	void TrySpawnSplitProjectiles(AEnemyBase* HitEnemy, const FVector& ImpactLocation);
	void SpawnSplitProjectile(const FVector& SpawnLocation, const FVector& Direction, AEnemyBase* HitEnemy);


	UPROPERTY()
	TObjectPtr<AActor> GameplayOwner;

	UPROPERTY()
	TObjectPtr<AActor> IgnoredOverlapActor;

	float SourceTargetingRange = 0.0f;
	int32 AdditionalPierceCount = 0;
	int32 RemainingEnemyHits = 1;
	int32 RemainingBounces = 0;
	bool bCanTriggerSplit = false;
	bool bIsProjectileInitialized = false;
	bool bImpactResolved = false;

	UPROPERTY()
	TSet<TObjectPtr<AEnemyBase>> DamagedEnemies;
};
