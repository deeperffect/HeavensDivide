#include "PlayerUpgradeComponent.h"
#include "BarrageBuild.h"
#include "ShurikenBuild.h"
#include "FangBuild.h"
#include "PlayerUpgradeRules.h"
#include "SurvivorPlayerController.h"
#include "SynergyMetaProgressionSubsystem.h"

using namespace PlayerUpgradeRules;

UPlayerUpgradeComponent::UPlayerUpgradeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	RarityTimeBrackets = {{0.0f, 75.0f, 23.0f, 2.0f}, {300.0f, 55.0f, 38.0f, 7.0f}, {600.0f, 40.0f, 45.0f, 15.0f}};
}

int32 UPlayerUpgradeComponent::GetUpgradeLevel(UUpgradeDefinition* Upgrade) const
{
	if (!IsValidUpgradeDefinition(Upgrade))
	{
		return 0;
	}

	if (const int32* FoundLevel = UpgradeLevels.Find(Upgrade->UpgradeId))
	{
		return *FoundLevel;
	}

	return 0;
}

int32 UPlayerUpgradeComponent::GetUpgradeLevelById(FName UpgradeId) const
{
	return IsUpgradeSuppressed(UpgradeId) ? 0 : UpgradeLevels.FindRef(UpgradeId);
}

bool UPlayerUpgradeComponent::HasUpgradeId(FName UpgradeId) const
{
	return GetUpgradeLevelById(UpgradeId) > 0;
}

bool UPlayerUpgradeComponent::CanAcquireUpgrade(UUpgradeDefinition* Upgrade) const
{
	if (!IsValidUpgradeDefinition(Upgrade))
		return false;
	const FName Id = Upgrade->UpgradeId;
	const bool bWheel = HasUpgradeId(TEXT("GreatShuriken"));
	if (ShurikenBuild::IsUpgrade(Id) && !bWheel) return false;
	const bool bBarrage = HasUpgradeId(TEXT("BarrageStance"));
	const bool bFang = HasUpgradeId(TEXT("ReturningFang"));
	if (BarrageBuild::IsUpgrade(Id) && !bBarrage)
		return false;
	if (FangBuild::IsUpgrade(Id) && !bFang)
		return false;
	if ((bBarrage || bFang || bWheel) && (Id == TEXT("NinjaDamage") || Id == TEXT("NinjaSpeed") || Id == TEXT("NinjaCoverage")))
		return false;
	if (HasSamuraiTradeoffConflict(Id))
		return false;
	// Before choosing a stance, offer the common investments. Afterwards only
	// their selected forms are eligible; acquisition of a stance converts ranks.
	if (IsSamuraiMeleeScalingUpgrade(Id) && (HasUpgradeId(TEXT("Iaijutsu")) || HasUpgradeId(TEXT("BladeWave"))))
		return false;
	if (IsCrescentUpgrade(Id) && !HasUpgradeId(TEXT("BladeWave")))
		return false;
	if (IsIaijutsuUpgrade(Id) && !HasUpgradeId(TEXT("Iaijutsu")))
		return false;
	if (IsBloodUpgrade(Id) && !HasUpgradeId(TEXT("BattleStance")))
		return false;
	if (Id == TEXT("BloodTransfer") && HasUpgradeId(TEXT("BloodDetonation")))
		return false;
	if (Id == TEXT("BloodTransferArea") && !HasUpgradeId(TEXT("BloodTransfer")) &&
	    !HasUpgradeId(TEXT("BloodDetonation")))
		return false;
	// Enforce stance exclusivity by stable ID even for stale pre-overhaul assets
	// whose Blade Wave definition still has no exclusivity group.
	if (Id == TEXT("BladeWave") || Id == TEXT("Iaijutsu") || Id == TEXT("BattleStance"))
		for (FName StanceId : {FName(TEXT("BladeWave")), FName(TEXT("Iaijutsu")), FName(TEXT("BattleStance"))})
			if (StanceId != Id && HasUpgradeId(StanceId))
				return false;
	if (BanishedUpgrades.Contains(Id))
		return false;
	if (Upgrade->Category == EUpgradeCategory::NinjaTrial)
		return false;
	if (Upgrade->Category == EUpgradeCategory::SamuraiTrial ||
	    Upgrade->SpecialEffects.Contains(EUpgradeSpecialEffect::SamuraiCleaver) ||
	    Upgrade->SpecialEffects.Contains(EUpgradeSpecialEffect::SamuraiDuelist) ||
	    Upgrade->SpecialEffects.Contains(EUpgradeSpecialEffect::SamuraiDeathblow))
		return false;
	bool bMetaEligible = true;
	if (Upgrade->Category == EUpgradeCategory::Synergy)
	{
		const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		const USynergyMetaProgressionSubsystem* MetaSubsystem =
		    GameInstance ? GameInstance->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
		bMetaEligible = MetaSubsystem ? MetaSubsystem->IsUpgradeMetaEligible(Upgrade)
		                              : !Upgrade->bRequiresMetaUnlock || Upgrade->bUnlockedByDefault;
	}

	bool bExclusivityEligible = true;
	if (!Upgrade->ExclusivityGroup.IsNone())
	{
		for (const TPair<FName, TObjectPtr<UUpgradeDefinition>>& Pair : AcquiredUpgradeDefinitions)
		{
			const UUpgradeDefinition* Acquired = Pair.Value;
			if (Acquired && Acquired != Upgrade && Acquired->ExclusivityGroup == Upgrade->ExclusivityGroup &&
			    GetUpgradeLevelById(Pair.Key) > 0)
			{
				bExclusivityEligible = false;
				break;
			}
		}
	}

	return bMetaEligible && bExclusivityEligible && GetUpgradeLevel(Upgrade) < UpgradeRankLimit(Upgrade) &&
	       MeetsPrerequisites(Upgrade);
}

