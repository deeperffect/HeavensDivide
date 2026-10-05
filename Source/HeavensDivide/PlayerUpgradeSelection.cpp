#include "PlayerUpgradeComponent.h"
#include "CombatAudio.h"
#include "ExperienceComponent.h"
#include "PlayerUpgradeRules.h"
#include "SurvivorPlayerController.h"
#include "SynergyMetaProgressionSubsystem.h"

using namespace PlayerUpgradeRules;

namespace
{
// Run-specific copy belongs to transient offer cards, never the saved definition.
bool TradeoffCopy(const UPlayerUpgradeComponent* U, const UUpgradeDefinition* Card,
                  float Magnitude, FText& Name, FText& Description)
{
 if (!Card) return false;
 const FName Id = Card->UpgradeId;
 auto Copy = [&](const TCHAR* Title, const FString& Body) {
  Name = FText::FromString(Title); Description = FText::FromString(Body); return true;
 };
 auto Percent = [](float Value) { return FText::AsNumber(Value * 100.f).ToString(); };
 if (U->HasUpgradeId(TEXT("BarrageProcession")))
 {
  if (Id == TEXT("ForkingProjectiles") || Id == TEXT("BarrageSplitChance"))
  {
   const bool bUnlock = Id == TEXT("ForkingProjectiles");
   const float Chance = Card->GetBalanceValue(bUnlock ? TEXT("Chance") : TEXT("PerRank"), bUnlock ? .15f : .1f);
   return Copy(bUnlock ? TEXT("Rebounding Blades") : TEXT("Ricochet Mastery"),
    FString::Printf(TEXT("Reduce damage lost on each ricochet by %s percentage points%s."),
     *Percent(Chance * .2f), bUnlock ? TEXT("") : TEXT(" per rank")));
  }
 }
 if (U->HasUpgradeId(TEXT("BloodDetonation")) && Id == TEXT("BloodTransferArea"))
  return Copy(TEXT("Crimson Reach"), TEXT("Increase Blood Detonation radius by 10% per rank."));
 if (U->HasUpgradeId(TEXT("CrescentEruptionPact")))
 {
  if (Id == TEXT("CrescentFieldPower"))
   return Copy(TEXT("Violent Wake"), FString::Printf(TEXT("Increase Sudden Eruption damage by %s%% per rank."), *Percent(Card->GetBalanceValue(TEXT("PerRank"), .15f))));
  if (Id == TEXT("CrescentFieldChance"))
   return Copy(TEXT("Restless Eruption"), TEXT("Gain 5 percentage points of chance to leave an erupting trail. Split waves share the trigger."));
 }
 if (U->HasUpgradeId(TEXT("BarrageBloom")) && Id == TEXT("BarragePoolRadius"))
  return Copy(TEXT("Spreading Bloom"), FString::Printf(TEXT("Increase Venom Bloom puddle radius by %s%% per rank."), *Percent(Card->GetBalanceValue(TEXT("PerRank"), .2f))));
 if (U->HasUpgradeId(TEXT("ShurikenHunger")) && Id == TEXT("ShurikenSize"))
  return Copy(TEXT("Ravenous Growth"), FString::Printf(TEXT("Increase blade size gained per kill by %s%% per rank. Growth remains limited by the blade's growth cap."), *Percent(Magnitude)));
 return false;
}
}

bool UPlayerUpgradeComponent::IsCategoryUnlocked(EUpgradeCategory Category) const
{
	if (Category != EUpgradeCategory::Synergy)
	{
		return true;
	}

	return GetCurrentPlayerLevel() >= SynergyUnlockLevel && HasAcquiredUpgradeInCategory(EUpgradeCategory::Samurai) &&
	       HasAcquiredUpgradeInCategory(EUpgradeCategory::Ninja);
}

TArray<EUpgradeCategory> UPlayerUpgradeComponent::GetEligibleCategories() const
{
	TArray<EUpgradeCategory> EligibleCategories;
	constexpr EUpgradeCategory Categories[] = {EUpgradeCategory::Samurai, EUpgradeCategory::Ninja,
	                                           EUpgradeCategory::Global, EUpgradeCategory::Synergy};

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
		if (Upgrade && Upgrade->Category == Category && !IsTrialBuildStarter(Upgrade) && !IsBloodShrineUpgrade(Upgrade))
		{
			CandidateUpgrades.Add(Upgrade);
		}
	}

	return GetEligibleUpgrades(CandidateUpgrades);
}

