#include "PlayerUpgradeComponent.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "PlayerUpgradeRules.h"

TArray<UUpgradeDefinition*> UPlayerUpgradeComponent::GetNormalLevelUpCandidates() const
{
 TArray<UUpgradeDefinition*> Result;
 for (auto Category : {EUpgradeCategory::Samurai, EUpgradeCategory::Ninja, EUpgradeCategory::Global})
  if (IsCategoryUnlocked(Category)) Result.Append(GetEligibleUpgradesForCategory(Category));
 return Result;
}

TArray<UUpgradeDefinition*> UPlayerUpgradeComponent::RollMixedChoices(const TArray<UUpgradeDefinition*>& Candidates, int32 Count) const
{
 TArray<UUpgradeDefinition*> Remaining, Result;
 for (auto* Candidate : Candidates)
  if (Candidate && Candidate->Category != EUpgradeCategory::Synergy) Remaining.Add(Candidate);
 if (Count <= 0) return Result;
 const auto Preferred = GetCurrentPlayerLevel() % 2 == 0 ? EUpgradeCategory::Samurai : EUpgradeCategory::Ninja;
 auto Pick = [&](TFunctionRef<bool(const UUpgradeDefinition*)> Predicate)
 {
  TArray<UUpgradeDefinition*> Matching;
  for (auto* Card : Remaining) if (Predicate(Card)) Matching.Add(Card);
  if (Matching.IsEmpty() || Result.Num() >= Count) return;
  auto* Card = Matching[FMath::RandRange(0, Matching.Num()-1)];
  Result.Add(Card); Remaining.Remove(Card);
 };
 Pick([Preferred](const auto* C) { return C->Category == Preferred; });
 if (Result.IsEmpty()) Pick([](const auto* C) { return C->Category == EUpgradeCategory::Samurai || C->Category == EUpgradeCategory::Ninja; });
 const auto First = Result.IsEmpty() ? EUpgradeCategory::Cursed : Result[0]->Category;
 Pick([First](const auto* C) { return C->Category != First; });
 while (Result.Num() < Count && !Remaining.IsEmpty()) Pick([](const auto*) { return true; });
 for (int32 I=Result.Num()-1; I>0; --I) Result.Swap(I,FMath::RandRange(0,I));
 return Result;
}

bool UPlayerUpgradeComponent::BeginNormalLevelUpSelection(int32 ChoiceCount)
{
 ClearCurrentOffer();
 CurrentUpgradeChoices = RollMixedChoices(GetNormalLevelUpCandidates(), FMath::Max(0,ChoiceCount));
 bHasSelectedCategory = !CurrentUpgradeChoices.IsEmpty();
 bDraftToolOffer = bMixedDraftOffer = bHasSelectedCategory;
 BuildOffersFromCurrentChoices(true);
 return bHasSelectedCategory;
}

int32 UPlayerUpgradeComponent::GetRerollsRemaining() const
{
 return FMath::Max(0, FMath::RoundToInt(GetMetaSkillBonus(TEXT("Reroll"))) - RerollsUsed);
}

int32 UPlayerUpgradeComponent::GetBanishesRemaining() const
{
 return FMath::Max(0, FMath::RoundToInt(GetMetaSkillBonus(TEXT("Banish"))) - BanishesUsed);
}

TArray<UUpgradeDefinition*> UPlayerUpgradeComponent::GetDraftAlternatives() const
{
 TArray<UUpgradeDefinition*> Result;
 if (!bDraftToolOffer || !bHasSelectedCategory || CurrentUpgradeChoices.IsEmpty()) return Result;
 for (auto* Upgrade : bMixedDraftOffer ? GetNormalLevelUpCandidates() : GetEligibleUpgradesForCategory(SelectedCategory))
 {
  const bool bOffered = CurrentUpgradeChoices.ContainsByPredicate([Upgrade](const auto& Current)
  { return Current && Current->UpgradeId == Upgrade->UpgradeId; });
  if (!bOffered) Result.AddUnique(Upgrade);
 }
 return Result;
}

bool UPlayerUpgradeComponent::CanReroll() const
{
 return GetRerollsRemaining() > 0 && !GetDraftAlternatives().IsEmpty();
}

bool UPlayerUpgradeComponent::CanBanish() const
{
 // Require a replacement: banishing never consumes the level or leaves an empty draft.
 return GetBanishesRemaining() > 0 && !GetDraftAlternatives().IsEmpty();
}

bool UPlayerUpgradeComponent::RerollUpgrades()
{
 if (!CanReroll()) return false;
 auto Remaining = GetDraftAlternatives();
 const auto PreviousChoices = CurrentUpgradeChoices;
 const int32 Count = PreviousChoices.Num();
 CurrentUpgradeChoices.Reset();
 if (bMixedDraftOffer)
 {
  CurrentUpgradeChoices = RollMixedChoices(Remaining, Count);
        for (UUpgradeDefinition* Card : CurrentUpgradeChoices) Remaining.Remove(Card);
 }
 while (CurrentUpgradeChoices.Num() < Count && !Remaining.IsEmpty())
 {
  const int32 Index = FMath::RandRange(0, Remaining.Num() - 1);
  CurrentUpgradeChoices.Add(Remaining[Index]);
  Remaining.RemoveAtSwap(Index);
 }
 // Near exhaustion, retain enough old cards to keep the selection usable.
 for (UUpgradeDefinition* Previous : PreviousChoices)
 {
  if (CurrentUpgradeChoices.Num() >= Count) break;
  CurrentUpgradeChoices.Add(Previous);
 }
 ++RerollsUsed;
 BuildOffersFromCurrentChoices(true);
 return true;
}

bool UPlayerUpgradeComponent::BanishUpgrade(int32 ChoiceIndex)
{
 if (!CanBanish() || !CurrentUpgradeChoices.IsValidIndex(ChoiceIndex)) return false;
 auto Remaining = GetDraftAlternatives();
 BanishedUpgrades.Add(CurrentUpgradeChoices[ChoiceIndex]->UpgradeId);
 auto* Replacement = Remaining[FMath::RandRange(0, Remaining.Num() - 1)];
 CurrentUpgradeChoices[ChoiceIndex] = Replacement;
 // Retain the exact rarity/magnitude of the untouched cards.
 CurrentUpgradeOffers[ChoiceIndex] = MakeUpgradeOffer(Replacement);
 ++BanishesUsed;
 ApplyMilestoneGuarantee();
 RebuildOfferPresentation();
 return true;
}
