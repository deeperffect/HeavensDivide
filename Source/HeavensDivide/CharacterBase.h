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
	void LogVisibilityState(const FString& Context) const;
	UFUNCTION(BlueprintPure, Category = "Stats")
	UCharacterStatsComponent* GetCharacterStats() const;
	void ApplySharedMoveSpeedMultiplier(float MoveSpeedMultiplier);

	UPROPERTY(BlueprintAssignable, Category = "Character")
	FOnCharacterModeChanged OnCharacterModeChanged;

	UFUNCTION(BlueprintPure, Category = "Animation")
	FVector2D GetLocalMovementVector() const;

	UFUNCTION(BlueprintPure, Category = "Animation")
	float GetLocalForwardSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Animation")
	float GetLocalRightSpeed() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	TObjectPtr<UCharacterStatsComponent> CharacterStatsComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mouse Facing", meta = (ClampMin = "0.0", UIMin = "0.0", FormerlySerializedAs = "MouseFacingRotationSpeed"))
	float FacingRotationSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	ECharacterMode CharacterMode = ECharacterMode::Active;

	void UpdateMouseFacing(float DeltaSeconds);

	FVector FacingTarget = FVector::ZeroVector;
	bool bHasFacingTarget = false;
	FVector FacingOverrideTarget = FVector::ZeroVector;
	bool bHasFacingOverride = false;
	float BaseMaxWalkSpeed = 0.0f;
};
