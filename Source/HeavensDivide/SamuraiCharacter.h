// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CharacterBase.h"
#include "SamuraiCharacter.generated.h"

class UAutoAttackComponent;

UCLASS()
class HEAVENSDIVIDE_API ASamuraiCharacter : public ACharacterBase
{
	GENERATED_BODY()

public:
	ASamuraiCharacter();
	virtual void SetCharacterMode(ECharacterMode NewMode) override;
	virtual void ApplySharedMoveSpeedMultiplier(float MoveSpeedMultiplier) override;
	void ApplyBloodRush(float Bonus, float Duration);
	void ClearBloodRush();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ToolTip = "Auto-attack component that owns Samurai melee attack timing, montages, hit traces, and related upgrades."))
	TObjectPtr<UAutoAttackComponent> AutoAttackComponent;
private:
	float SharedMoveSpeedMultiplier = 1.f;
	float BloodRushBonus = 0.f;
	FTimerHandle BloodRushTimer;
};
