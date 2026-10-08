#include "PlayerUpgradeComponent.h"
#include "BarrageBuild.h"
#include "ShurikenBuild.h"
#include "CharacterManagerComponent.h"
#include "FangBuild.h"
#include "PlayerUpgradeRules.h"
#include "SamuraiCharacter.h"
#include "SurvivorPlayerController.h"

using namespace PlayerUpgradeRules;

void UPlayerUpgradeComponent::CaptureRunState(FPlayerUpgradeRunState& OutState) const
{
	OutState.Levels = UpgradeLevels;
	OutState.AccumulatedMagnitudes = AccumulatedUpgradeMagnitudes;
	OutState.Definitions = AcquiredUpgradeDefinitions;
	OutState.SamuraiMastery = SamuraiMasteryPoints;
	OutState.NinjaMastery = NinjaMasteryPoints;
	OutState.BanishedUpgrades = BanishedUpgrades;
	OutState.RerollsUsed = RerollsUsed;
	OutState.BanishesUsed = BanishesUsed;
}

void UPlayerUpgradeComponent::RestoreRunState(const FPlayerUpgradeRunState& State)
{
	if (auto* PC = Cast<ASurvivorPlayerController>(GetOwner()); PC && PC->GetCharacterManager())
		if (auto* Samurai = PC->GetCharacterManager()->GetSamurai())
			Samurai->ClearBloodRush();
	// Clear the old snapshot's modifiers before installing a different stance or build.
	for (const auto& Pair : AcquiredUpgradeDefinitions)
		ClearUpgradeModifiers(Pair.Value);
	UpgradeLevels = State.Levels;
	AccumulatedUpgradeMagnitudes = State.AccumulatedMagnitudes;
	AcquiredUpgradeDefinitions = State.Definitions;
	BanishedUpgrades = State.BanishedUpgrades;
	// Normalize old runs into one current Samurai stance. An existing Iaijutsu
	// wins over the formerly mixable wave; otherwise preserve a wave build.
	const auto Owned = [this](FName Id) { return UpgradeLevels.FindRef(Id) > 0; };
	const FName Stance = Owned(TEXT("Iaijutsu"))                                 ? FName(TEXT("Iaijutsu"))
	                     : Owned(TEXT("BladeWave")) || Owned(TEXT("WaveStance")) ? FName(TEXT("BladeWave"))
	                     : Owned(TEXT("BattleStance")) || Owned(TEXT("BloodStance")) || Owned(TEXT("ExecutionStance"))
	                         ? FName(TEXT("BattleStance"))
	                         : NAME_None;
	const auto Remove = [this](FName Id) {
		UpgradeLevels.Remove(Id);
		AccumulatedUpgradeMagnitudes.Remove(Id);
		AcquiredUpgradeDefinitions.Remove(Id);
	};
	for (FName Id : {FName(TEXT("BloodStance")), FName(TEXT("ExecutionStance")), FName(TEXT("WaveStance")),
	                 FName(TEXT("BladeWave")), FName(TEXT("BattleStance")), FName(TEXT("Iaijutsu"))})
		if (Id != Stance)
			Remove(Id);
	if (!Stance.IsNone())
		if (auto* Definition = FindUpgradeDefinition(Stance))
		{
			UpgradeLevels.Add(Stance, 1);
			AcquiredUpgradeDefinitions.Add(Stance, Definition);
		}
	if (Stance != FName(TEXT("BladeWave")))
		for (FName Id : {FName(TEXT("ReturningBlade")), FName(TEXT("CrossingBlades")), FName(TEXT("SplinterWave")),
		                 FName(TEXT("BladeWavePower")), FName(TEXT("WideArc")), FName(TEXT("BladeWaveHaste")),
		                 FName(TEXT("WaveMultishot"))})
			Remove(Id);
	ConvertSamuraiScalingUpgrades();
	ConvertNinjaScalingUpgrades();
	// The former low-health scaler is now a one-time pursuit unlock. Preserve
	// additional purchased ranks in its chance branch, once, without adding mastery.
	if (Owned(TEXT("ReturningFang")) && UpgradeLevels.FindRef(TEXT("FangKillingEdge")) > 0
	    && (UpgradeLevels.FindRef(TEXT("FangKillingEdge")) > 1 || AccumulatedUpgradeMagnitudes.FindRef(TEXT("FangKillingEdge")) > 0.f))
	{
		if (auto* Chance = FindUpgradeDefinition(TEXT("FangPursuitChance")))
		{
			const int32 Extra = UpgradeLevels.FindRef(TEXT("FangKillingEdge")) - 1;
			if (Extra > 0)
			{
				UpgradeLevels.FindOrAdd(TEXT("FangPursuitChance")) = FMath::Min(Chance->MaxLevel,
				    UpgradeLevels.FindRef(TEXT("FangPursuitChance")) + Extra);
				AcquiredUpgradeDefinitions.Add(TEXT("FangPursuitChance"), Chance);
			}
			UpgradeLevels[TEXT("FangKillingEdge")] = 1;
			AccumulatedUpgradeMagnitudes.Remove(TEXT("FangKillingEdge"));
		}
	}
	TArray<FName> RestoredIds;
	UpgradeLevels.GetKeys(RestoredIds);
	for (FName Id : RestoredIds)
	{
		if (IsRetiredUpgrade(Id) || (IsBloodUpgrade(Id) && Stance != TEXT("BattleStance")) ||
		    (IsIaijutsuUpgrade(Id) && Stance != TEXT("Iaijutsu")) ||
		    (IsCrescentUpgrade(Id) && Stance != TEXT("BladeWave")) ||
		    (FangBuild::IsUpgrade(Id) && !Owned(TEXT("ReturningFang"))) ||
		    (BarrageBuild::IsUpgrade(Id) && !Owned(TEXT("BarrageStance"))) ||
		    (ShurikenBuild::IsUpgrade(Id) && !Owned(TEXT("GreatShuriken"))))
		{
			Remove(Id);
			continue;
		}
		if (auto* Current = FindUpgradeDefinition(Id))
		{
			AcquiredUpgradeDefinitions.Add(Id, Current);
			UpgradeLevels[Id] = FMath::Clamp(UpgradeLevels[Id], 0, UpgradeRankLimit(Current));
		}
	}
	NormalizeSamuraiTradeoffUpgrades();
	SamuraiMasteryPoints = FMath::Max(0, State.SamuraiMastery);
	NinjaMasteryPoints = FMath::Max(0, State.NinjaMastery);
	RerollsUsed = FMath::Clamp(State.RerollsUsed, 0, 3);
	BanishesUsed = FMath::Clamp(State.BanishesUsed, 0, 2);
	ClearCurrentOffer();
	RebuildAllUpgradeModifiers();
}

