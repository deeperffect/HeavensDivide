#include "ChestRewardRoll.h"

TConstArrayView<FChestRewardTier> ChestRewards::Tiers()
{
    static const FChestRewardTier Values[]={
        {EChestRewardTier::Common,TEXT("COMMON"),7000,80,120,3,5,1,1},
        {EChestRewardTier::Rare,TEXT("RARE"),2300,125,175,6,9,1,2},
        {EChestRewardTier::Epic,TEXT("EPIC"),600,200,300,12,18,2,3},
        {EChestRewardTier::Jackpot,TEXT("JACKPOT"),100,500,800,40,60,6,10}
    };
    return MakeArrayView(Values);
}

FChestRewardValues ChestRewards::Roll(int32 BaseXP,FRandomStream& Random)
{
    FChestRewardValues Result;
    // Invalid/empty chests do not mint currency.
    if(BaseXP<=0)return Result;
    int32 Weight=0;
    for(const auto& Tier:Tiers())Weight+=Tier.Weight;
    int32 Ticket=Random.RandRange(0,Weight-1);
    for(const auto& Tier:Tiers())
    {
        if(Ticket>=Tier.Weight){Ticket-=Tier.Weight;continue;}
        Result.Tier=Tier.Tier;
        const int64 Scaled=int64(BaseXP)*Random.RandRange(Tier.MinXPPercent,Tier.MaxXPPercent);
        Result.XP=static_cast<int32>(FMath::Clamp<int64>((Scaled+50)/100,1,MAX_int32));
        Result.Embers=Random.RandRange(Tier.MinEmbers,Tier.MaxEmbers);
        Result.Sigils=Random.RandRange(Tier.MinSigils,Tier.MaxSigils);
        return Result;
    }
    return Result;
}

const TCHAR* ChestRewards::TierName(EChestRewardTier Tier)
{
    for(const auto& Definition:Tiers())if(Definition.Tier==Tier)return Definition.Name;
    return TEXT("COMMON");
}
