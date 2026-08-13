// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeleeEnemyBase.h"
#include "TankMeleeEnemyBase.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
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
	ETankSlamAttackShape AttackShape = ETankSlamAttackShape::Box;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "AttackShape == ETankSlamAttackShape::Circle", EditConditionHides))
	float AttackAoERadius = 375.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Box", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "AttackShape == ETankSlamAttackShape::Box", EditConditionHides))
	float AttackBoxLength = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Box", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "AttackShape == ETankSlamAttackShape::Box", EditConditionHides))
	float AttackBoxWidth = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Box", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "AttackShape == ETankSlamAttackShape::Box", EditConditionHides))
	float AttackBoxForwardOffset = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SlamDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Facing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WindupTrackingRotationSpeed = 540.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Telegraph", meta = (ClampMin = "0.01", UIMin = "0.01", AdvancedDisplay))
	float TelegraphWindupDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Telegraph", meta = (ClampMin = "0.01", UIMin = "0.01", AdvancedDisplay))
	float TelegraphFillUpdateInterval = 0.025f;

private:
	void ShowAttackTelegraph();
	void HideAttackTelegraph();
	void InitializeTelegraphMaterialInstance();
	void StartTelegraphFill();
	void StopTelegraphFill();
	void ResetTelegraphFill();
	void SetTelegraphFillAmount(float FillAmount);
	void HandleTelegraphFillTimerElapsed();
	float CalculateTelegraphFillDuration(float& OutImpactNotifyTime, float& OutMontagePosition, float& OutMontagePlayRate) const;
	bool FindImpactNotifyTime(float MontagePosition, float& OutImpactNotifyTime) const;
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

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TelegraphMaterialInstance;

	FTimerHandle TelegraphFillTimerHandle;
	double TelegraphFillStartTime = 0.0;
	float ActiveTelegraphFillDuration = 1.0f;
};
