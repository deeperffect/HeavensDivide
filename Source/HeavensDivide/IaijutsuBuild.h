#pragma once
#include "PlayerUpgradeComponent.h"
#include "UpgradeDefinition.h"

namespace IaijutsuBuild
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
inline float Charge(const UPlayerUpgradeComponent* U)
{
    float Duration = Value(U, TEXT("Iaijutsu"), TEXT("ChargeDuration"), 1.f)
        / FMath::Max(.01f, 1.f + Scaling(U, TEXT("IaijutsuChargeSpeed"), .15f));
    if (U && U->HasUpgradeId(TEXT("IaijutsuPowerPact"))) Duration *= 1.3f;
    return FMath::Max(.01f, Duration);
}
inline float VacuumReach(const UPlayerUpgradeComponent* U)
{
    return FMath::Max(0.f, Value(U, TEXT("Iaijutsu"), TEXT("VacuumReach"), 60.f))
        * (1.f + Scaling(U, TEXT("IaijutsuVacuumReach"), .15f));
}
inline float Chance(const UPlayerUpgradeComponent* U, FName Unlock, FName ScalingId, float Base, float PerRank)
{
    return U && U->HasUpgradeId(Unlock)
        ? FMath::Clamp(Value(U, Unlock, TEXT("Chance"), Base) + Scaling(U, ScalingId, PerRank), 0.f, 1.f) : 0.f;
}
}
