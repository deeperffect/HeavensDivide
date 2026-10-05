#include "BarrageBuild.h"
#include "FangBuild.h"
#include "PlayerUpgradeComponent.h"
bool BarrageBuild::IsUpgrade(FName Id)
{ return Id == TEXT("NeedleRain") || Id == TEXT("ForkingProjectiles") || (Id != TEXT("BarrageStance") && Id.ToString().StartsWith(TEXT("Barrage"))); }
bool BarrageBuild::IsShrine(FName Id)
{ return Id == TEXT("BarragePointBlank") || Id == TEXT("BarrageProcession") || Id == TEXT("BarrageBloom"); }
bool BarrageBuild::IsLegacy(FName Id)
{ return FangBuild::IsLegacyShared(Id) || Id == TEXT("FocusedVolley") || Id == TEXT("Crescendo"); }
float BarrageBuild::Value(const UPlayerUpgradeComponent* U,FName Id,FName Key,float Default)
{ const auto* C=U?U->FindUpgradeDefinition(Id):nullptr;return C?C->GetBalanceValue(Key,Default):Default; }
float BarrageBuild::Scaling(const UPlayerUpgradeComponent* U,FName Id,float Default)
{ return FangBuild::Scaling(U,Id,Default); }
float BarrageBuild::SplitChance(const UPlayerUpgradeComponent* U)
{ return U && U->HasUpgradeId(TEXT("ForkingProjectiles")) ? FMath::Clamp(Value(U,TEXT("ForkingProjectiles"),TEXT("Chance"),.15f)+Scaling(U,TEXT("BarrageSplitChance"),.1f),0.f,1.f):0.f; }
float BarrageBuild::RangeMultiplier(const UPlayerUpgradeComponent* U)
{ return (1.f+Scaling(U,TEXT("BarrageRange"),.15f))*(U&&U->HasUpgradeId(TEXT("BarragePointBlank"))?Value(U,TEXT("BarragePointBlank"),TEXT("RangeMultiplier"),.3f):1.f); }