TArray<UUpgradeDefinition*> UPlayerUpgradeComponent::RollUpgradeChoices(EUpgradeCategory Category,
                                                                        int32 ChoiceCount) const
{
	TArray<UUpgradeDefinition*> RemainingUpgrades = GetEligibleUpgradesForCategory(Category);
	TArray<UUpgradeDefinition*> OfferedUpgrades;
	const int32 DesiredChoiceCount = FMath::Max(0, ChoiceCount);
	// Character offers give a choice between discovering an ability and investing
	// in an unlocked build and choosing a branch or evolution.
	if (Category == EUpgradeCategory::Samurai || Category == EUpgradeCategory::Ninja)
	{
		for (EUpgradeRole Role : {EUpgradeRole::Starter, EUpgradeRole::Support, EUpgradeRole::Mechanic})
		{
			if (OfferedUpgrades.Num() >= DesiredChoiceCount)
				break;
			TArray<UUpgradeDefinition*> Candidates;
			for (UUpgradeDefinition* Upgrade : RemainingUpgrades)
				if (Upgrade->Role == Role ||
				    (Role == EUpgradeRole::Mechanic && Upgrade->Role == EUpgradeRole::Evolution))
					Candidates.Add(Upgrade);
			if (Candidates.IsEmpty())
				continue;
			UUpgradeDefinition* Pick = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
			OfferedUpgrades.Add(Pick);
			RemainingUpgrades.Remove(Pick);
		}
	}

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
		UE_LOG(LogTemp, Log, TEXT("  %s Weight=%.2f Misses=%d"), *CategoryToString(Category),
		       GetCategoryRollWeight(Category), CategoryRollsSinceLastOffered.FindRef(Category));
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
		if (Upgrade && !IsTrialBuildStarter(Upgrade) && !IsBloodShrineUpgrade(Upgrade) &&
		    IsCategoryUnlocked(Upgrade->Category) && CanAcquireUpgrade(Upgrade))
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

bool UPlayerUpgradeComponent::BeginBloodShrineSelection(int32 ChoiceCount)
{
	ClearCurrentOffer();
	SelectedCategory = EUpgradeCategory::Cursed;
	if (HasUpgradeId(TEXT("BattleStance")) || HasUpgradeId(TEXT("Iaijutsu")) || HasUpgradeId(TEXT("BladeWave")) ||
	    HasUpgradeId(TEXT("ReturningFang")) || HasUpgradeId(TEXT("BarrageStance")) || HasUpgradeId(TEXT("GreatShuriken")))
	{
		for (UUpgradeDefinition* Upgrade : UpgradePool)
			if (IsBloodShrineUpgrade(Upgrade) && CanAcquireUpgrade(Upgrade))
				CurrentUpgradeChoices.Add(Upgrade);
		for (int32 Index = CurrentUpgradeChoices.Num() - 1; Index > 0; --Index)
			CurrentUpgradeChoices.Swap(Index, FMath::RandRange(0, Index));
		if (CurrentUpgradeChoices.Num() > FMath::Max(0, ChoiceCount))
			CurrentUpgradeChoices.SetNum(FMath::Max(0, ChoiceCount));
	}
	if (CurrentUpgradeChoices.IsEmpty())
		CurrentUpgradeChoices = RollUpgradeChoices(EUpgradeCategory::Cursed, ChoiceCount);
	bHasSelectedCategory = !CurrentUpgradeChoices.IsEmpty();
	BuildOffersFromCurrentChoices(false);
	return bHasSelectedCategory;
}

bool UPlayerUpgradeComponent::BeginDirectCategoryUpgradeSelection(EUpgradeCategory Category, int32 UpgradeChoiceCount)
{
	const bool bCharacterTrial = Category == EUpgradeCategory::SamuraiTrial || Category == EUpgradeCategory::NinjaTrial;
	if (Category == EUpgradeCategory::SamuraiTrial)
		Category = EUpgradeCategory::Samurai;
	if (Category == EUpgradeCategory::NinjaTrial)
		Category = EUpgradeCategory::Ninja;
	ClearCurrentOffer();
	SelectedCategory = Category;
	if (bCharacterTrial)
	{
		TArray<UUpgradeDefinition*> Starters;
		for (UUpgradeDefinition* Upgrade : UpgradePool)
			if (IsTrialBuildStarter(Upgrade) && Upgrade->Category == Category && CanAcquireUpgrade(Upgrade))
				Starters.AddUnique(Upgrade);
		while (!Starters.IsEmpty() && CurrentUpgradeChoices.Num() < FMath::Max(0, UpgradeChoiceCount))
		{
			const int32 Index = FMath::RandRange(0, Starters.Num() - 1);
			CurrentUpgradeChoices.Add(Starters[Index]);
			Starters.RemoveAtSwap(Index);
		}
	}
	// Once a stance is owned, exclusivity removes the other starters. Repeat trials
	// still reward eligible support/branch cards without replacing the chosen route.
	if (CurrentUpgradeChoices.IsEmpty())
		CurrentUpgradeChoices = RollUpgradeChoices(Category, UpgradeChoiceCount);
	bHasSelectedCategory = CurrentUpgradeChoices.Num() > 0;
	BuildOffersFromCurrentChoices(false);
	return bHasSelectedCategory;
}

TArray<UUpgradeDefinition*> UPlayerUpgradeComponent::GetLockedSynergyDiscoveryCandidates() const
{
	TArray<UUpgradeDefinition*> Candidates;
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const USynergyMetaProgressionSubsystem* MetaSubsystem =
	    GameInstance ? GameInstance->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
	if (!MetaSubsystem)
		return Candidates;

	for (UUpgradeDefinition* Upgrade : UpgradePool)
	{
		if (Upgrade && Upgrade->Category == EUpgradeCategory::Synergy && Upgrade->bRequiresMetaUnlock &&
		    !Upgrade->MetaUnlockId.IsNone() && !MetaSubsystem->IsSynergyUpgradeUnlocked(Upgrade->MetaUnlockId))
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
	if (!bHasSelectedCategory || !CurrentUpgradeChoices.Contains(Upgrade) || !Upgrade)
		return false;
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	USynergyMetaProgressionSubsystem* MetaSubsystem =
	    GameInstance ? GameInstance->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
	if (!MetaSubsystem || !MetaSubsystem->UnlockSynergyUpgrade(Upgrade->MetaUnlockId))
		return false;

	const bool bGrantedForCurrentRun = AcquireUpgrade(Upgrade);
	UE_LOG(LogTemp, Log, TEXT("Synergy discovered: %s PermanentUnlock=true CurrentRunGrant=%s"),
	       *Upgrade->MetaUnlockId.ToString(), bGrantedForCurrentRun ? TEXT("true") : TEXT("false"));
	ClearCurrentOffer();
	return true;
}

bool UPlayerUpgradeComponent::SelectCategory(EUpgradeCategory Category, int32 UpgradeChoiceCount)
{
	if (!CurrentCategoryChoices.Contains(Category))
	{
		UE_LOG(LogTemp, Warning, TEXT("Upgrade category selection rejected: %s was not offered."),
		       *CategoryToString(Category));
		return false;
	}

	SelectedCategory = Category;
	bHasSelectedCategory = true;
	CurrentUpgradeChoices = RollUpgradeChoices(Category, UpgradeChoiceCount);
	bDraftToolOffer = true;
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

	const FUpgradeOffer* Offer = CurrentUpgradeOffers.FindByPredicate(
	    [ResolvedUpgrade](const FUpgradeOffer& Candidate) { return Candidate.UpgradeDefinition == ResolvedUpgrade; });
	if (!AcquireUpgradeResolved(ResolvedUpgrade, Offer ? Offer->ResolvedMagnitude : 0.0f,
	                            Offer ? Offer->RolledRarity : EUpgradeRarity::Common))
	{
		UE_LOG(LogTemp, Warning, TEXT("Upgrade selection failed to acquire: %s"), *UpgradeToLogString(Upgrade));
		return false;
	}

	UCombatAudioLibrary::PlayEvent(this, TEXT("UpgradeSelect"), FVector::ZeroVector, true);
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
	const TArray<TObjectPtr<UUpgradeDefinition>>& Source =
	    CurrentPresentationChoices.Num() == CurrentUpgradeChoices.Num() ? CurrentPresentationChoices
	                                                                    : CurrentUpgradeChoices;
	for (UUpgradeDefinition* Upgrade : Source)
	{
		UpgradeChoices.Add(Upgrade);
	}

	return UpgradeChoices;
}

TArray<UUpgradeDefinition*> UPlayerUpgradeComponent::GetEligibleUpgrades(
    const TArray<UUpgradeDefinition*>& CandidateUpgrades) const
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

float UPlayerUpgradeComponent::GetCategoryRollWeight(EUpgradeCategory Category) const
{
	const int32 MissCount = FMath::Max(0, CategoryRollsSinceLastOffered.FindRef(Category));
	return FMath::Max(0.0f, 1.0f + MissCount * CategoryBadLuckWeightPerMiss);
}

int32 UPlayerUpgradeComponent::GetCurrentPlayerLevel() const
{
	const ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(GetOwner());
	const UExperienceComponent* Experience =
	    SurvivorController ? SurvivorController->GetExperienceComponent() : nullptr;
	return Experience ? Experience->GetCurrentLevel() : 1;
}

bool UPlayerUpgradeComponent::HasAcquiredUpgradeInCategory(EUpgradeCategory Category) const
{
	for (const TPair<FName, TObjectPtr<UUpgradeDefinition>>& UpgradePair : AcquiredUpgradeDefinitions)
	{
		const UUpgradeDefinition* Upgrade = UpgradePair.Value;
		if (IsValidUpgradeDefinition(Upgrade) && Upgrade->Category == Category &&
		    GetUpgradeLevel(UpgradePair.Value) > 0)
		{
			return true;
		}
	}

	return false;
}

EUpgradeCategory UPlayerUpgradeComponent::PickWeightedCategory(const TArray<EUpgradeCategory>& Categories,
                                                               const TArray<float>& Weights) const
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
		if (RunTime >= Candidate.MinimumRunTimeSeconds &&
		    Candidate.MinimumRunTimeSeconds >= Bracket.MinimumRunTimeSeconds)
		{
			Bracket = Candidate;
		}
	}
	const float Total = FMath::Max(0.0f, Bracket.CommonWeight) + FMath::Max(0.0f, Bracket.RareWeight) +
	                    FMath::Max(0.0f, Bracket.EpicWeight);
	float Roll = FMath::FRandRange(0.0f, Total);
	if ((Roll -= FMath::Max(0.0f, Bracket.CommonWeight)) <= 0.0f)
		return EUpgradeRarity::Common;
	if ((Roll -= FMath::Max(0.0f, Bracket.RareWeight)) <= 0.0f)
		return EUpgradeRarity::Rare;
	return EUpgradeRarity::Epic;
}

