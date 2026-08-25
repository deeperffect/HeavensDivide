// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyStatusTypes.generated.h"

UENUM(BlueprintType)
enum class EEnemyStatusEffect : uint8
{
	Bleed,
	Poison
};
