// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AttackProjectileBase.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

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
	void InitializeProjectile(AActor* InGameplayOwner, FVector Direction, float Damage, float Speed, EProjectileTargetType InTargetType = EProjectileTargetType::Enemies, AActor* InHomingTarget = nullptr, float InHomingStrengthMultiplier = 1.0f, FVector InHomingTargetOffset = FVector::ZeroVector);

protected:
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing")
	bool bIsHoming = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HomingAccelerationMagnitude = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float NearTargetDistance = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float NearTargetHomingMultiplier = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float OvershootHomingMultiplier = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HomingHitForgivenessRadius = 32.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinimumHomingAccelerationSpeedScale = 1.25f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Projectile")
	EProjectileTargetType TargetType = EProjectileTargetType::Enemies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Debug")
	bool bDebugProjectileFiltering = false;

private:
	UFUNCTION()
	void HandleProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleHomingTargetDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleHomingTargetDeath();

	void ConfigureHoming(AActor* InHomingTarget, float InHomingStrengthMultiplier, const FVector& InHomingTargetOffset);
	void DisableHoming();
	void UpdateHomingAssist();
	bool TryApplyAssignedTargetHit(float DistanceToTarget);
	FVector GetHomingTargetCenter() const;
	void LogProjectileFilterResult(AActor* OtherActor, bool bValidDamageTarget) const;

	UPROPERTY()
	TObjectPtr<AActor> GameplayOwner;

	UPROPERTY()
	TObjectPtr<AActor> HomingTargetActor;

	UPROPERTY()
	TObjectPtr<USceneComponent> HomingOffsetTargetComponent;

	float BaseConfiguredHomingAcceleration = 0.0f;
	float ConfiguredHomingStrengthMultiplier = 1.0f;
	FVector ConfiguredHomingTargetOffset = FVector::ZeroVector;
	bool bWasApproachingHomingTarget = false;
	bool bLoggedNearTargetSteering = false;
	bool bLoggedOvershootRecovery = false;
	bool bIsProjectileInitialized = false;
};