bool UPlayerUpgradeComponent::AcquireUpgrade(UUpgradeDefinition* Upgrade)
{
	const float Magnitude =
	    Upgrade && Upgrade->bUsesRolledRarity ? ResolveMagnitude(Upgrade, EUpgradeRarity::Common) : 0.0f;
	return AcquireUpgradeResolved(Upgrade, Magnitude, EUpgradeRarity::Common);
}

bool UPlayerUpgradeComponent::AcquireUpgradeResolved(UUpgradeDefinition* Upgrade, float ResolvedMagnitude,
                                                     EUpgradeRarity Rarity)
{
	if (!CanAcquireUpgrade(Upgrade))
	{
		return false;
	}

	const int32 NewLevel = GetUpgradeLevel(Upgrade) + 1;
	UpgradeLevels.FindOrAdd(Upgrade->UpgradeId) = NewLevel;
	AcquiredUpgradeDefinitions.FindOrAdd(Upgrade->UpgradeId) = Upgrade;
	if (Upgrade->bUsesRolledRarity)
	{
		AccumulatedUpgradeMagnitudes.FindOrAdd(Upgrade->UpgradeId) += ResolvedMagnitude;
		UE_LOG(LogTemp, Log, TEXT("[UpgradeRarity] ACQUIRED %s Rarity=%s Magnitude=%.3f Accumulated=%.3f"),
		       *Upgrade->UpgradeId.ToString(), *GetRarityDisplayName(Rarity).ToString(), ResolvedMagnitude,
		       AccumulatedUpgradeMagnitudes.FindRef(Upgrade->UpgradeId));
	}
	else if (IsSamuraiScalingUpgrade(Upgrade->UpgradeId))
	{
		// Legacy stance-specific ranks have no stored magnitude. Seed them before
		// adding a rank so conversion bonuses and earlier ranks are both retained.
		const float Base = UpgradeScalingBase(Upgrade);
		if (!AccumulatedUpgradeMagnitudes.Contains(Upgrade->UpgradeId))
			AccumulatedUpgradeMagnitudes.Add(Upgrade->UpgradeId, Base * (NewLevel - 1));
		AccumulatedUpgradeMagnitudes[Upgrade->UpgradeId] += Base;
	}

	FinalizeUpgradeAcquisition(Upgrade, NewLevel);
	return true;
}

bool UPlayerUpgradeComponent::HasSamuraiTradeoffConflict(FName Id) const
{
	if (HasUpgradeId(TEXT("IaijutsuDashPact")) && (Id == TEXT("IaijutsuMarkDamage") || Id == TEXT("IaijutsuMarkPact")))
		return true;
	if (Id == TEXT("IaijutsuDashPact") && HasUpgradeId(TEXT("IaijutsuMarkPact")))
		return true;
	return (Id == TEXT("CrescentSlow") || Id == TEXT("CrescentSlowDuration")) &&
	       HasUpgradeId(TEXT("CrescentEruptionPact"));
}

float UPlayerUpgradeComponent::GetAccumulatedUpgradeMagnitude(FName UpgradeId) const
{
	return IsUpgradeSuppressed(UpgradeId) ? 0.f : AccumulatedUpgradeMagnitudes.FindRef(UpgradeId);
}

bool UPlayerUpgradeComponent::DebugAcquireUpgrade(UUpgradeDefinition* Upgrade)
{
	const bool bAcquired = AcquireUpgrade(Upgrade);
	if (!bAcquired)
	{
		UE_LOG(
		    LogTemp, Warning,
		    TEXT("DebugAcquireUpgrade failed: Upgrade=%s Valid=%s CurrentLevel=%d MaxLevel=%d MeetsPrerequisites=%s"),
		    *UpgradeToLogString(Upgrade), IsValidUpgradeDefinition(Upgrade) ? TEXT("true") : TEXT("false"),
		    GetUpgradeLevel(Upgrade), Upgrade ? Upgrade->MaxLevel : 0,
		    MeetsPrerequisites(Upgrade) ? TEXT("true") : TEXT("false"));
	}

	return bAcquired;
}

