// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CharacterBase.h"
#include "NinjaCharacter.generated.h"

class UAutoAttackComponent;

UCLASS()
class HEAVENSDIVIDE_API ANinjaCharacter : public ACharacterBase
{
	GENERATED_BODY()

public:
	ANinjaCharacter();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ToolTip = "Auto-attack component that owns Ninja projectile attack timing, montages, projectile spawning, and related upgrades."))
	TObjectPtr<UAutoAttackComponent> AutoAttackComponent;
};
