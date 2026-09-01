// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CharacterBase.generated.h"

class USceneComponent;
class UAnimMontage;
class UCharacterStatsComponent;

UENUM(BlueprintType)
enum class ECharacterMode : uint8
{
	Active,
	Inactive,
	Assisting
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCharacterModeChanged, ECharacterMode, OldMode, ECharacterMode, NewMode);

UCLASS()
class HEAVENSDIVIDE_API ACharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ACharacterBase();

	virtual void Tick(float DeltaSeconds) override;

	void MoveCharacter(FVector2D Input);
	void SetFacingTarget(FVector WorldTarget);
	void SetFacingOverrideTarget(FVector WorldTarget);
	void ClearFacingOverride();
	void SetCharacterMode(ECharacterMode NewMode);
	ECharacterMode GetCharacterMode() const;
	FRotator GetVisualFacingRotation() const;
	FVector GetVisualForwardVector() const;
	void SetVisualFacingRotation(FRotator NewRotation);
	void StopPlayerGameplay();
	void PlayDeathMontage();
	void StartDashVisual(float GameplayDashDuration, FVector DashDirection);
	void EndDashVisual();
	bool IsDashing() const;
	UFUNCTION(BlueprintPure, Category = "Stats")
	UCharacterStatsComponent* GetCharacterStats() const;
	void ApplySharedMoveSpeedMultiplier(float MoveSpeedMultiplier);

	UPROPERTY(BlueprintAssignable, Category = "Character", meta = (ToolTip = "Broadcast when this character changes between Active, Inactive, and Assisting modes."))
	FOnCharacterModeChanged OnCharacterModeChanged;

	UFUNCTION(BlueprintPure, Category = "Animation")
	FVector2D GetLocalMovementVector() const;

	UFUNCTION(BlueprintPure, Category = "Animation")
	float GetLocalForwardSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Animation")
	float GetLocalRightSpeed() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual", meta = (ToolTip = "Root scene component used to rotate/face the character visuals independently from the actor capsule."))
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ToolTip = "Character-specific runtime stats and upgrade modifiers for this character."))
	TObjectPtr<UCharacterStatsComponent> CharacterStatsComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mouse Facing", meta = (ClampMin = "0.0", UIMin = "0.0", FormerlySerializedAs = "MouseFacingRotationSpeed", ToolTip = "How quickly the character visual turns toward the mouse/facing target. Higher values turn faster."))
	float FacingRotationSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ToolTip = "Death montage played for this player character when the shared player health reaches zero."))
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash", meta = (ToolTip = "Dash montage played by this character when the shared dash system starts a dash."))
	TObjectPtr<UAnimMontage> DashMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.01", UIMin = "0.01", ToolTip = "Minimum play rate used when matching the dash montage to the gameplay dash duration."))
	float MinDashMontagePlayRate = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.01", UIMin = "0.01", ToolTip = "Maximum play rate used when matching the dash montage to the gameplay dash duration."))
	float MaxDashMontagePlayRate = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash", meta = (ToolTip = "If enabled, the character visual temporarily faces the dash direction while dashing."))
	bool bFaceDashDirection = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character", meta = (ToolTip = "Current gameplay mode for this character: active, inactive, or temporarily assisting."))
	ECharacterMode CharacterMode = ECharacterMode::Active;

	void UpdateMouseFacing(float DeltaSeconds);

	FVector FacingTarget = FVector::ZeroVector;
	bool bHasFacingTarget = false;
	FVector FacingOverrideTarget = FVector::ZeroVector;
	bool bHasFacingOverride = false;
	float BaseMaxWalkSpeed = 0.0f;
	bool bIsDashing = false;
};
