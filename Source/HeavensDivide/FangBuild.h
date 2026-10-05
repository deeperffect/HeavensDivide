#pragma once
#include "CoreMinimal.h"
class UPlayerUpgradeComponent;

namespace FangBuild
{
bool IsUpgrade(FName Id);
bool IsShrine(FName Id);
bool IsLegacyShared(FName Id);
float Scaling(const UPlayerUpgradeComponent* Upgrades, FName Id, float Default);
}
