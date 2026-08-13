// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeleeEnemyBase.h"
#include "TankMeleeEnemyBase.generated.h"

class UDecalComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class ETankSlamAttackShape : uint8
{
	Circle,
	Box
};

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ATankMeleeEnemyBase : public AMeleeEnemyBase
{
	GENERATED_BODY()

public:
	ATankMeleeEnemyBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Enemy|Attack|Slam")
	void CommitSlamFacing();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleDeath() override;
	virtual void UpdateEnemyBehavior(float DeltaSeconds) override;
	virtual void StopEnemyBehavior() override;
	virtual void HandleAttackCommitted() override;
	virtual void HandleAttackFinished() override;
	virtual void ExecuteAttackHit() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDecalComponent> AttackTelegraphDecal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam")
	TObjectPtr<UMaterialInterface> AttackTelegraphMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam")
	ETankSlamAttackShape AttackShape = ETankSlamAttackShape::Circle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "AttackShape == ETankSlamAttackShape::Circle", EditConditionHides))
	float AttackAoERadius = 375.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Box", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "AttackShape == ETankSlamAttackShape::Box", EditConditionHides))
	float AttackBoxLength = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Box", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "AttackShape == ETankSlamAttackShape::Box", EditConditionHides))
	float AttackBoxWidth = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Box", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "AttackShape == ETankSlamAttackShape::Box", EditConditionHides))
	float AttackBoxForwardOffset = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SlamDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Facing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WindupTrackingRotationSpeed = 540.0f;

private:
	void ShowAttackTelegraph();
	void HideAttackTelegraph();
	void StartWindupFacingTracking();
	void LockAttackFacing();
	void ClearAttackFacingState();
	void UpdateWindupFacing(float DeltaSeconds);
	void UpdateAttackTelegraphSizeAndPlacement();
	bool IsPlayerInsideSlam(const ACharacterBase* ActivePlayerCharacter, float& OutDistanceForLog) const;
	bool IsPlayerInsideCircleSlam(const ACharacterBase* ActivePlayerCharacter, float& OutDistanceForLog) const;
	bool IsPlayerInsideBoxSlam(const ACharacterBase* ActivePlayerCharacter, float& OutDistanceForLog) const;
	FVector GetBoxSlamCenter() const;

	bool bTrackPlayerDuringAttackWindup = false;
	bool bAttackFacingLocked = false;
};