void UPlayerUpgradeComponent::ConvertSamuraiScalingUpgrades()
{
	const int32 Column = HasUpgradeId(TEXT("Iaijutsu")) ? 1 : HasUpgradeId(TEXT("BladeWave")) ? 2 : 0;
	for (const auto& Group : SamuraiScalingIds)
	{
		const FName TargetId = Group[Column];
		auto* Target = FindUpgradeDefinition(TargetId);
		if (!Target)
			continue; // Do not discard an investment if its asset is missing.
		const float TargetBase = UpgradeScalingBase(Target);
		int32 TotalRanks = 0;
		float TotalMagnitude = 0.f;
		bool bBanished = false;
		for (FName SourceId : Group)
		{
			bBanished |= BanishedUpgrades.Remove(SourceId) > 0;
			const int32 Ranks = FMath::Max(0, UpgradeLevels.FindRef(SourceId));
			if (Ranks > 0)
			{
				auto* Source = FindUpgradeDefinition(SourceId);
				if (!Source)
					Source = AcquiredUpgradeDefinitions.FindRef(SourceId);
				const float SourceBase = UpgradeScalingBase(Source);
				const float* Stored = AccumulatedUpgradeMagnitudes.Find(SourceId);
				// Old Heavy Blade was a one-rank +40% tradeoff. Keep that damage
				// investment when migrating it to the ordinary scalable damage card.
				const float Magnitude =
				    Stored ? *Stored : Ranks * (SourceId == TEXT("SamuraiHeavyBlade") ? .4f : SourceBase);
				TotalRanks += Ranks;
				TotalMagnitude += SourceId == TargetId        ? Magnitude
				                  : SourceBase > SMALL_NUMBER ? Magnitude * TargetBase / SourceBase
				                                              : Ranks * TargetBase;
				ClearUpgradeModifiers(Source);
			}
			UpgradeLevels.Remove(SourceId);
			AccumulatedUpgradeMagnitudes.Remove(SourceId);
			AcquiredUpgradeDefinitions.Remove(SourceId);
		}
		if (bBanished)
			BanishedUpgrades.Add(TargetId);
		if (TotalRanks > 0)
		{
			UpgradeLevels.Add(TargetId, FMath::Min(TotalRanks, Target->MaxLevel));
			AccumulatedUpgradeMagnitudes.Add(TargetId, TotalMagnitude);
			AcquiredUpgradeDefinitions.Add(TargetId, Target);
		}
	}
}