bool UPlayerUpgradeComponent::DebugForceAcquireUpgrade(UUpgradeDefinition* Upgrade, int32 Level)
{
#if !UE_BUILD_SHIPPING
	if (Upgrade && HasSamuraiTradeoffConflict(Upgrade->UpgradeId))
		return false;
	if (Upgrade && IsSamuraiMeleeScalingUpgrade(Upgrade->UpgradeId) &&
	    (HasUpgradeId(TEXT("Iaijutsu")) || HasUpgradeId(TEXT("BladeWave"))))
		return false;
	if (!IsValidUpgradeDefinition(Upgrade))
	{
		UE_LOG(LogTemp, Warning, TEXT("DebugForceAcquireUpgrade failed: invalid upgrade."));
		return false;
	}

	const int32 NewLevel = FMath::Clamp(Level, 1, FMath::Max(1, UpgradeRankLimit(Upgrade)));
	UpgradeLevels.FindOrAdd(Upgrade->UpgradeId) = NewLevel;
	AcquiredUpgradeDefinitions.FindOrAdd(Upgrade->UpgradeId) = Upgrade;
	if (Upgrade->bUsesRolledRarity)
	{
		AccumulatedUpgradeMagnitudes.FindOrAdd(Upgrade->UpgradeId) =
		    ResolveMagnitude(Upgrade, EUpgradeRarity::Common) * NewLevel;
	}
	else if (IsSamuraiScalingUpgrade(Upgrade->UpgradeId))
		AccumulatedUpgradeMagnitudes.FindOrAdd(Upgrade->UpgradeId) = UpgradeScalingBase(Upgrade) * NewLevel;

	FinalizeUpgradeAcquisition(Upgrade, NewLevel);

	UE_LOG(LogTemp, Log, TEXT("DebugForceAcquireUpgrade succeeded: %s"), *UpgradeToLogString(Upgrade));
	return true;
#else
	UE_LOG(LogTemp, Warning, TEXT("DebugForceAcquireUpgrade is disabled in shipping builds."));
	return false;
#endif
}

bool UPlayerUpgradeComponent::DebugBeginUpgradeSelection(int32 CategoryChoiceCount)
{
	return BeginUpgradeSelection(CategoryChoiceCount);
}

bool UPlayerUpgradeComponent::DebugSelectCategory(EUpgradeCategory Category, int32 UpgradeChoiceCount)
{
	return SelectCategory(Category, UpgradeChoiceCount);
}

bool UPlayerUpgradeComponent::DebugSelectUpgrade(UUpgradeDefinition* Upgrade)
{
	return SelectUpgrade(Upgrade);
}

int32 UPlayerUpgradeComponent::GetSpecialEffectLevel(EUpgradeSpecialEffect SpecialEffect) const
{
	if (SpecialEffect == EUpgradeSpecialEffect::None)
	{
		return 0;
	}

	int32 TotalLevel = 0;
	for (const TPair<FName, TObjectPtr<UUpgradeDefinition>>& UpgradePair : AcquiredUpgradeDefinitions)
	{
		const UUpgradeDefinition* Upgrade = UpgradePair.Value;
		if (IsValidUpgradeDefinition(Upgrade) && Upgrade->SpecialEffects.Contains(SpecialEffect))
		{
			TotalLevel += GetUpgradeLevel(UpgradePair.Value);
		}
	}

	return TotalLevel;
}

UUpgradeDefinition* UPlayerUpgradeComponent::GetAcquiredUpgradeWithSpecialEffect(
    EUpgradeSpecialEffect SpecialEffect) const
{
	if (SpecialEffect == EUpgradeSpecialEffect::None)
	{
		return nullptr;
	}

	for (const TPair<FName, TObjectPtr<UUpgradeDefinition>>& UpgradePair : AcquiredUpgradeDefinitions)
	{
		UUpgradeDefinition* Upgrade = UpgradePair.Value;
		if (IsValidUpgradeDefinition(Upgrade) && Upgrade->SpecialEffects.Contains(SpecialEffect) &&
		    GetUpgradeLevel(Upgrade) > 0)
		{
			return Upgrade;
		}
	}

	return nullptr;
}

bool UPlayerUpgradeComponent::IsValidUpgradeDefinition(const UUpgradeDefinition* Upgrade) const
{
	return Upgrade && Upgrade->MaxLevel > 0 && !IsUpgradeSuppressed(Upgrade->UpgradeId) &&
	       !IsRetiredUpgrade(Upgrade->BuildFamilyId);
}

