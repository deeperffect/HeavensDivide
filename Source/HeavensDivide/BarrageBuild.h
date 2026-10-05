#pragma once
#include "CoreMinimal.h"
class UPlayerUpgradeComponent;
namespace BarrageBuild
{
bool IsUpgrade(FName Id);
bool IsShrine(FName Id);
bool IsLegacy(FName Id);
float Value(const UPlayerUpgradeComponent* U, FName Id, FName Key, float Default);
float Scaling(const UPlayerUpgradeComponent* U, FName Id, float Default);
float SplitChance(const UPlayerUpgradeComponent* U);
float RangeMultiplier(const UPlayerUpgradeComponent* U);
}
