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
	void InitializeProjectile(AActor* InGameplayOwner, FVector Direction, float Damage, float Speed, EProjectileTargetType InTargetType = EProjectileTargetType::Enemies);

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Marked for Death", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MarkedTargetDamageMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Marked for Death", meta = (ClampMin = "1", UIMin = "1"))
	int32 ChainExecutionTargetCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Marked for Death", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float ChainExecutionRadius = 500.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Projectile")
	EProjectileTargetType TargetType = EProjectileTargetType::Enemies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Debug")
	bool bDebugProjectileFiltering = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Debug")
	bool bDebugMarkedDamage = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Debug")
	bool bDebugChainExecution = false;

private:
	UFUNCTION()
	void HandleProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void LogProjectileFilterResult(AActor* OtherActor, bool bValidDamageTarget) const;
	void TryTriggerChainExecution(AEnemyBase* ExecutedEnemy, const FVector& ExecutionLocation);
	int32 GetSafeChainExecutionTargetCount() const;
	float GetSafeChainExecutionRadius() const;

	UPROPERTY()
	TObjectPtr<AActor> GameplayOwner;

	bool bIsProjectileInitialized = false;
};