void UPlayerUpgradeComponent::NormalizeSamuraiTradeoffUpgrades()
{
	const auto ConvertToDamage = [this](FName SourceId, FName TargetId) {
		const int32 Ranks = FMath::Max(0, UpgradeLevels.FindRef(SourceId));
		if (Ranks == 0)
			return;
		auto* Target = FindUpgradeDefinition(TargetId);
		if (!Target)
			return; // Keep the investment intact if its destination asset is unavailable.
		auto* Source = FindUpgradeDefinition(SourceId);
		if (!Source)
			Source = AcquiredUpgradeDefinitions.FindRef(SourceId);
		const float Base = UpgradeScalingBase(Target);
		const int32 PreviousRanks = FMath::Max(0, UpgradeLevels.FindRef(TargetId));
		const float* Stored = AccumulatedUpgradeMagnitudes.Find(TargetId);
		const float PreviousMagnitude = Stored ? *Stored : PreviousRanks * Base;
		// These status cards have fixed per-rank tuning: one spent rank becomes
		// one normal damage rank. Preserve all strength even when merging at cap.
		ClearUpgradeModifiers(Source);
		UpgradeLevels.Remove(SourceId);
		AccumulatedUpgradeMagnitudes.Remove(SourceId);
		AcquiredUpgradeDefinitions.Remove(SourceId);
		UpgradeLevels.Add(TargetId, FMath::Min(PreviousRanks + Ranks, UpgradeRankLimit(Target)));
		AccumulatedUpgradeMagnitudes.Add(TargetId, PreviousMagnitude + Ranks * Base);
		AcquiredUpgradeDefinitions.Add(TargetId, Target);
	};
	if (HasUpgradeId(TEXT("Iaijutsu")) && HasUpgradeId(TEXT("IaijutsuDashPact")))
	{
		ConvertToDamage(TEXT("IaijutsuMarkDamage"), TEXT("IaijutsuDamage"));
		// Only old saves can contain both pacts. Keep Relentless Steps and refund
		// the ineffective Focused Malice purchase as damage, removing its penalty.
		ConvertToDamage(TEXT("IaijutsuMarkPact"), TEXT("IaijutsuDamage"));
	}
	if (HasUpgradeId(TEXT("BladeWave")) && HasUpgradeId(TEXT("CrescentEruptionPact")))
	{
		ConvertToDamage(TEXT("CrescentSlow"), TEXT("CrescentDamage"));
		ConvertToDamage(TEXT("CrescentSlowDuration"), TEXT("CrescentDamage"));
	}
}

