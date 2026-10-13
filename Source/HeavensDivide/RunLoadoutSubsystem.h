#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RunLoadoutSubsystem.generated.h"

class UPlayerUpgradeComponent;
class UUpgradeDefinition;

/** Starting equipment persists across map travel and retries; acquired run ranks stay in the run snapshot. */
UCLASS()
class HEAVENSDIVIDE_API URunLoadoutSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    static constexpr int32 MaxSelectedSynergies = 5;
    bool SetStances(FName Samurai, FName Ninja);
    bool ApplyToFreshRun(UPlayerUpgradeComponent* Upgrades);
    FName GetSamuraiStance() const { return SamuraiStance; }
    FName GetNinjaStance() const { return NinjaStance; }
    TArray<FName> GetSelectedSynergies() const;
    TArray<UUpgradeDefinition*> GetAvailableSynergies() const;
    bool SetSelectedSynergies(const TArray<FName>& UpgradeIds);
    bool SetSynergySelected(FName UpgradeId, bool bSelected);
    FName GetSelectedComboAbility() const;
    bool SetSelectedComboAbility(FName Id);
    bool IsComboAbilityUnlocked(FName Id) const;
    bool CanEditSelections() const;
    void SanitizeSelections();
private:
    friend class FRunLoadoutSelectionTest;
    UUpgradeDefinition* FindSynergyDefinition(FName Id) const;
    UPROPERTY(Transient) FName SamuraiStance = TEXT("BattleStance");
    UPROPERTY(Transient) FName NinjaStance = TEXT("BarrageStance");
    UPROPERTY(Transient) TArray<FName> SelectedSynergies = {FName(TEXT("TagTeam"))};
    UPROPERTY(Transient) FName SelectedComboAbility = NAME_None;
    UPROPERTY(Transient) bool bExplicitComboChoice = false;
    UPROPERTY(Transient) mutable TArray<TObjectPtr<UUpgradeDefinition>> CachedSynergies;
};
