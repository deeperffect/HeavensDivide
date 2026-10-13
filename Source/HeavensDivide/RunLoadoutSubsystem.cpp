#include "RunLoadoutSubsystem.h"
#include "RunStanceCatalog.h"
#include "PlayerUpgradeComponent.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "SurvivorPlayerController.h"

namespace
{
bool IsLoadoutStanceUnlocked(const UGameInstance* GameInstance, FName Id)
{
    const auto* Meta = GameInstance ? GameInstance->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
    return Meta ? Meta->IsStanceUnlocked(Id) : RunStances::IsDefaultUnlocked(Id);
}
}

UUpgradeDefinition* URunLoadoutSubsystem::FindSynergyDefinition(FName Id) const
{
    if (CachedSynergies.IsEmpty())
        if (const auto* GI = GetGameInstance())
            if (const auto* Meta = GI->GetSubsystem<USynergyMetaProgressionSubsystem>())
                for (auto* Definition : Meta->GetSynergyUpgradeDefinitions()) CachedSynergies.Add(Definition);
    for (UUpgradeDefinition* Definition : CachedSynergies)
        if (Definition && Definition->Category == EUpgradeCategory::Synergy && Definition->UpgradeId == Id) return Definition;
    return nullptr;
}

TArray<UUpgradeDefinition*> URunLoadoutSubsystem::GetAvailableSynergies() const
{
    FindSynergyDefinition(NAME_None); // Load the asset catalog once, without polling the registry in UI paint.
    const auto* GI = GetGameInstance();
    const auto* Meta = GI ? GI->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
    TArray<UUpgradeDefinition*> Result;
    if (Meta)
        for (UUpgradeDefinition* Definition : CachedSynergies)
            if (Definition && Definition->Category == EUpgradeCategory::Synergy && Meta->IsUpgradeMetaEligible(Definition)) Result.Add(Definition);
    return Result;
}

TArray<FName> URunLoadoutSubsystem::GetSelectedSynergies() const
{
    const auto Available = GetAvailableSynergies();
    TArray<FName> Result;
    for (FName Id : SelectedSynergies)
        if (Result.Num() < MaxSelectedSynergies && !Result.Contains(Id) &&
            Available.ContainsByPredicate([Id](const UUpgradeDefinition* Definition){return Definition->UpgradeId == Id;})) Result.Add(Id);
    return Result;
}

bool URunLoadoutSubsystem::CanEditSelections() const
{
    if (const UWorld* World = GetWorld())
        for (auto It = World->GetPlayerControllerIterator(); It; ++It)
            if (Cast<ASurvivorPlayerController>(It->Get())) return false;
    return true;
}

bool URunLoadoutSubsystem::SetSelectedSynergies(const TArray<FName>& UpgradeIds)
{
    if (!CanEditSelections() || UpgradeIds.Num() > MaxSelectedSynergies) return false;
    const auto Available = GetAvailableSynergies();
    TSet<FName> Seen;
    for (FName Id : UpgradeIds)
    {
        if (Seen.Contains(Id) || !Available.ContainsByPredicate([Id](const UUpgradeDefinition* Definition){return Definition->UpgradeId == Id;})) return false;
        Seen.Add(Id);
    }
    SelectedSynergies = UpgradeIds;
    return true;
}

bool URunLoadoutSubsystem::SetSynergySelected(FName Id, bool bSelected)
{
    TArray<FName> NewSelection = GetSelectedSynergies();
    // Unknown/locked IDs are rejected on both paths; callers never mutate saved unlocks.
    const auto Available = GetAvailableSynergies();
    if (!Available.ContainsByPredicate([Id](const UUpgradeDefinition* Definition){return Definition->UpgradeId == Id;})) return false;
    if (bSelected) NewSelection.AddUnique(Id); else NewSelection.Remove(Id);
    return SetSelectedSynergies(NewSelection);
}

bool URunLoadoutSubsystem::IsComboAbilityUnlocked(FName Id) const
{
    if (Id != TEXT("SoulConvergence")) return false;
    const auto* GI = GetGameInstance();
    const auto* Meta = GI ? GI->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
    return Meta && Meta->GetSkillBonus(TEXT("ComboUnlock")) > 0.f;
}

