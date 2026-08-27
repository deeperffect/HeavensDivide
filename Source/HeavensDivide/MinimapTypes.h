#pragma once

#include "CoreMinimal.h"
#include "MinimapTypes.generated.h"

UENUM(BlueprintType)
enum class EMinimapMarkerType : uint8
{
	Player,
	Objective,
	BossGate
};

UENUM(BlueprintType)
enum class EMinimapMarkerState : uint8
{
	Available,
	Active,
	Completed,
	Failed,
	Locked
};