float UPlayerUpgradeComponent::ResolveMagnitude(const UUpgradeDefinition* Upgrade, EUpgradeRarity Rarity) const
{
	if (!Upgrade || !Upgrade->bUsesRolledRarity)
		return 0.0f;
	for (const FUpgradeRarityMagnitude& Entry : Upgrade->RarityMagnitudes)
	{
		if (Entry.Rarity == Rarity)
			return Entry.Magnitude;
	}
	return Upgrade->StatModifiers.Num() > 0 ? Upgrade->StatModifiers[0].ValuePerLevel : 0.0f;
}

FText UPlayerUpgradeComponent::ResolveOfferDescription(const UUpgradeDefinition* Upgrade, float Magnitude) const
{
	FText Name, Description;
	if (TradeoffCopy(this, Upgrade, Magnitude, Name, Description)) return Description;
	if (Upgrade && Upgrade->bUsesRolledRarity)
	{
		for (const FUpgradeRarityMagnitude& Entry : Upgrade->RarityMagnitudes)
		{
			if (FMath::IsNearlyEqual(Entry.Magnitude, Magnitude) && !Entry.DescriptionOverride.IsEmpty())
				return Entry.DescriptionOverride;
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
	const bool bUsesNormalRarityRoll = IsNormalScalableUpgrade(Upgrade);
	const bool bIsFixedLegendary =
	    Upgrade && !Upgrade->bUsesRolledRarity && Upgrade->Rarity == EUpgradeRarity::Legendary;
	const bool bStanceRare = IsStanceRareUpgrade(Upgrade);
	Offer.bDisplaysRarity = bUsesNormalRarityRoll || bIsFixedLegendary || bStanceRare;
	Offer.RolledRarity = bUsesNormalRarityRoll ? RollRarity()
	                                           : (bIsFixedLegendary ? EUpgradeRarity::Legendary
	                                              : bStanceRare     ? EUpgradeRarity::Rare
	                                                                : EUpgradeRarity::Common);
	Offer.ResolvedMagnitude = bUsesNormalRarityRoll ? ResolveMagnitude(Upgrade, Offer.RolledRarity) : 0.0f;
	Offer.ResolvedDescription = ResolveOfferDescription(Upgrade, Offer.ResolvedMagnitude);
	return Offer;
}

bool UPlayerUpgradeComponent::IsNormalScalableUpgrade(const UUpgradeDefinition* Upgrade) const
{
	return Upgrade && Upgrade->bUsesRolledRarity &&
	       (Upgrade->Category == EUpgradeCategory::Samurai || Upgrade->Category == EUpgradeCategory::Ninja ||
	        Upgrade->Category == EUpgradeCategory::Global);
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
			FixedOffer.bDisplaysRarity = IsStanceRareUpgrade(Upgrade);
			FixedOffer.RolledRarity = FixedOffer.bDisplaysRarity ? EUpgradeRarity::Rare : EUpgradeRarity::Common;
			FixedOffer.ResolvedMagnitude = Upgrade && Upgrade->bUsesRolledRarity ? ResolveMagnitude(Upgrade, FixedOffer.RolledRarity) : 0.f;
			FixedOffer.ResolvedDescription = ResolveOfferDescription(Upgrade, FixedOffer.ResolvedMagnitude);
			CurrentUpgradeOffers.Add(FixedOffer);
		}
	}
	if (bApplyNormalLevelGuarantee)
		ApplyMilestoneGuarantee();
	RebuildOfferPresentation();
}

