#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ChestRewardRoll.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChestRewardRollTest,"HeavensDivide.Rewards.ChestTiers",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FChestRewardRollTest::RunTest(const FString&)
{
    constexpr int32 Samples=100000,BaseXP=123;
    FRandomStream Random(20261011),Repeat(20261011);
    int32 Counts[4]={};
    int64 XPSum=0,EmberSum=0,SigilSum=0;
    for(int32 I=0;I<Samples;++I)
    {
        const auto Roll=ChestRewards::Roll(BaseXP,Random),Same=ChestRewards::Roll(BaseXP,Repeat);
        if(Roll.Tier!=Same.Tier||Roll.XP!=Same.XP||Roll.Embers!=Same.Embers||Roll.Sigils!=Same.Sigils)
        {AddError(TEXT("Seeded rewards must reproduce every value"));return false;}
        const auto& Tier=ChestRewards::Tiers()[int32(Roll.Tier)];
        const int32 MinXP=(BaseXP*Tier.MinXPPercent+50)/100,MaxXP=(BaseXP*Tier.MaxXPPercent+50)/100;
        if(Roll.XP<MinXP||Roll.XP>MaxXP||Roll.Embers<Tier.MinEmbers||Roll.Embers>Tier.MaxEmbers||Roll.Sigils<Tier.MinSigils||Roll.Sigils>Tier.MaxSigils)
        {AddError(TEXT("Chest reward escaped its tier bounds"));return false;}
        ++Counts[int32(Roll.Tier)];XPSum+=Roll.XP;EmberSum+=Roll.Embers;SigilSum+=Roll.Sigils;
    }
    for(const auto& Tier:ChestRewards::Tiers())
    {
        const double Expected=double(Tier.Weight)/10000;
        const double Observed=double(Counts[int32(Tier.Tier)])/Samples;
        TestTrue(FString::Printf(TEXT("%s chance agrees with weight"),Tier.Name),FMath::Abs(Expected-Observed)<.005);
        TestTrue(TEXT("Every tier is reachable"),Counts[int32(Tier.Tier)]>0);
    }
    const auto Empty=ChestRewards::Roll(0,Random),Negative=ChestRewards::Roll(-10,Random);
    TestEqual(TEXT("Empty chest XP"),Empty.XP,0);
    TestEqual(TEXT("Empty chest currency"),Empty.Embers+Empty.Sigils,0);
    TestEqual(TEXT("Negative chest currency"),Negative.Embers+Negative.Sigils,0);
    for(int32 I=0;I<100;++I)
        TestTrue(TEXT("Large XP cannot wrap negative"),ChestRewards::Roll(MAX_int32,Random).XP>0);
    AddInfo(FString::Printf(TEXT("100000 chest rolls: XP %.3fx, Embers %.3f, Sigils %.3f; Jackpot count %d"),
        double(XPSum)/Samples/BaseXP,double(EmberSum)/Samples,double(SigilSum)/Samples,Counts[3]));
    return !HasAnyErrors();
}
#endif
