#pragma once
#include "PlayerUpgradeComponent.h"
#include "UpgradeDefinition.h"

namespace CrescentBuild
{
inline float Value(const UPlayerUpgradeComponent* U, FName Id, FName Key, float Default)
{
    const auto* Card = U ? U->FindUpgradeDefinition(Id) : nullptr;
    return Card ? Card->GetBalanceValue(Key, Default) : Default;
}
inline float Scaling(const UPlayerUpgradeComponent* U, FName Id, float Default)
{
    if (!U) return 0.f;
    const float Stored = U->GetAccumulatedUpgradeMagnitude(Id);
    return Stored > 0.f ? Stored : U->GetUpgradeLevelById(Id) * Value(U, Id, TEXT("PerRank"), Default);
}
inline float Chance(const UPlayerUpgradeComponent* U, FName Unlock, FName ScalingId, float Base, float PerRank)
{
    return U && U->HasUpgradeId(Unlock)
        ? FMath::Clamp(Value(U, Unlock, TEXT("Chance"), Base) + Scaling(U, ScalingId, PerRank), 0.f, 1.f) : 0.f;
}
bool TryKillAssist(UWorld* World);
}
