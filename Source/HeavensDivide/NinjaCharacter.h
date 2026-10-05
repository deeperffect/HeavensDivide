// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CharacterBase.h"
#include "NinjaCharacter.generated.h"

class UAutoAttackComponent;
class UNinjaBuildComponent;

UCLASS()
class HEAVENSDIVIDE_API ANinjaCharacter : public ACharacterBase
{
	GENERATED_BODY()

public:
	ANinjaCharacter();
 virtual void SetCharacterMode(ECharacterMode NewMode) override;
 virtual void ApplySharedMoveSpeedMultiplier(float Multiplier) override;
 void ApplyViperRush(float Bonus, float Duration);
 void ClearViperRush();

private:
 float SharedMoveSpeedMultiplier = 1.f, ViperRushBonus = 0.f;
 FTimerHandle ViperRushTimer;
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ToolTip = "Ninja stance attacks and their presentation, including the custom shuriken mesh."))
	TObjectPtr<UNinjaBuildComponent> NinjaBuildComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ToolTip = "Auto-attack component that owns Ninja projectile attack timing, montages, projectile spawning, and related upgrades."))
	TObjectPtr<UAutoAttackComponent> AutoAttackComponent;
};
