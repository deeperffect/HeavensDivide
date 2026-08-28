// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerUpgradeComponent.h"

#include "AutoAttackComponent.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "CharacterStatsComponent.h"
#include "ExperienceComponent.h"
#include "HealthComponent.h"
#include "HAL/IConsoleManager.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "SharedPlayerStatsComponent.h"
#include "SurvivorPlayerController.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "UpgradeDefinition.h"

static TAutoConsoleVariable<int32> CVarHDLogPlayerUpgradeStats(
	TEXT("hd.LogPlayerUpgradeStats"),
	0,
	TEXT("Logs player upgrade/stat summary after upgrade stat rebuilds when enabled."));

UPlayerUpgradeComponent::UPlayerUpgradeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	RarityTimeBrackets = {
		{ 0.0f, 75.0f, 23.0f, 2.0f },
		{ 300.0f, 55.0f, 38.0f, 7.0f },
		{ 600.0f, 40.0f, 45.0f, 15.0f }
	};
}

void UPlayerUpgradeComponent::CaptureRunState(FPlayerUpgradeRunState& OutState) const
{
	OutState.Levels = UpgradeLevels;
	OutState.AccumulatedMagnitudes = AccumulatedUpgradeMagnitudes;
	OutState.Definitions = AcquiredUpgradeDefinitions;
	OutState.SamuraiMastery = SamuraiMasteryPoints;
	OutState.NinjaMastery = NinjaMasteryPoints;
}

void UPlayerUpgradeComponent::RestoreRunState(const FPlayerUpgradeRunState& State)
{
	UpgradeLevels = State.Levels;
	AccumulatedUpgradeMagnitudes = State.AccumulatedMagnitudes;
	AcquiredUpgradeDefinitions = State.Definitions;
	SamuraiMasteryPoints = FMath::Max(0, State.SamuraiMastery);
	NinjaMasteryPoints = FMath::Max(0, State.NinjaMastery);
	ClearCurrentOffer();
	RebuildAllUpgradeModifiers();
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
	if (UpgradeId.IsNone())
	{
		return 0;
	}

	if (const int32* FoundLevel = UpgradeLevels.Find(UpgradeId))
	{
		return *FoundLevel;
	}

	return 0;
}

bool UPlayerUpgradeComponent::HasUpgradeId(FName UpgradeId) const
{
	return GetUpgradeLevelById(UpgradeId) > 0;
}

bool UPlayerUpgradeComponent::CanAcquireUpgrade(UUpgradeDefinition* Upgrade) const
{
	bool bMetaEligible = true;
	if (Upgrade && Upgrade->Category == EUpgradeCategory::Synergy)
	{
		const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		const USynergyMetaProgressionSubsystem* MetaSubsystem = GameInstance ? GameInstance->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
		bMetaEligible = MetaSubsystem ? MetaSubsystem->IsUpgradeMetaEligible(Upgrade) : !Upgrade->bRequiresMetaUnlock || Upgrade->bUnlockedByDefault;
	}

	bool bExclusivityEligible = true;
	if (Upgrade && !Upgrade->ExclusivityGroup.IsNone())
	{
		for (const TPair<FName, TObjectPtr<UUpgradeDefinition>>& Pair : AcquiredUpgradeDefinitions)
		{
			const UUpgradeDefinition* Acquired = Pair.Value;
			if (Acquired && Acquired != Upgrade && Acquired->ExclusivityGroup == Upgrade->ExclusivityGroup && GetUpgradeLevelById(Pair.Key) > 0)
			{
				bExclusivityEligible = false;
				break;
			}
		}
	}

	return IsValidUpgradeDefinition(Upgrade)
		&& bMetaEligible
		&& bExclusivityEligible
		&& GetUpgradeLevel(Upgrade) < Upgrade->MaxLevel
		&& MeetsPrerequisites(Upgrade);
}

bool UPlayerUpgradeComponent::AcquireUpgrade(UUpgradeDefinition* Upgrade)
{
	const float Magnitude = Upgrade && Upgrade->bUsesRolledRarity ? ResolveMagnitude(Upgrade, EUpgradeRarity::Common) : 0.0f;
	return AcquireUpgradeResolved(Upgrade, Magnitude, EUpgradeRarity::Common);
}

bool UPlayerUpgradeComponent::AcquireUpgradeResolved(UUpgradeDefinition* Upgrade, float ResolvedMagnitude, EUpgradeRarity Rarity)
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

	RebuildUpgradeModifiers(Upgrade, NewLevel);
	if (Upgrade->InvestmentOwner == EUpgradeInvestmentOwner::Samurai)
	{
		++SamuraiMasteryPoints;
		UE_LOG(LogTemp, Log, TEXT("[Mastery] Samurai Points=%d Power=%.3f Upgrade=%s Level=%d"), SamuraiMasteryPoints, GetSamuraiPowerMultiplier(), *Upgrade->UpgradeId.ToString(), NewLevel);
	}
	else if (Upgrade->InvestmentOwner == EUpgradeInvestmentOwner::Ninja)
	{
		++NinjaMasteryPoints;
		UE_LOG(LogTemp, Log, TEXT("[Mastery] Ninja Points=%d Power=%.3f Upgrade=%s Level=%d"), NinjaMasteryPoints, GetNinjaPowerMultiplier(), *Upgrade->UpgradeId.ToString(), NewLevel);
	}
	LogPlayerUpgradeStats();

	OnUpgradeAcquired.Broadcast(Upgrade, NewLevel);
	OnUpgradeLevelChanged.Broadcast(Upgrade, NewLevel);
	return true;
}

void UPlayerUpgradeComponent::RebuildAllUpgradeModifiers()
{
	UE_LOG(LogTemp, Log, TEXT("RebuildAllUpgradeModifiers: UpgradeCount=%d"), AcquiredUpgradeDefinitions.Num());

	for (const TPair<FName, TObjectPtr<UUpgradeDefinition>>& UpgradePair : AcquiredUpgradeDefinitions)
	{
		UUpgradeDefinition* Upgrade = UpgradePair.Value;
		if (!IsValidUpgradeDefinition(Upgrade))
		{
			continue;
		}

		const int32 Level = GetUpgradeLevel(Upgrade);
		if (Level > 0)
		{
			RebuildUpgradeModifiers(Upgrade, Level);
		}
	}

	LogPlayerUpgradeStats();
}