bool UPlayerUpgradeComponent::IsUpgradeSuppressed(FName UpgradeId) const
{
	// Inspect raw stance ranks here: querying HasUpgradeId would recurse.
	return UpgradeId.IsNone() || IsRetiredUpgrade(UpgradeId) || IsSamuraiUpgradeTemporarilyDisabled(UpgradeId) ||
	       (UpgradeLevels.FindRef(TEXT("GreatShuriken")) > 0 && FangBuild::IsLegacyShared(UpgradeId)) ||
	       (UpgradeLevels.FindRef(TEXT("BarrageStance")) > 0 && BarrageBuild::IsLegacy(UpgradeId)) ||
	       (UpgradeLevels.FindRef(TEXT("ReturningFang")) > 0 && FangBuild::IsLegacyShared(UpgradeId));
}

bool UPlayerUpgradeComponent::MeetsPrerequisites(const UUpgradeDefinition* Upgrade) const
{
	if (!IsValidUpgradeDefinition(Upgrade))
	{
		return false;
	}

	for (const FName PrerequisiteUpgradeId : Upgrade->PrerequisiteUpgradeIds)
	{
		if (!HasUpgradeId(PrerequisiteUpgradeId))
		{
			return false;
		}
	}

	for (const FUpgradePrerequisiteRequirement& Requirement : Upgrade->PrerequisiteRequirements)
	{
		if (Requirement.UpgradeId.IsNone() ||
		    GetUpgradeLevelById(Requirement.UpgradeId) < FMath::Max(1, Requirement.MinimumLevel))
		{
			return false;
		}
	}

	return true;
}

FString UPlayerUpgradeComponent::UpgradeToLogString(const UUpgradeDefinition* Upgrade) const
{
	if (!Upgrade)
	{
		return TEXT("None");
	}

	const FString DisplayName =
	    Upgrade->DisplayName.IsEmpty() ? Upgrade->UpgradeId.ToString() : Upgrade->DisplayName.ToString();
	return FString::Printf(TEXT("%s (%s) Level=%d/%d"), *DisplayName, *Upgrade->UpgradeId.ToString(),
	                       GetUpgradeLevel(const_cast<UUpgradeDefinition*>(Upgrade)), Upgrade->MaxLevel);
}

UUpgradeDefinition* UPlayerUpgradeComponent::FindUpgradeDefinition(FName Id) const
{
	for (UUpgradeDefinition* Definition : UpgradePool)
		if (Definition && Definition->UpgradeId == Id)
			return Definition;
	return nullptr;
}

void UPlayerUpgradeComponent::FinalizeUpgradeAcquisition(UUpgradeDefinition* Upgrade, int32 NewLevel)
{
	if (Upgrade->UpgradeId == TEXT("BattleStance") || Upgrade->UpgradeId == TEXT("Iaijutsu") ||
	    Upgrade->UpgradeId == TEXT("BladeWave"))
	{
		ConvertSamuraiScalingUpgrades();
		RebuildAllUpgradeModifiers();
	}
	else if (Upgrade->UpgradeId == TEXT("ReturningFang") || Upgrade->UpgradeId == TEXT("BarrageStance") || Upgrade->UpgradeId == TEXT("GreatShuriken"))
	{
		ConvertNinjaScalingUpgrades();
		RebuildAllUpgradeModifiers();
	}
	else if (Upgrade->UpgradeId == TEXT("IaijutsuDashPact") || Upgrade->UpgradeId == TEXT("CrescentEruptionPact"))
	{
		NormalizeSamuraiTradeoffUpgrades();
		RebuildAllUpgradeModifiers();
	}
	else
		RebuildUpgradeModifiers(Upgrade, NewLevel);
	if (Upgrade->InvestmentOwner == EUpgradeInvestmentOwner::Samurai)
	{
		++SamuraiMasteryPoints;
		UE_LOG(LogTemp, Log, TEXT("[Mastery] Samurai Points=%d Power=%.3f Upgrade=%s Level=%d"), SamuraiMasteryPoints,
		       GetSamuraiPowerMultiplier(), *Upgrade->UpgradeId.ToString(), NewLevel);
	}
	else if (Upgrade->InvestmentOwner == EUpgradeInvestmentOwner::Ninja)
	{
		++NinjaMasteryPoints;
		UE_LOG(LogTemp, Log, TEXT("[Mastery] Ninja Points=%d Power=%.3f Upgrade=%s Level=%d"), NinjaMasteryPoints,
		       GetNinjaPowerMultiplier(), *Upgrade->UpgradeId.ToString(), NewLevel);
	}
	LogPlayerUpgradeStats();

	OnUpgradeAcquired.Broadcast(Upgrade, NewLevel);
	OnUpgradeLevelChanged.Broadcast(Upgrade, NewLevel);
}
