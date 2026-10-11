#pragma once
#include "PlayerUpgradeComponent.h"
#include "UpgradeDefinition.h"

/** Named values live on the same saved upgrade edited by the balance workspace. */
namespace UpgradeBalance
{
inline float Value(const UPlayerUpgradeComponent* U,FName Id,FName Key,float Default)
{
    const auto* Card=U?U->FindUpgradeDefinition(Id):nullptr;
    return Card?Card->GetBalanceValue(Key,Default):Default;
}
inline float Rank(const UPlayerUpgradeComponent* U,FName Id,float Default)
{
    if(!U)return 0.f;
    const auto* Card=U->FindUpgradeDefinition(Id);
    return Card&&Card->bUsesRolledRarity?U->GetAccumulatedUpgradeMagnitude(Id):U->GetUpgradeLevelById(Id)*Value(U,Id,TEXT("PerRank"),Default);
}
inline int32 BloodCap(const UPlayerUpgradeComponent* U)
{
    return FMath::Clamp(FMath::RoundToInt(Value(U,TEXT("BattleStance"),TEXT("BleedStackCap"),5.f)+Rank(U,TEXT("BloodCapacity"),1.f)),1,1000);
}
}