void UPlayerUpgradeComponent::LogPlayerUpgradeStats() const
{
	if (CVarHDLogPlayerUpgradeStats.GetValueOnGameThread() == 0)
	{
		return;
	}

	const ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(GetOwner());
	const UCharacterManagerComponent* CharacterManager = SurvivorController ? SurvivorController->GetCharacterManager() : nullptr;
	const USharedPlayerStatsComponent* SharedStats = SurvivorController ? SurvivorController->GetSharedPlayerStats() : nullptr;
	const UHealthComponent* PlayerHealth = SurvivorController ? SurvivorController->GetPlayerHealthComponent() : nullptr;
	const ACharacterBase* Samurai = CharacterManager ? CharacterManager->GetSamurai() : nullptr;
	const ACharacterBase* Ninja = CharacterManager ? CharacterManager->GetNinja() : nullptr;
	const UAutoAttackComponent* SamuraiAttack = Samurai ? Samurai->FindComponentByClass<UAutoAttackComponent>() : nullptr;
	const UAutoAttackComponent* NinjaAttack = Ninja ? Ninja->FindComponentByClass<UAutoAttackComponent>() : nullptr;
	const UCharacterStatsComponent* SamuraiStats = Samurai ? Samurai->GetCharacterStats() : nullptr;
	const UCharacterStatsComponent* NinjaStats = Ninja ? Ninja->GetCharacterStats() : nullptr;

	UE_LOG(LogTemp, Log, TEXT("=== PLAYER UPGRADE STATS ==="));
	UE_LOG(LogTemp, Log, TEXT("SHARED"));
	UE_LOG(LogTemp, Log, TEXT("Move Speed Multiplier: %.3f"), SharedStats ? SharedStats->GetFinalMoveSpeedMultiplier() : 1.0f);
	UE_LOG(LogTemp, Log, TEXT("Max Health: Current %.2f / Final %.2f"), PlayerHealth ? PlayerHealth->GetCurrentHealth() : 0.0f, PlayerHealth ? PlayerHealth->GetMaxHealth() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("Pickup Radius Multiplier: %.3f"), SharedStats ? SharedStats->GetFinalPickupRadiusMultiplier() : 1.0f);
	UE_LOG(LogTemp, Log, TEXT("Global Damage Multiplier: %.3f"), SharedStats ? SharedStats->GetFinalDamageMultiplier() : 1.0f);
	UE_LOG(LogTemp, Log, TEXT("SAMURAI"));
	UE_LOG(LogTemp, Log, TEXT("Damage: Base %.2f -> Final %.2f"), SamuraiAttack ? SamuraiAttack->GetBaseAttackDamage() : 0.0f, SamuraiAttack ? SamuraiAttack->GetEffectiveAttackDamage() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("Attack Interval: Base %.3f -> Final %.3f"), SamuraiAttack ? SamuraiAttack->GetBaseAttackInterval() : 0.0f, SamuraiAttack ? SamuraiAttack->GetEffectiveAttackInterval() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("Area Radius: Base %.2f -> Final %.2f"), SamuraiAttack ? SamuraiAttack->GetBaseAttackRadius() : 0.0f, SamuraiAttack ? SamuraiAttack->GetEffectiveAttackRadius() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("HP Regen/sec: %.3f"), SamuraiStats ? SamuraiStats->GetFinalHPRegenPerSecond() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("Damage Reduction: %.1f%%"), SamuraiStats ? SamuraiStats->GetFinalDamageReduction() * 100.0f : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("NINJA"));
	UE_LOG(LogTemp, Log, TEXT("Damage: Base %.2f -> Final %.2f"), NinjaAttack ? NinjaAttack->GetBaseAttackDamage() : 0.0f, NinjaAttack ? NinjaAttack->GetEffectiveAttackDamage() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("Attack Interval: Base %.3f -> Final %.3f"), NinjaAttack ? NinjaAttack->GetBaseAttackInterval() : 0.0f, NinjaAttack ? NinjaAttack->GetEffectiveAttackInterval() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("Projectile Count: Base 1 -> Final %d"), NinjaAttack ? NinjaAttack->GetEffectiveProjectileCount() : 1);
	UE_LOG(LogTemp, Log, TEXT("Projectile Pierce Bonus: %d"), NinjaAttack ? NinjaAttack->GetEffectiveProjectilePierceBonus() : 0);
	UE_LOG(LogTemp, Log, TEXT("Projectile Speed: Base %.2f -> Final %.2f"), NinjaAttack ? NinjaAttack->GetBaseProjectileSpeed() : 0.0f, NinjaAttack ? NinjaAttack->GetEffectiveProjectileSpeed() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("Health On Kill: %.3f"), NinjaStats ? NinjaStats->GetFinalHealthOnKill() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("Dodge Chance: %.1f%%"), NinjaStats ? NinjaStats->GetFinalDodgeChance() * 100.0f : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("============================"));
}

bool UPlayerUpgradeComponent::IsCategoryUnlocked(EUpgradeCategory Category) const
{
	if (Category != EUpgradeCategory::Synergy)
	{
		return true;
	}

	return GetCurrentPlayerLevel() >= SynergyUnlockLevel
		&& HasAcquiredUpgradeInCategory(EUpgradeCategory::Samurai)
		&& HasAcquiredUpgradeInCategory(EUpgradeCategory::Ninja);
}

TArray<EUpgradeCategory> UPlayerUpgradeComponent::GetEligibleCategories() const
{
	TArray<EUpgradeCategory> EligibleCategories;
	constexpr EUpgradeCategory Categories[] =
	{
		EUpgradeCategory::Samurai,
		EUpgradeCategory::Ninja,
		EUpgradeCategory::Global,
		EUpgradeCategory::Synergy
	};

	for (const EUpgradeCategory Category : Categories)
	{
		if (IsCategoryUnlocked(Category) && GetEligibleUpgradesForCategory(Category).Num() > 0)
		{
			EligibleCategories.Add(Category);
		}
	}

	return EligibleCategories;
}