FName URunLoadoutSubsystem::GetSelectedComboAbility() const
{
    const FName Id = bExplicitComboChoice ? SelectedComboAbility : FName(TEXT("SoulConvergence"));
    return IsComboAbilityUnlocked(Id) ? Id : NAME_None;
}

bool URunLoadoutSubsystem::SetSelectedComboAbility(FName Id)
{
    if (!CanEditSelections() || (!Id.IsNone() && !IsComboAbilityUnlocked(Id))) return false;
    SelectedComboAbility = Id;
    bExplicitComboChoice = true;
    return true;
}

void URunLoadoutSubsystem::SanitizeSelections()
{
    if (!IsLoadoutStanceUnlocked(GetGameInstance(), SamuraiStance)) SamuraiStance = TEXT("BattleStance");
    if (!IsLoadoutStanceUnlocked(GetGameInstance(), NinjaStance)) NinjaStance = TEXT("BarrageStance");
    SelectedSynergies = GetSelectedSynergies();
    if (bExplicitComboChoice && !SelectedComboAbility.IsNone() && !IsComboAbilityUnlocked(SelectedComboAbility))
    {
        SelectedComboAbility = NAME_None;
        bExplicitComboChoice = false;
    }
}

bool URunLoadoutSubsystem::SetStances(FName Samurai, FName Ninja)
{
    const auto* Steel = RunStances::Find(Samurai);
    const auto* Shadow = RunStances::Find(Ninja);
    if (!Steel || !Steel->bSamurai || !Shadow || Shadow->bSamurai ||
        !IsLoadoutStanceUnlocked(GetGameInstance(), Samurai) || !IsLoadoutStanceUnlocked(GetGameInstance(), Ninja)) return false;
    SamuraiStance = Samurai;
    NinjaStance = Ninja;
    return true;
}

bool URunLoadoutSubsystem::ApplyToFreshRun(UPlayerUpgradeComponent* Upgrades)
{
    if (!Upgrades || !IsLoadoutStanceUnlocked(GetGameInstance(), SamuraiStance) ||
        !IsLoadoutStanceUnlocked(GetGameInstance(), NinjaStance)) return false;
    SanitizeSelections();
    auto* Steel = Upgrades->FindUpgradeDefinition(SamuraiStance);
    auto* Shadow = Upgrades->FindUpgradeDefinition(NinjaStance);
    if (!Steel || !Shadow) return false;
    // Validate the whole pair before granting either half; never replace an existing run's stance.
    if ((!Upgrades->HasUpgradeId(SamuraiStance) && !Upgrades->CanAcquireUpgrade(Steel)) ||
        (!Upgrades->HasUpgradeId(NinjaStance) && !Upgrades->CanAcquireUpgrade(Shadow)))
    {
        UE_LOG(LogTemp,Warning,TEXT("[RunLoadout] Could not apply %s / %s: run already contains conflicting upgrades."),*SamuraiStance.ToString(),*NinjaStance.ToString());
        return false;
    }
    if (!Upgrades->HasUpgradeId(SamuraiStance)) Upgrades->AcquireUpgrade(Steel);
    if (!Upgrades->HasUpgradeId(NinjaStance)) Upgrades->AcquireUpgrade(Shadow);
    bool bApplied=Upgrades->HasUpgradeId(SamuraiStance) && Upgrades->HasUpgradeId(NinjaStance);
    // Selected synergies begin at rank one. Reapplying setup never upgrades an existing selection.
    if (bApplied)
    {
        TGuardValue<bool> StartingSynergyGrant(Upgrades->bGrantingStartingSynergy, true);
        for (FName Id : GetSelectedSynergies())
            if (!Upgrades->HasUpgradeId(Id))
            {
                UUpgradeDefinition* Definition = Upgrades->FindUpgradeDefinition(Id);
                if (!Definition) Definition = FindSynergyDefinition(Id);
                if (!Definition || !Upgrades->AcquireUpgrade(Definition)) bApplied = false;
            }
    }
    if(bApplied)UE_LOG(LogTemp,Log,TEXT("[RunLoadout] Applied Samurai=%s Ninja=%s Synergies=%d Combo=%s"),*SamuraiStance.ToString(),*NinjaStance.ToString(),GetSelectedSynergies().Num(),*GetSelectedComboAbility().ToString());
    return bApplied;
}
