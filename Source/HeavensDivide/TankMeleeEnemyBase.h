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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam", meta = (ToolTip = "Deferred decal material used for the slam telegraph. The material can use FillAmount, BackgroundColor, FillColor, and TelegraphOpacity parameters."))
	TObjectPtr<UMaterialInterface> AttackTelegraphMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam", meta = (ToolTip = "Shape used by the tank slam telegraph and hit area. Box is a long rectangle in front of the enemy; Circle is an area around the enemy."))
	ETankSlamAttackShape AttackShape = ETankSlamAttackShape::Box;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "AttackShape == ETankSlamAttackShape::Circle", EditConditionHides, ToolTip = "Radius of the circular slam area when Attack Shape is Circle."))
	float AttackAoERadius = 375.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Box", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "AttackShape == ETankSlamAttackShape::Box", EditConditionHides, ToolTip = "Length of the rectangular slam area in front of the enemy."))
	float AttackBoxLength = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Box", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "AttackShape == ETankSlamAttackShape::Box", EditConditionHides, ToolTip = "Width of the rectangular slam area."))
	float AttackBoxWidth = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Box", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "AttackShape == ETankSlamAttackShape::Box", EditConditionHides, ToolTip = "Forward offset from the enemy to the center of the rectangular slam area. Usually half of Attack Box Length."))
	float AttackBoxForwardOffset = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Multiplier applied to this enemy's base attack damage when the slam hit resolves."))
	float SlamDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Facing", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "deg/s", ToolTip = "Maximum yaw turn speed while tracking the player during the slam windup. Lower values make the telegraph easier to outrun. Set to 0 to disable windup tracking."))
	float WindupTrackingRotationSpeed = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Telegraph", meta = (ClampMin = "0.01", UIMin = "0.01", AdvancedDisplay, ToolTip = "Duration used to animate the telegraph material FillAmount from 0 to 1 before impact."))
	float TelegraphWindupDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam|Telegraph", meta = (ClampMin = "0.01", UIMin = "0.01", AdvancedDisplay, ToolTip = "Seconds between temporary telegraph fill updates during the slam windup. Lower is smoother; higher is cheaper."))
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
