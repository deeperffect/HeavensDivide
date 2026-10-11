#include "RunLoadoutSubsystem.h"
#include "RunStanceCatalog.h"
#include "PlayerUpgradeComponent.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "Engine/GameInstance.h"

namespace
{
bool IsLoadoutStanceUnlocked(const UGameInstance* GameInstance, FName Id)
{
    const auto* Meta = GameInstance ? GameInstance->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
    return Meta ? Meta->IsStanceUnlocked(Id) : RunStances::IsDefaultUnlocked(Id);
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

bool URunLoadoutSubsystem::ApplyToFreshRun(UPlayerUpgradeComponent* Upgrades) const
{
    if (!Upgrades || !IsLoadoutStanceUnlocked(GetGameInstance(), SamuraiStance) ||
        !IsLoadoutStanceUnlocked(GetGameInstance(), NinjaStance)) return false;
    auto* Steel = Upgrades->FindUpgradeDefinition(SamuraiStance);
    auto* Shadow = Upgrades->FindUpgradeDefinition(NinjaStance);
    if (!Steel || !Shadow) return false;
    if (Upgrades->HasUpgradeId(SamuraiStance) && Upgrades->HasUpgradeId(NinjaStance)) return true;
    // Validate the whole pair before granting either half; never replace an existing run's stance.
    if ((!Upgrades->HasUpgradeId(SamuraiStance) && !Upgrades->CanAcquireUpgrade(Steel)) ||
        (!Upgrades->HasUpgradeId(NinjaStance) && !Upgrades->CanAcquireUpgrade(Shadow)))
    {
        UE_LOG(LogTemp,Warning,TEXT("[RunLoadout] Could not apply %s / %s: run already contains conflicting upgrades."),*SamuraiStance.ToString(),*NinjaStance.ToString());
        return false;
    }
    if (!Upgrades->HasUpgradeId(SamuraiStance)) Upgrades->AcquireUpgrade(Steel);
    if (!Upgrades->HasUpgradeId(NinjaStance)) Upgrades->AcquireUpgrade(Shadow);
    const bool bApplied=Upgrades->HasUpgradeId(SamuraiStance) && Upgrades->HasUpgradeId(NinjaStance);
    if(bApplied)UE_LOG(LogTemp,Log,TEXT("[RunLoadout] Applied Samurai=%s Ninja=%s"),*SamuraiStance.ToString(),*NinjaStance.ToString());
    return bApplied;
}