TArray<EUpgradeCategory> UPlayerUpgradeComponent::RollCategoryChoices(int32 ChoiceCount)
{
	TArray<EUpgradeCategory> RemainingCategories = GetEligibleCategories();
	TArray<EUpgradeCategory> OfferedCategories;
	const int32 DesiredChoiceCount = FMath::Max(0, ChoiceCount);

	while (OfferedCategories.Num() < DesiredChoiceCount && RemainingCategories.Num() > 0)
	{
		TArray<float> Weights;
		Weights.Reserve(RemainingCategories.Num());

		for (const EUpgradeCategory Category : RemainingCategories)
		{
			Weights.Add(GetCategoryRollWeight(Category));
		}

		const EUpgradeCategory PickedCategory = PickWeightedCategory(RemainingCategories, Weights);
		OfferedCategories.Add(PickedCategory);
		RemainingCategories.Remove(PickedCategory);
	}

	UpdateCategoryBadLuckHistory(OfferedCategories);
	return OfferedCategories;
}

TArray<UUpgradeDefinition*> UPlayerUpgradeComponent::GetEligibleUpgradesForCategory(EUpgradeCategory Category) const
{
	TArray<UUpgradeDefinition*> CandidateUpgrades;
	for (UUpgradeDefinition* Upgrade : UpgradePool)
	{
		if (Upgrade && Upgrade->Category == Category)
		{
			CandidateUpgrades.Add(Upgrade);
		}
	}

	return GetEligibleUpgrades(CandidateUpgrades);
}

TArray<UUpgradeDefinition*> UPlayerUpgradeComponent::RollUpgradeChoices(EUpgradeCategory Category, int32 ChoiceCount) const
{
	TArray<UUpgradeDefinition*> RemainingUpgrades = GetEligibleUpgradesForCategory(Category);
	TArray<UUpgradeDefinition*> OfferedUpgrades;
	const int32 DesiredChoiceCount = FMath::Max(0, ChoiceCount);

	while (OfferedUpgrades.Num() < DesiredChoiceCount && RemainingUpgrades.Num() > 0)
	{
		const int32 PickedIndex = FMath::RandRange(0, RemainingUpgrades.Num() - 1);
		OfferedUpgrades.Add(RemainingUpgrades[PickedIndex]);
		RemainingUpgrades.RemoveAtSwap(PickedIndex);
	}

	return OfferedUpgrades;
}

bool UPlayerUpgradeComponent::BeginUpgradeSelection(int32 CategoryChoiceCount)
{
	ClearCurrentOffer();
	CurrentCategoryChoices = RollCategoryChoices(CategoryChoiceCount);

	UE_LOG(LogTemp, Log, TEXT("=== UPGRADE SELECTION START ==="));
	UE_LOG(LogTemp, Log, TEXT("Eligible categories:"));
	for (const EUpgradeCategory Category : GetEligibleCategories())
	{
		UE_LOG(LogTemp, Log, TEXT("  %s Weight=%.2f Misses=%d"),
			*CategoryToString(Category),
			GetCategoryRollWeight(Category),
			CategoryRollsSinceLastOffered.FindRef(Category));
	}

	UE_LOG(LogTemp, Log, TEXT("Offered categories:"));
	for (const EUpgradeCategory Category : CurrentCategoryChoices)
	{
		UE_LOG(LogTemp, Log, TEXT("  %s"), *CategoryToString(Category));
	}

	return CurrentCategoryChoices.Num() > 0;
}

bool UPlayerUpgradeComponent::BeginDirectUpgradeSelection(int32 UpgradeChoiceCount)
{
	ClearCurrentOffer();
	TArray<UUpgradeDefinition*> RemainingUpgrades;
	for (UUpgradeDefinition* Upgrade : UpgradePool)
	{
		if (Upgrade && IsCategoryUnlocked(Upgrade->Category) && CanAcquireUpgrade(Upgrade))
		{
			RemainingUpgrades.Add(Upgrade);
		}
	}

	const int32 DesiredChoiceCount = FMath::Max(0, UpgradeChoiceCount);
	while (CurrentUpgradeChoices.Num() < DesiredChoiceCount && RemainingUpgrades.Num() > 0)
	{
		const int32 PickedIndex = FMath::RandRange(0, RemainingUpgrades.Num() - 1);
		CurrentUpgradeChoices.Add(RemainingUpgrades[PickedIndex]);
		RemainingUpgrades.RemoveAtSwap(PickedIndex);
	}

	bHasSelectedCategory = CurrentUpgradeChoices.Num() > 0;
	BuildOffersFromCurrentChoices(false);
	return bHasSelectedCategory;
}

bool UPlayerUpgradeComponent::BeginDirectCategoryUpgradeSelection(EUpgradeCategory Category, int32 UpgradeChoiceCount)
{
	ClearCurrentOffer();
	SelectedCategory = Category;
	CurrentUpgradeChoices = RollUpgradeChoices(Category, UpgradeChoiceCount);
	bHasSelectedCategory = CurrentUpgradeChoices.Num() > 0;
	BuildOffersFromCurrentChoices(false);
	return bHasSelectedCategory;
}

int32 UPlayerUpgradeComponent::GetMasteryPoints(EUpgradeInvestmentOwner InvestmentOwner) const
{
	switch (InvestmentOwner)
	{
	case EUpgradeInvestmentOwner::Samurai: return SamuraiMasteryPoints;
	case EUpgradeInvestmentOwner::Ninja: return NinjaMasteryPoints;
	default: return 0;
	}
}

float UPlayerUpgradeComponent::GetSamuraiPowerMultiplier() const
{
	return 1.0f + FMath::Max(0, SamuraiMasteryPoints) * FMath::Max(0.0f, PowerPerMasteryPoint);
}

float UPlayerUpgradeComponent::GetNinjaPowerMultiplier() const
{
	return 1.0f + FMath::Max(0, NinjaMasteryPoints) * FMath::Max(0.0f, PowerPerMasteryPoint);
}

TArray<UUpgradeDefinition*> UPlayerUpgradeComponent::GetLockedSynergyDiscoveryCandidates() const
{
	TArray<UUpgradeDefinition*> Candidates;
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const USynergyMetaProgressionSubsystem* MetaSubsystem = GameInstance ? GameInstance->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
	if (!MetaSubsystem) return Candidates;

	for (UUpgradeDefinition* Upgrade : UpgradePool)
	{
		if (Upgrade && Upgrade->Category == EUpgradeCategory::Synergy && Upgrade->bRequiresMetaUnlock
			&& !Upgrade->MetaUnlockId.IsNone() && !MetaSubsystem->IsSynergyUpgradeUnlocked(Upgrade->MetaUnlockId))
		{
			Candidates.AddUnique(Upgrade);
		}
	}
	return Candidates;
}

