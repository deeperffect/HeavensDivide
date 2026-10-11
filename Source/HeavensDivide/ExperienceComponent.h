// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExperienceComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnXPChanged, int32, CurrentXP, int32, XPToNextLevel, float, XPPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelUp, int32, NewLevel);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API UExperienceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UExperienceComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Experience")
	void AddXP(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Experience")
	int32 GetCurrentXP() const;

	UFUNCTION(BlueprintPure, Category = "Experience")
	int32 GetCurrentLevel() const;

	UFUNCTION(BlueprintPure, Category = "Experience")
	int32 GetXPToNextLevel() const;

	UFUNCTION(BlueprintPure, Category = "Experience")
	float GetXPPercent() const;

	void RestoreRunState(int32 Level, int32 XP);

	UPROPERTY(BlueprintAssignable, Category = "Experience")
	FOnXPChanged OnXPChanged;

	UPROPERTY(BlueprintAssignable, Category = "Experience")
	FOnLevelUp OnLevelUp;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience")
	int32 CurrentXP = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience")
	int32 CurrentLevel = 1;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use XPRequirementScale; requirements use the authored level curve."))
	int32 BaseXPRequirement = 10;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use XPRequirementScale; requirements use the authored level curve."))
	int32 XPRequirementGrowth = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience", meta=(ClampMin="0.1", ClampMax="10", ToolTip="Scales the complete authored XP curve. 1 preserves current leveling."))
	float XPRequirementScale = 1.f;

private:
	int32 CalculateXPToNextLevel() const;
	void BroadcastXPChanged();
};
