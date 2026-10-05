#pragma once
#include "CoreMinimal.h"
class UPlayerUpgradeComponent;
namespace ShurikenBuild
{
bool IsUpgrade(FName Id);
bool IsShrine(FName Id);
float ContactInterval(const UPlayerUpgradeComponent* U);
float PulseInterval(const UPlayerUpgradeComponent* U);
}