bool UPlayerUpgradeComponent::BeginSynergyDiscoverySelection(int32 UpgradeChoiceCount)
{
	ClearCurrentOffer();
	SelectedCategory = EUpgradeCategory::Synergy;
	TArray<UUpgradeDefinition*> Remaining = GetLockedSynergyDiscoveryCandidates();
	const int32 DesiredCount = FMath::Max(0, UpgradeChoiceCount);
	while (CurrentUpgradeChoices.Num() < DesiredCount && Remaining.Num() > 0)
	{
		const int32 PickedIndex = FMath::RandRange(0, Remaining.Num() - 1);
		CurrentUpgradeChoices.Add(Remaining[PickedIndex]);
		Remaining.RemoveAtSwap(PickedIndex);
	}
	bHasSelectedCategory = CurrentUpgradeChoices.Num() > 0;
	BuildOffersFromCurrentChoices(false);
	return bHasSelectedCategory;
}

bool UPlayerUpgradeComponent::SelectSynergyDiscoveryUpgrade(UUpgradeDefinition* Upgrade)
{
	if (!bHasSelectedCategory || !CurrentUpgradeChoices.Contains(Upgrade) || !Upgrade) return false;
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	USynergyMetaProgressionSubsystem* MetaSubsystem = GameInstance ? GameInstance->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
	if (!MetaSubsystem || !MetaSubsystem->UnlockSynergyUpgrade(Upgrade->MetaUnlockId)) return false;

	const bool bGrantedForCurrentRun = AcquireUpgrade(Upgrade);
	UE_LOG(LogTemp, Log, TEXT("Synergy discovered: %s PermanentUnlock=true CurrentRunGrant=%s"),
		*Upgrade->MetaUnlockId.ToString(), bGrantedForCurrentRun ? TEXT("true") : TEXT("false"));
	ClearCurrentOffer();
	return true;
}

float UPlayerUpgradeComponent::GetAccumulatedUpgradeMagnitude(FName UpgradeId) const
{
	return AccumulatedUpgradeMagnitudes.FindRef(UpgradeId);
}

bool UPlayerUpgradeComponent::SelectCategory(EUpgradeCategory Category, int32 UpgradeChoiceCount)
{
	if (!CurrentCategoryChoices.Contains(Category))
	{
		UE_LOG(LogTemp, Warning, TEXT("Upgrade category selection rejected: %s was not offered."), *CategoryToString(Category));
		return false;
	}

	SelectedCategory = Category;
	bHasSelectedCategory = true;
	CurrentUpgradeChoices = RollUpgradeChoices(Category, UpgradeChoiceCount);
	BuildOffersFromCurrentChoices(true);

	UE_LOG(LogTemp, Log, TEXT("Category selected: %s"), *CategoryToString(Category));
	UE_LOG(LogTemp, Log, TEXT("Eligible %s upgrades:"), *CategoryToString(Category));
	for (const UUpgradeDefinition* Upgrade : GetEligibleUpgradesForCategory(Category))
	{
		UE_LOG(LogTemp, Log, TEXT("  %s"), *UpgradeToLogString(Upgrade));
	}

	UE_LOG(LogTemp, Log, TEXT("Offered upgrades:"));
	for (const UUpgradeDefinition* Upgrade : CurrentUpgradeChoices)
	{
		UE_LOG(LogTemp, Log, TEXT("  %s"), *UpgradeToLogString(Upgrade));
	}

	return CurrentUpgradeChoices.Num() > 0;
}