void UPlayerUpgradeComponent::RebuildOfferPresentation()
{
	CurrentPresentationChoices.Reset();
	for (const FUpgradeOffer& Offer : CurrentUpgradeOffers)
	{
		UUpgradeDefinition* Presentation = Offer.UpgradeDefinition;
		FText ContextName, ContextDescription;
		const bool bContextCopy = TradeoffCopy(this, Offer.UpgradeDefinition, Offer.ResolvedMagnitude, ContextName, ContextDescription);
		if ((Offer.bDisplaysRarity || bContextCopy) && Offer.UpgradeDefinition)
		{
			Presentation = DuplicateObject<UUpgradeDefinition>(Offer.UpgradeDefinition, this);
			Presentation->Rarity = Offer.RolledRarity;
			Presentation->Description = Offer.ResolvedDescription;
			if (bContextCopy) Presentation->DisplayName = ContextName;
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
	if (!bEpicRequired && !bRareRequired)
		return;

	auto Satisfies = [this, bEpicRequired](const FUpgradeOffer& Offer) {
		return IsNormalScalableUpgrade(Offer.UpgradeDefinition) &&
		       (bEpicRequired ? Offer.RolledRarity == EUpgradeRarity::Epic
		                      : Offer.RolledRarity != EUpgradeRarity::Common);
	};
	if (CurrentUpgradeOffers.ContainsByPredicate(Satisfies))
		return;

	int32 PromoteIndex = CurrentUpgradeOffers.IndexOfByPredicate(
	    [this](const FUpgradeOffer& Offer) { return IsNormalScalableUpgrade(Offer.UpgradeDefinition); });
	if (PromoteIndex == INDEX_NONE)
	{
		TArray<UUpgradeDefinition*> Candidates;
		for (UUpgradeDefinition* Upgrade : UpgradePool)
		{
			if (Upgrade && Upgrade->Category == SelectedCategory && IsNormalScalableUpgrade(Upgrade) &&
			    CanAcquireUpgrade(Upgrade) && !CurrentUpgradeChoices.Contains(Upgrade))
			{
				Candidates.Add(Upgrade);
			}
		}
		if (Candidates.Num() == 0 || CurrentUpgradeOffers.Num() == 0)
			return;
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
	case EUpgradeRarity::Rare:
		return FText::FromString(TEXT("RARE"));
	case EUpgradeRarity::Epic:
		return FText::FromString(TEXT("EPIC"));
	case EUpgradeRarity::Legendary:
		return FText::FromString(TEXT("LEGENDARY"));
	default:
		return FText::FromString(TEXT("COMMON"));
	}
}

void UPlayerUpgradeComponent::ClearCurrentOffer()
{
	bDraftToolOffer = false;
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
