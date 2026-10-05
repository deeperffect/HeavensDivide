#pragma once

#include "CoreMinimal.h"

class UUpgradeDefinition;

// Shared native rules for eligibility, save conversion and offer presentation.
// Stable retired IDs remain rejection records for old assets and run snapshots.
namespace PlayerUpgradeRules
{
// Columns: unchosen/Blood, Iaijutsu, Crescent. Each row is one investment.
extern const FName SamuraiScalingIds[3][3];
bool IsSamuraiUpgradeTemporarilyDisabled(FName Id);
bool IsBloodUpgrade(FName Id);
bool IsIaijutsuUpgrade(FName Id);
bool IsCrescentUpgrade(FName Id);
bool IsSamuraiMeleeScalingUpgrade(FName Id);
bool IsSamuraiScalingUpgrade(FName Id);
float UpgradeScalingBase(const UUpgradeDefinition* Card);
int32 UpgradeRankLimit(const UUpgradeDefinition* Card);
bool IsStanceRareUpgrade(const UUpgradeDefinition* U);
bool IsBloodShrineUpgrade(const UUpgradeDefinition* U);
bool IsTrialBuildStarter(const UUpgradeDefinition* Upgrade);
bool IsRetiredUpgrade(FName Id);
} // namespace PlayerUpgradeRules