bool UPlayerUpgradeComponent::SelectUpgrade(UUpgradeDefinition* Upgrade)
{
	UUpgradeDefinition* ResolvedUpgrade = Upgrade;
	if (Upgrade && !CurrentUpgradeChoices.Contains(Upgrade))
	{
		ResolvedUpgrade = nullptr;
		for (UUpgradeDefinition* Candidate : CurrentUpgradeChoices)
		{
			if (Candidate && Candidate->UpgradeId == Upgrade->UpgradeId)
			{
				ResolvedUpgrade = Candidate;
				break;
			}
		}
	}
	if (!bHasSelectedCategory || !ResolvedUpgrade || !CurrentUpgradeChoices.Contains(ResolvedUpgrade))
	{
		UE_LOG(LogTemp, Warning, TEXT("Upgrade selection rejected: %s was not offered."), *UpgradeToLogString(Upgrade));
		return false;
	}

	const FUpgradeOffer* Offer = CurrentUpgradeOffers.FindByPredicate([ResolvedUpgrade](const FUpgradeOffer& Candidate)
	{
		return Candidate.UpgradeDefinition == ResolvedUpgrade;
	});
	if (!AcquireUpgradeResolved(ResolvedUpgrade, Offer ? Offer->ResolvedMagnitude : 0.0f, Offer ? Offer->RolledRarity : EUpgradeRarity::Common))
	{
		UE_LOG(LogTemp, Warning, TEXT("Upgrade selection failed to acquire: %s"), *UpgradeToLogString(Upgrade));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Upgrade selected: %s"), *UpgradeToLogString(ResolvedUpgrade));
	UE_LOG(LogTemp, Log, TEXT("New Level: %d"), GetUpgradeLevel(ResolvedUpgrade));
	UE_LOG(LogTemp, Log, TEXT("=== UPGRADE SELECTION END ==="));
	ClearCurrentOffer();
	return true;
}

TArray<EUpgradeCategory> UPlayerUpgradeComponent::GetCurrentCategoryChoices() const
{
	return CurrentCategoryChoices;
}

EUpgradeCategory UPlayerUpgradeComponent::GetSelectedCategory() const
{
	return SelectedCategory;
}

TArray<UUpgradeDefinition*> UPlayerUpgradeComponent::GetCurrentUpgradeChoices() const
{
	TArray<UUpgradeDefinition*> UpgradeChoices;
	const TArray<TObjectPtr<UUpgradeDefinition>>& Source = CurrentPresentationChoices.Num() == CurrentUpgradeChoices.Num()
		? CurrentPresentationChoices : CurrentUpgradeChoices;
	for (UUpgradeDefinition* Upgrade : Source)
	{
		UpgradeChoices.Add(Upgrade);
	}

	return UpgradeChoices;
}

bool UPlayerUpgradeComponent::DebugAcquireUpgrade(UUpgradeDefinition* Upgrade)
{
	const bool bAcquired = AcquireUpgrade(Upgrade);
	if (!bAcquired)
	{
		UE_LOG(LogTemp, Warning, TEXT("DebugAcquireUpgrade failed: Upgrade=%s Valid=%s CurrentLevel=%d MaxLevel=%d MeetsPrerequisites=%s"),
			*UpgradeToLogString(Upgrade),
			IsValidUpgradeDefinition(Upgrade) ? TEXT("true") : TEXT("false"),
			GetUpgradeLevel(Upgrade),
			Upgrade ? Upgrade->MaxLevel : 0,
			MeetsPrerequisites(Upgrade) ? TEXT("true") : TEXT("false"));
	}

	return bAcquired;
}

bool UPlayerUpgradeComponent::DebugForceAcquireUpgrade(UUpgradeDefinition* Upgrade, int32 Level)
{
#if !UE_BUILD_SHIPPING
	if (!IsValidUpgradeDefinition(Upgrade))
	{
		UE_LOG(LogTemp, Warning, TEXT("DebugForceAcquireUpgrade failed: invalid upgrade."));
		return false;
	}

	const int32 NewLevel = FMath::Clamp(Level, 1, FMath::Max(1, Upgrade->MaxLevel));
	UpgradeLevels.FindOrAdd(Upgrade->UpgradeId) = NewLevel;
	AcquiredUpgradeDefinitions.FindOrAdd(Upgrade->UpgradeId) = Upgrade;
	if (Upgrade->bUsesRolledRarity)
	{
		AccumulatedUpgradeMagnitudes.FindOrAdd(Upgrade->UpgradeId) = ResolveMagnitude(Upgrade, EUpgradeRarity::Common) * NewLevel;
	}

	RebuildUpgradeModifiers(Upgrade, NewLevel);
	if (Upgrade->InvestmentOwner == EUpgradeInvestmentOwner::Samurai)
	{
		++SamuraiMasteryPoints;
		UE_LOG(LogTemp, Log, TEXT("[Mastery] Samurai Points=%d Power=%.3f Upgrade=%s Level=%d"),
			SamuraiMasteryPoints, GetSamuraiPowerMultiplier(), *Upgrade->UpgradeId.ToString(), NewLevel);
	}
	else if (Upgrade->InvestmentOwner == EUpgradeInvestmentOwner::Ninja)
	{
		++NinjaMasteryPoints;
		UE_LOG(LogTemp, Log, TEXT("[Mastery] Ninja Points=%d Power=%.3f Upgrade=%s Level=%d"),
			NinjaMasteryPoints, GetNinjaPowerMultiplier(), *Upgrade->UpgradeId.ToString(), NewLevel);
	}
	LogPlayerUpgradeStats();

	OnUpgradeAcquired.Broadcast(Upgrade, NewLevel);
	OnUpgradeLevelChanged.Broadcast(Upgrade, NewLevel);

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

TArray<UUpgradeDefinition*> UPlayerUpgradeComponent::GetEligibleUpgrades(const TArray<UUpgradeDefinition*>& CandidateUpgrades) const
{
	TArray<UUpgradeDefinition*> EligibleUpgrades;
	for (UUpgradeDefinition* Upgrade : CandidateUpgrades)
	{
		if (CanAcquireUpgrade(Upgrade))
		{
			EligibleUpgrades.Add(Upgrade);
		}
	}

	return EligibleUpgrades;
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

UUpgradeDefinition* UPlayerUpgradeComponent::GetAcquiredUpgradeWithSpecialEffect(EUpgradeSpecialEffect SpecialEffect) const
{
	if (SpecialEffect == EUpgradeSpecialEffect::None)
	{
		return nullptr;
	}

	for (const TPair<FName, TObjectPtr<UUpgradeDefinition>>& UpgradePair : AcquiredUpgradeDefinitions)
	{
		UUpgradeDefinition* Upgrade = UpgradePair.Value;
		if (IsValidUpgradeDefinition(Upgrade) && Upgrade->SpecialEffects.Contains(SpecialEffect) && GetUpgradeLevel(Upgrade) > 0)
		{
			return Upgrade;
		}
	}

	return nullptr;
}

bool UPlayerUpgradeComponent::IsValidUpgradeDefinition(const UUpgradeDefinition* Upgrade) const
{
	return Upgrade && !Upgrade->UpgradeId.IsNone() && Upgrade->MaxLevel > 0;
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
		if (Requirement.UpgradeId.IsNone() || GetUpgradeLevelById(Requirement.UpgradeId) < FMath::Max(1, Requirement.MinimumLevel))
		{
			return false;
		}
	}

	return true;
}

float UPlayerUpgradeComponent::GetBaseCategoryWeight(EUpgradeCategory Category) const
{
	return 1.0f;
}

float UPlayerUpgradeComponent::GetCategoryRollWeight(EUpgradeCategory Category) const
{
	const int32 MissCount = FMath::Max(0, CategoryRollsSinceLastOffered.FindRef(Category));
	return FMath::Max(0.0f, GetBaseCategoryWeight(Category) + MissCount * CategoryBadLuckWeightPerMiss);
}

int32 UPlayerUpgradeComponent::GetCurrentPlayerLevel() const
{
	const ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(GetOwner());
	const UExperienceComponent* Experience = SurvivorController ? SurvivorController->GetExperienceComponent() : nullptr;
	return Experience ? Experience->GetCurrentLevel() : 1;
}

bool UPlayerUpgradeComponent::HasAcquiredUpgradeInCategory(EUpgradeCategory Category) const
{
	for (const TPair<FName, TObjectPtr<UUpgradeDefinition>>& UpgradePair : AcquiredUpgradeDefinitions)
	{
		const UUpgradeDefinition* Upgrade = UpgradePair.Value;
		if (IsValidUpgradeDefinition(Upgrade) && Upgrade->Category == Category && GetUpgradeLevel(UpgradePair.Value) > 0)
		{
			return true;
		}
	}

	return false;
}

EUpgradeCategory UPlayerUpgradeComponent::PickWeightedCategory(const TArray<EUpgradeCategory>& Categories, const TArray<float>& Weights) const
{
	float TotalWeight = 0.0f;
	for (const float Weight : Weights)
	{
		TotalWeight += FMath::Max(0.0f, Weight);
	}

	if (Categories.Num() == 0 || TotalWeight <= KINDA_SMALL_NUMBER)
	{
		return Categories.Num() > 0 ? Categories[0] : EUpgradeCategory::Global;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (int32 Index = 0; Index < Categories.Num(); ++Index)
	{
		Roll -= FMath::Max(0.0f, Weights.IsValidIndex(Index) ? Weights[Index] : 0.0f);
		if (Roll <= 0.0f)
		{
			return Categories[Index];
		}
	}

	return Categories.Last();
}

void UPlayerUpgradeComponent::UpdateCategoryBadLuckHistory(const TArray<EUpgradeCategory>& OfferedCategories)
{
	for (const EUpgradeCategory Category : GetEligibleCategories())
	{
		if (OfferedCategories.Contains(Category))
		{
			CategoryRollsSinceLastOffered.FindOrAdd(Category) = 0;
		}
		else
		{
			CategoryRollsSinceLastOffered.FindOrAdd(Category)++;
		}
	}
}

EUpgradeRarity UPlayerUpgradeComponent::RollRarity() const
{
	const ASurvivorPlayerController* Controller = Cast<ASurvivorPlayerController>(GetOwner());
	const float RunTime = Controller ? Controller->GetRunTimeSeconds() : 0.0f;
	FUpgradeRarityTimeBracket Bracket;
	for (const FUpgradeRarityTimeBracket& Candidate : RarityTimeBrackets)
	{
		if (RunTime >= Candidate.MinimumRunTimeSeconds && Candidate.MinimumRunTimeSeconds >= Bracket.MinimumRunTimeSeconds)
		{
			Bracket = Candidate;
		}
	}
	const float Total = FMath::Max(0.0f, Bracket.CommonWeight) + FMath::Max(0.0f, Bracket.RareWeight) + FMath::Max(0.0f, Bracket.EpicWeight);
	float Roll = FMath::FRandRange(0.0f, Total);
	if ((Roll -= FMath::Max(0.0f, Bracket.CommonWeight)) <= 0.0f) return EUpgradeRarity::Common;
	if ((Roll -= FMath::Max(0.0f, Bracket.RareWeight)) <= 0.0f) return EUpgradeRarity::Rare;
	return EUpgradeRarity::Epic;
}

float UPlayerUpgradeComponent::ResolveMagnitude(const UUpgradeDefinition* Upgrade, EUpgradeRarity Rarity) const
{
	if (!Upgrade || !Upgrade->bUsesRolledRarity) return 0.0f;
	for (const FUpgradeRarityMagnitude& Entry : Upgrade->RarityMagnitudes)
	{
		if (Entry.Rarity == Rarity) return Entry.Magnitude;
	}
	return Upgrade->StatModifiers.Num() > 0 ? Upgrade->StatModifiers[0].ValuePerLevel : 0.0f;
}

FText UPlayerUpgradeComponent::ResolveOfferDescription(const UUpgradeDefinition* Upgrade, float Magnitude) const
{
	if (Upgrade && Upgrade->bUsesRolledRarity)
	{
		for (const FUpgradeRarityMagnitude& Entry : Upgrade->RarityMagnitudes)
		{
			if (FMath::IsNearlyEqual(Entry.Magnitude, Magnitude) && !Entry.DescriptionOverride.IsEmpty()) return Entry.DescriptionOverride;
		}
	}
	if (!Upgrade || !Upgrade->bUsesRolledRarity || Upgrade->RolledDescriptionFormat.IsEmpty())
	{
		return Upgrade ? Upgrade->Description : FText::GetEmpty();
	}
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Magnitude"), FText::AsNumber(Magnitude));
	Arguments.Add(TEXT("Percent"), FText::AsNumber(FMath::RoundToInt(Magnitude * 100.0f)));
	return FText::Format(Upgrade->RolledDescriptionFormat, Arguments);
}

FUpgradeOffer UPlayerUpgradeComponent::MakeUpgradeOffer(UUpgradeDefinition* Upgrade) const
{
	FUpgradeOffer Offer;
	Offer.UpgradeDefinition = Upgrade;
	Offer.bDisplaysRarity = IsNormalScalableUpgrade(Upgrade);
	Offer.RolledRarity = Offer.bDisplaysRarity ? RollRarity() : EUpgradeRarity::Common;
	Offer.ResolvedMagnitude = Offer.bDisplaysRarity ? ResolveMagnitude(Upgrade, Offer.RolledRarity) : 0.0f;
	Offer.ResolvedDescription = ResolveOfferDescription(Upgrade, Offer.ResolvedMagnitude);
	return Offer;
}

bool UPlayerUpgradeComponent::IsNormalScalableUpgrade(const UUpgradeDefinition* Upgrade) const
{
	return Upgrade && Upgrade->bUsesRolledRarity
		&& (Upgrade->Category == EUpgradeCategory::Samurai || Upgrade->Category == EUpgradeCategory::Ninja || Upgrade->Category == EUpgradeCategory::Global);
}

void UPlayerUpgradeComponent::BuildOffersFromCurrentChoices(bool bApplyNormalLevelGuarantee)
{
	CurrentUpgradeOffers.Reset();
	for (UUpgradeDefinition* Upgrade : CurrentUpgradeChoices)
	{
		if (bApplyNormalLevelGuarantee)
		{
			CurrentUpgradeOffers.Add(MakeUpgradeOffer(Upgrade));
		}
		else
		{
			FUpgradeOffer FixedOffer;
			FixedOffer.UpgradeDefinition = Upgrade;
			FixedOffer.ResolvedDescription = Upgrade ? Upgrade->Description : FText::GetEmpty();
			CurrentUpgradeOffers.Add(FixedOffer);
		}
	}
	if (bApplyNormalLevelGuarantee) ApplyMilestoneGuarantee();
	CurrentPresentationChoices.Reset();
	for (const FUpgradeOffer& Offer : CurrentUpgradeOffers)
	{
		UUpgradeDefinition* Presentation = Offer.UpgradeDefinition;
		if (Offer.bDisplaysRarity && Offer.UpgradeDefinition)
		{
			Presentation = DuplicateObject<UUpgradeDefinition>(Offer.UpgradeDefinition, this);
			Presentation->Rarity = Offer.RolledRarity;
			Presentation->Description = Offer.ResolvedDescription;
		}
		CurrentPresentationChoices.Add(Presentation);
	}
	for (const FUpgradeOffer& Offer : CurrentUpgradeOffers)
	{
		UE_LOG(LogTemp, Log, TEXT("[UpgradeRarity] OFFER %s Rarity=%s Magnitude=%.3f DisplaysRarity=%s"),
			Offer.UpgradeDefinition ? *Offer.UpgradeDefinition->UpgradeId.ToString() : TEXT("None"),
			*GetRarityDisplayName(Offer.RolledRarity).ToString(), Offer.ResolvedMagnitude,
			Offer.bDisplaysRarity ? TEXT("true") : TEXT("false"));
	}
}

void UPlayerUpgradeComponent::ApplyMilestoneGuarantee()
{
	const int32 Level = GetCurrentPlayerLevel();
	const bool bEpicRequired = Level >= 10 && Level % 5 == 0;
	const bool bRareRequired = Level == 5;
	if (!bEpicRequired && !bRareRequired) return;

	auto Satisfies = [bEpicRequired](const FUpgradeOffer& Offer)
	{
		return Offer.bDisplaysRarity && (bEpicRequired ? Offer.RolledRarity == EUpgradeRarity::Epic : Offer.RolledRarity != EUpgradeRarity::Common);
	};
	if (CurrentUpgradeOffers.ContainsByPredicate(Satisfies)) return;

	int32 PromoteIndex = CurrentUpgradeOffers.IndexOfByPredicate([](const FUpgradeOffer& Offer) { return Offer.bDisplaysRarity; });
	if (PromoteIndex == INDEX_NONE)
	{
		TArray<UUpgradeDefinition*> Candidates;
		for (UUpgradeDefinition* Upgrade : UpgradePool)
		{
			if (IsNormalScalableUpgrade(Upgrade) && CanAcquireUpgrade(Upgrade) && !CurrentUpgradeChoices.Contains(Upgrade)) Candidates.Add(Upgrade);
		}
		if (Candidates.Num() == 0 || CurrentUpgradeOffers.Num() == 0) return;
		const int32 Pick = FMath::RandRange(0, Candidates.Num() - 1);
		PromoteIndex = FMath::RandRange(0, CurrentUpgradeOffers.Num() - 1);
		CurrentUpgradeChoices[PromoteIndex] = Candidates[Pick];
		CurrentUpgradeOffers[PromoteIndex] = MakeUpgradeOffer(Candidates[Pick]);
	}

	FUpgradeOffer& Offer = CurrentUpgradeOffers[PromoteIndex];
	Offer.RolledRarity = bEpicRequired ? EUpgradeRarity::Epic : EUpgradeRarity::Rare;
	Offer.ResolvedMagnitude = ResolveMagnitude(Offer.UpgradeDefinition, Offer.RolledRarity);
	Offer.ResolvedDescription = ResolveOfferDescription(Offer.UpgradeDefinition, Offer.ResolvedMagnitude);
	UE_LOG(LogTemp, Log, TEXT("[UpgradeRarity] MILESTONE Level=%d Promoted=%s Rarity=%s"), Level,
		*Offer.UpgradeDefinition->UpgradeId.ToString(), *GetRarityDisplayName(Offer.RolledRarity).ToString());
}

FText UPlayerUpgradeComponent::GetRarityDisplayName(EUpgradeRarity Rarity) const
{
	switch (Rarity)
	{
	case EUpgradeRarity::Rare: return FText::FromString(TEXT("RARE"));
	case EUpgradeRarity::Epic: return FText::FromString(TEXT("EPIC"));
	default: return FText::FromString(TEXT("COMMON"));
	}
}

void UPlayerUpgradeComponent::ClearCurrentOffer()
{
	CurrentCategoryChoices.Reset();
	CurrentUpgradeChoices.Reset();
	CurrentUpgradeOffers.Reset();
	CurrentPresentationChoices.Reset();
	SelectedCategory = EUpgradeCategory::Global;
	bHasSelectedCategory = false;
}

FString UPlayerUpgradeComponent::CategoryToString(EUpgradeCategory Category) const
{
	switch (Category)
	{
	case EUpgradeCategory::Samurai:
		return TEXT("Samurai");
	case EUpgradeCategory::Ninja:
		return TEXT("Ninja");
	case EUpgradeCategory::Global:
		return TEXT("Global");
	case EUpgradeCategory::Synergy:
		return TEXT("Synergy");
	case EUpgradeCategory::Cursed:
		return TEXT("Blood Pact");
	case EUpgradeCategory::NinjaTrial:
		return TEXT("Ninja Technique");
	case EUpgradeCategory::SamuraiTrial:
		return TEXT("Samurai Technique");
	default:
		return TEXT("Unknown");
	}
}

FString UPlayerUpgradeComponent::UpgradeToLogString(const UUpgradeDefinition* Upgrade) const
{
	if (!Upgrade)
	{
		return TEXT("None");
	}

	const FString DisplayName = Upgrade->DisplayName.IsEmpty() ? Upgrade->UpgradeId.ToString() : Upgrade->DisplayName.ToString();
	return FString::Printf(TEXT("%s (%s) Level=%d/%d"),
		*DisplayName,
		*Upgrade->UpgradeId.ToString(),
		GetUpgradeLevel(const_cast<UUpgradeDefinition*>(Upgrade)),
		Upgrade->MaxLevel);
}

void UPlayerUpgradeComponent::RebuildUpgradeModifiers(UUpgradeDefinition* Upgrade, int32 NewLevel)
{
	if (!IsValidUpgradeDefinition(Upgrade))
	{
		return;
	}

	ClearUpgradeModifiers(Upgrade);

	ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(GetOwner());
	UCharacterManagerComponent* CharacterManager = SurvivorController ? SurvivorController->GetCharacterManager() : nullptr;
	USharedPlayerStatsComponent* SharedStats = SurvivorController ? SurvivorController->GetSharedPlayerStats() : nullptr;
	const FName SourceId = MakeUpgradeModifierSourceId(Upgrade);

	UE_LOG(LogTemp, Log, TEXT("RebuildUpgradeModifiers: Upgrade=%s Level=%d Modifiers=%d Controller=%s CharacterManager=%s SharedStats=%s Samurai=%s Ninja=%s"),
		*Upgrade->UpgradeId.ToString(),
		NewLevel,
		Upgrade->StatModifiers.Num(),
		*GetNameSafe(SurvivorController),
		*GetNameSafe(CharacterManager),
		*GetNameSafe(SharedStats),
		*GetNameSafe(CharacterManager ? CharacterManager->GetSamurai() : nullptr),
		*GetNameSafe(CharacterManager ? CharacterManager->GetNinja() : nullptr));

	for (int32 Index = 0; Index < Upgrade->StatModifiers.Num(); ++Index)
	{
		const FUpgradeStatModifierDefinition& ModifierDefinition = Upgrade->StatModifiers[Index];
		const float* StoredMagnitude = AccumulatedUpgradeMagnitudes.Find(Upgrade->UpgradeId);
		const float ModifierValue = Upgrade->bUsesRolledRarity && StoredMagnitude
			? *StoredMagnitude
			: ModifierDefinition.ValuePerLevel * NewLevel;

		if (ModifierDefinition.Target == EUpgradeStatTarget::SharedPlayer)
		{
			if (!SharedStats)
			{
				UE_LOG(LogTemp, Warning, TEXT("Upgrade modifier skipped: Upgrade=%s Target=SharedPlayer SharedStats missing"),
					*Upgrade->UpgradeId.ToString());
				continue;
			}

			FSharedPlayerStatModifier Modifier;
			Modifier.ModifierId = MakeUpgradeModifierId(Upgrade, Index);
			Modifier.SourceId = SourceId;
			Modifier.Stat = ModifierDefinition.SharedPlayerStat;
			Modifier.Operation = ModifierDefinition.Operation;
			Modifier.Value = ModifierValue;
			SharedStats->AddModifier(Modifier);
			UE_LOG(LogTemp, Log, TEXT("Upgrade modifier applied: Upgrade=%s Target=SharedPlayer Stat=%d Operation=%d Value=%.3f"),
				*Upgrade->UpgradeId.ToString(),
				static_cast<int32>(Modifier.Stat),
				static_cast<int32>(Modifier.Operation),
				Modifier.Value);
			continue;
		}

		ACharacterBase* TargetCharacter = nullptr;
		if (CharacterManager)
		{
			TargetCharacter = ModifierDefinition.Target == EUpgradeStatTarget::Samurai
				? Cast<ACharacterBase>(CharacterManager->GetSamurai())
				: Cast<ACharacterBase>(CharacterManager->GetNinja());
		}

		UCharacterStatsComponent* CharacterStats = TargetCharacter ? TargetCharacter->GetCharacterStats() : nullptr;
		if (!CharacterStats)
		{
			UE_LOG(LogTemp, Warning, TEXT("Upgrade modifier skipped: Upgrade=%s Target=%d Character=%s CharacterStats missing"),
				*Upgrade->UpgradeId.ToString(),
				static_cast<int32>(ModifierDefinition.Target),
				*GetNameSafe(TargetCharacter));
			continue;
		}

		FCharacterStatModifier Modifier;
		Modifier.ModifierId = MakeUpgradeModifierId(Upgrade, Index);
		Modifier.SourceId = SourceId;
		Modifier.Stat = ModifierDefinition.CharacterStat;
		Modifier.Operation = ModifierDefinition.Operation;
		Modifier.Value = ModifierValue;
		CharacterStats->AddModifier(Modifier);
		UE_LOG(LogTemp, Log, TEXT("Upgrade modifier applied: Upgrade=%s Target=%d Character=%s Stat=%d Operation=%d Value=%.3f FinalStat=%.3f ModifierCount=%d"),
			*Upgrade->UpgradeId.ToString(),
			static_cast<int32>(ModifierDefinition.Target),
			*GetNameSafe(TargetCharacter),
			static_cast<int32>(Modifier.Stat),
			static_cast<int32>(Modifier.Operation),
			Modifier.Value,
			CharacterStats->GetFinalStat(Modifier.Stat),
			CharacterStats->GetModifierCount());
	}
}

void UPlayerUpgradeComponent::ClearUpgradeModifiers(UUpgradeDefinition* Upgrade)
{
	if (!IsValidUpgradeDefinition(Upgrade))
	{
		return;
	}

	ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(GetOwner());
	UCharacterManagerComponent* CharacterManager = SurvivorController ? SurvivorController->GetCharacterManager() : nullptr;
	const FName SourceId = MakeUpgradeModifierSourceId(Upgrade);

	if (CharacterManager)
	{
		if (ASamuraiCharacter* Samurai = CharacterManager->GetSamurai())
		{
			if (UCharacterStatsComponent* CharacterStats = Samurai->GetCharacterStats())
			{
				CharacterStats->ClearModifiersFromSource(SourceId);
			}
		}

		if (ANinjaCharacter* Ninja = CharacterManager->GetNinja())
		{
			if (UCharacterStatsComponent* CharacterStats = Ninja->GetCharacterStats())
			{
				CharacterStats->ClearModifiersFromSource(SourceId);
			}
		}
	}

	if (USharedPlayerStatsComponent* SharedStats = SurvivorController ? SurvivorController->GetSharedPlayerStats() : nullptr)
	{
		SharedStats->ClearModifiersFromSource(SourceId);
	}
}

FName UPlayerUpgradeComponent::MakeUpgradeModifierSourceId(const UUpgradeDefinition* Upgrade) const
{
	return IsValidUpgradeDefinition(Upgrade) ? Upgrade->UpgradeId : NAME_None;
}

FName UPlayerUpgradeComponent::MakeUpgradeModifierId(const UUpgradeDefinition* Upgrade, int32 ModifierIndex) const
{
	if (!IsValidUpgradeDefinition(Upgrade))
	{
		return NAME_None;
	}

	return FName(*FString::Printf(TEXT("%s.Modifier.%d"), *Upgrade->UpgradeId.ToString(), ModifierIndex));
}
