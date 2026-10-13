#pragma once
#include "CoreMinimal.h"

enum class EChestRewardTier : uint8 { Common, Rare, Epic, Jackpot };

struct FChestRewardValues
{
    EChestRewardTier Tier=EChestRewardTier::Common;
    int32 XP=0,Embers=0,Sigils=0;
    bool IsJackpot() const { return Tier==EChestRewardTier::Jackpot; }
};

struct FChestRewardTier
{
    EChestRewardTier Tier;
    const TCHAR* Name;
    int32 Weight,MinXPPercent,MaxXPPercent,MinEmbers,MaxEmbers,MinSigils,MaxSigils;
};

namespace ChestRewards
{
    HEAVENSDIVIDE_API TConstArrayView<FChestRewardTier> Tiers();
    HEAVENSDIVIDE_API FChestRewardValues Roll(int32 BaseXP,FRandomStream& Random);
    HEAVENSDIVIDE_API const TCHAR* TierName(EChestRewardTier Tier);
}