void UPlayerUpgradeComponent::ConvertNinjaScalingUpgrades()
{
	const bool bWheel = UpgradeLevels.FindRef(TEXT("GreatShuriken")) > 0;
	const bool bBarrage = UpgradeLevels.FindRef(TEXT("BarrageStance")) > 0;
	if (!bWheel && !bBarrage && UpgradeLevels.FindRef(TEXT("ReturningFang")) <= 0)
		return;
	const FName Pairs[][2] = {{TEXT("NinjaDamage"), bWheel ? TEXT("ShurikenDamage") : bBarrage ? TEXT("BarrageDamage") : TEXT("FangDamage")},
	                          {TEXT("NinjaSpeed"), bWheel ? TEXT("ShurikenSpeed") : bBarrage ? TEXT("BarrageSpeed") : TEXT("FangSpeed")},
	                          {TEXT("NinjaCoverage"), bWheel ? TEXT("ShurikenSize") : bBarrage ? TEXT("BarrageRange") : TEXT("FangRange")},
	                          {TEXT("CuttingReturn"), TEXT("FangResonance")},
	                          {TEXT("FinalPursuit"), TEXT("FangSplinter")},
	                          {TEXT("HeavyShuriken"), TEXT("ShurikenTempo")}};
	for (const auto& Pair : Pairs)
	{
		if (Pair[0] == TEXT("HeavyShuriken") && !bWheel) continue;
		if ((bBarrage || bWheel) && (Pair[0] == TEXT("CuttingReturn") || Pair[0] == TEXT("FinalPursuit")))
			continue;
		auto* Target = FindUpgradeDefinition(Pair[1]);
		if (!Target)
			continue;
		if (BanishedUpgrades.Remove(Pair[0]))
			BanishedUpgrades.Add(Pair[1]);
		const int32 Ranks = UpgradeLevels.FindRef(Pair[0]);
		if (Ranks <= 0)
			continue;
		auto* Source = AcquiredUpgradeDefinitions.FindRef(Pair[0]).Get();
		ClearUpgradeModifiers(Source);
		const float* Stored = AccumulatedUpgradeMagnitudes.Find(Pair[0]);
		const float Magnitude = Stored ? *Stored : Ranks * UpgradeScalingBase(Target);
		UpgradeLevels.FindOrAdd(Pair[1]) = FMath::Min(Target->MaxLevel, UpgradeLevels.FindRef(Pair[1]) + Ranks);
		if (Target->bUsesRolledRarity)
			AccumulatedUpgradeMagnitudes.FindOrAdd(Pair[1]) += Magnitude;
		AcquiredUpgradeDefinitions.Add(Pair[1], Target);
		UpgradeLevels.Remove(Pair[0]);
		AccumulatedUpgradeMagnitudes.Remove(Pair[0]);
		AcquiredUpgradeDefinitions.Remove(Pair[0]);
	}
	// Old shared Ninja cards do not belong to the new Fang tree. Credit their paid
	// ranks as Fang Damage, retaining the full bonus even when the displayed cap is reached.
	const FName DamageId = bWheel ? TEXT("ShurikenDamage") : bBarrage ? TEXT("BarrageDamage") : TEXT("FangDamage");
	if (auto* Damage = FindUpgradeDefinition(DamageId))
	{
		TArray<FName> Ids;
		UpgradeLevels.GetKeys(Ids);
		for (FName Id : Ids)
		{
			if (!(bBarrage ? BarrageBuild::IsLegacy(Id) : FangBuild::IsLegacyShared(Id)))
				continue;
			const int32 Ranks = UpgradeLevels.FindRef(Id);
			ClearUpgradeModifiers(AcquiredUpgradeDefinitions.FindRef(Id));
			UpgradeLevels.FindOrAdd(DamageId) = FMath::Min(Damage->MaxLevel, UpgradeLevels.FindRef(DamageId) + Ranks);
			AccumulatedUpgradeMagnitudes.FindOrAdd(DamageId) += Ranks * UpgradeScalingBase(Damage);
			AcquiredUpgradeDefinitions.Add(DamageId, Damage);
			UpgradeLevels.Remove(Id);
			AccumulatedUpgradeMagnitudes.Remove(Id);
			AcquiredUpgradeDefinitions.Remove(Id);
		}
	}
}
