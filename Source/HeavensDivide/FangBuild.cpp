#include "FangBuild.h"
#include "PlayerUpgradeComponent.h"

bool FangBuild::IsUpgrade(FName Id)
{
    return Id == TEXT("RelentlessFang") || Id.ToString().StartsWith(TEXT("Fang"));
}
bool FangBuild::IsShrine(FName Id)
{
    return Id == TEXT("FangFarstrider") || Id == TEXT("FangRedline") || Id == TEXT("FangPredator");
}
bool FangBuild::IsLegacyShared(FName Id)
{
    static const TSet<FName> Ids = {TEXT("ShadowStep"), TEXT("MultipleStrikes"), TEXT("AfterimageFrenzy"),
        TEXT("EmbeddedBlades"), TEXT("FragmentDamage"), TEXT("FragmentReach"), TEXT("FragmentLoad"), TEXT("VenomousKunai")};
    return Ids.Contains(Id);
}
float FangBuild::Scaling(const UPlayerUpgradeComponent* U, FName Id, float Default)
{
    if (!U || U->GetUpgradeLevelById(Id) <= 0) return 0;
    const auto* Card = U->FindUpgradeDefinition(Id);
    return Card && Card->bUsesRolledRarity ? U->GetAccumulatedUpgradeMagnitude(Id)
        : U->GetUpgradeLevelById(Id) * (Card ? Card->GetBalanceValue(TEXT("PerRank"), Default) : Default);
}
