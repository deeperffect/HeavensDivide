#pragma once
#include "CoreMinimal.h"

// Stable IDs and short descriptions for the pre-run loadout.
namespace RunStances
{
struct FStance
{
    const TCHAR* Id;
    bool bSamurai;
    const TCHAR* Name;
    const TCHAR* Tagline;
    const TCHAR* Summary;
};
inline const FStance All[] = {
    {TEXT("BattleStance"), true, TEXT("BLOOD"), TEXT("CLOSE COMBAT / BLEED"), TEXT("Fast, wide melee strikes that stack bleeding wounds.")},
    {TEXT("Iaijutsu"), true, TEXT("IAIJUTSU"), TEXT("CHARGED SLASH / MARK"), TEXT("Charge a pulling slash. Mark enemies to amplify follow-up hits.")},
    {TEXT("BladeWave"), true, TEXT("CRESCENT"), TEXT("PIERCING WAVES / SLOW"), TEXT("Piercing blade waves damage and slow enemies in their path.")},
    {TEXT("ReturningFang"), false, TEXT("RETURNING FANG"), TEXT("HOMING / PRECISION"), TEXT("A seeking blade strikes and returns. Stay close for faster attacks.")},
    {TEXT("BarrageStance"), false, TEXT("BARRAGE"), TEXT("RAPID VOLLEYS / POISON"), TEXT("Rapid kunai volleys stack poison across groups of enemies.")},
    {TEXT("GreatShuriken"), false, TEXT("GIANT SHURIKEN"), TEXT("HEAVY BLADES / MULTI-HIT"), TEXT("Heavy piercing blades strike enemies repeatedly.")}
};
inline const FStance* Find(FName Id)
{
    for (const auto& Stance : All) if (Id == Stance.Id) return &Stance;
    return nullptr;
}
inline bool IsDefaultUnlocked(FName Id)
{
    return Id == TEXT("BattleStance") || Id == TEXT("BarrageStance");
}
}
