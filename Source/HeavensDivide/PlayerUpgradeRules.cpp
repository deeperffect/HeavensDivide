#include "PlayerUpgradeRules.h"

#include "BarrageBuild.h"
#include "ShurikenBuild.h"
#include "BuildFamilyCatalog.h"
#include "FangBuild.h"
#include "UpgradeDefinition.h"

namespace PlayerUpgradeRules
{
// Temporary availability switch. Keep definitions, tuning and combat implementations
// so these cards can be reused when the other Samurai routes are redesigned.
bool IsSamuraiUpgradeTemporarilyDisabled(FName Id)
{
	static const TSet<FName> Disabled = {TEXT("OverkillBurst"),  TEXT("BurstRadius"),   TEXT("WaveMultishot"),
	                                     TEXT("CrossingBlades"), TEXT("SplinterWave"),  TEXT("BladeWavePower"),
	                                     TEXT("WideArc"),        TEXT("BladeWaveHaste")};
	return Disabled.Contains(Id);
}
bool IsBloodUpgrade(FName Id)
{
	static const TSet<FName> Ids = {
	    TEXT("BloodTransfer"),     TEXT("Bloodletting"),       TEXT("LingeringWounds"),     TEXT("DoubleCut"),
	    TEXT("BloodEcho"),         TEXT("BloodCritical"),      TEXT("BloodAssist"),         TEXT("BloodCapacity"),
	    TEXT("BloodTransferArea"), TEXT("DoubleCutFrequency"), TEXT("BloodCriticalChance"), TEXT("BloodEchoChance"),
	    TEXT("BloodAssistChance"), TEXT("BloodPactPower"),     TEXT("BloodPactSpeed"),      TEXT("BloodDetonation")};
	return Id == TEXT("BloodRush") || Ids.Contains(Id);
}
bool IsIaijutsuUpgrade(FName Id)
{
	return Id != TEXT("Iaijutsu") && Id.ToString().StartsWith(TEXT("Iaijutsu"));
}
bool IsCrescentUpgrade(FName Id)
{
	return Id == TEXT("ReturningBlade") || Id.ToString().StartsWith(TEXT("Crescent"));
}
bool IsSamuraiMeleeScalingUpgrade(FName Id)
{
	return Id == TEXT("SamuraiHeavyBlade") || Id == TEXT("SamuraiTempo") || Id == TEXT("SamuraiArea");
}
// Columns: unchosen/Blood, Iaijutsu, Crescent. Each row is one investment.
const FName SamuraiScalingIds[3][3] = {{TEXT("SamuraiHeavyBlade"), TEXT("IaijutsuDamage"), TEXT("CrescentDamage")},
                                       {TEXT("SamuraiTempo"), TEXT("IaijutsuChargeSpeed"), TEXT("CrescentSpeed")},
                                       {TEXT("SamuraiArea"), TEXT("IaijutsuWidth"), TEXT("CrescentRange")}};
bool IsSamuraiScalingUpgrade(FName Id)
{
	for (const auto& Group : SamuraiScalingIds)
		for (FName Member : Group)
			if (Id == Member)
				return true;
	return false;
}
float UpgradeScalingBase(const UUpgradeDefinition* Card)
{
	if (!Card)
		return 0.f;
	if (Card->bUsesRolledRarity)
		for (const auto& Entry : Card->RarityMagnitudes)
			if (Entry.Rarity == EUpgradeRarity::Common)
				return Entry.Magnitude;
	return Card->GetBalanceValue(TEXT("PerRank"),
	                             Card->StatModifiers.IsEmpty() ? 0.f : Card->StatModifiers[0].ValuePerLevel);
}
int32 UpgradeRankLimit(const UUpgradeDefinition* Card)
{
	if (!Card)
		return 0;
	// Stable-ID guard also covers old asset references with the former four-rank cap.
	const bool bDoubleCutFrequency =
	    Card->UpgradeId == TEXT("DoubleCutFrequency") || Card->UpgradeId == TEXT("IaijutsuDoubleCutFrequency") ||
	    Card->UpgradeId == TEXT("CrescentDoubleCutFrequency") || Card->UpgradeId == TEXT("FangTwinFrequency") ||
	    Card->UpgradeId == TEXT("BarrageRainFrequency") || Card->UpgradeId == TEXT("ShurikenTwinFrequency");
	return bDoubleCutFrequency ? FMath::Min(3, Card->MaxLevel) : Card->MaxLevel;
}
bool IsStanceRareUpgrade(const UUpgradeDefinition* U)
{
	return U && U->Rarity == EUpgradeRarity::Rare &&
	       (IsBloodUpgrade(U->UpgradeId) || IsIaijutsuUpgrade(U->UpgradeId) || IsCrescentUpgrade(U->UpgradeId) ||
	        FangBuild::IsUpgrade(U->UpgradeId) || BarrageBuild::IsUpgrade(U->UpgradeId) || ShurikenBuild::IsUpgrade(U->UpgradeId));
}
bool IsBloodShrineUpgrade(const UUpgradeDefinition* U)
{
	return U && (ShurikenBuild::IsShrine(U->UpgradeId) || BarrageBuild::IsShrine(U->UpgradeId) || FangBuild::IsShrine(U->UpgradeId) ||
	             U->UpgradeId == TEXT("BloodPactPower") || U->UpgradeId == TEXT("BloodPactSpeed") ||
	             U->UpgradeId == TEXT("BloodDetonation") || U->UpgradeId == TEXT("CrescentFieldPact") ||
	             U->UpgradeId == TEXT("CrescentPowerPact") || U->UpgradeId == TEXT("CrescentEruptionPact") ||
	             U->UpgradeId == TEXT("IaijutsuMarkPact") || U->UpgradeId == TEXT("IaijutsuPowerPact") ||
	             U->UpgradeId == TEXT("IaijutsuDashPact"));
}
bool IsTrialBuildStarter(const UUpgradeDefinition* Upgrade)
{
	static const TSet<FName> Starters = {TEXT("BladeWave"),     TEXT("BattleStance"),  TEXT("Iaijutsu"),
	                                     TEXT("ReturningFang"), TEXT("BarrageStance"), TEXT("GreatShuriken")};
	return Upgrade && Starters.Contains(Upgrade->UpgradeId);
}
bool IsRetiredUpgrade(FName Id)
{
	static const TSet<FName> Retired = [] {
		TSet<FName> Ids = {TEXT("HeavyShuriken"), TEXT("CuttingReturn"),
		                   TEXT("FinalPursuit"),
		                   TEXT("FocusedVolley"),
		                   TEXT("Crescendo"),
		                   TEXT("BloodStance"),
		                   TEXT("ExecutionStance"),
		                   TEXT("WaveStance"),
		                   TEXT("BleedingEdge"),
		                   TEXT("DeepCuts"),
		                   TEXT("Bloodhound"),
		                   TEXT("AlternatingFans"),
		                   TEXT("Crossfire"),
		                   TEXT("ExecutionersKunai"),
		                   TEXT("FanOfBlades"),
		                   TEXT("NinjaProjectileBonus"),
		                   TEXT("NinjaProjectilePierce"),
		                   TEXT("ChainExecution"),
		                   TEXT("BladeCascade"),
		                   TEXT("ProjectileBounce"),
		                   TEXT("ProjectileSplit"),
		                   TEXT("PotentVenom"),
		                   TEXT("VirulentStrain"),
		                   TEXT("HemotoxicReaction"),
		                   TEXT("AcceleratedVenom")};
		// Build this once, rather than reconstructing every retired scaling ID for
		// every candidate in every offer. These families have no runtime implementation.
		for (const auto& Family : BuildFamilies)
		{
			if (Family.Available)
				continue;
			Ids.Add(FName(Family.Id));
			for (const auto* Branch : Family.Branches)
				Ids.Add(FName(Branch));
			for (const auto* Suffix : {TEXT("Power"), TEXT("Area"), TEXT("Haste")})
				Ids.Add(FName(FString(Family.Id) + Suffix));
		}
		return Ids;
	}();
	return Retired.Contains(Id);
}
} // namespace PlayerUpgradeRules
