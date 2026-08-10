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

	UPROPERTY(BlueprintAssignable, Category = "Experience")
	FOnXPChanged OnXPChanged;

	UPROPERTY(BlueprintAssignable, Category = "Experience")
	FOnLevelUp OnLevelUp;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience")
	int32 CurrentXP = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Experience")
	int32 CurrentLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience", meta = (ClampMin = "1", UIMin = "1"))
	int32 BaseXPRequirement = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience", meta = (ClampMin = "0", UIMin = "0"))
	int32 XPRequirementGrowth = 5;

private:
	int32 CalculateXPToNextLevel() const;
	void BroadcastXPChanged();
};
