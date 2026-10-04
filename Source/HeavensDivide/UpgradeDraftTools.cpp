#include "PlayerUpgradeComponent.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

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
 for (auto* Upgrade : GetEligibleUpgradesForCategory(SelectedCategory))
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
