#include "ShurikenBuild.h"
#include "BarrageBuild.h"
#include "PlayerUpgradeComponent.h"
bool ShurikenBuild::IsUpgrade(FName Id)
{
 return Id.ToString().StartsWith(TEXT("Shuriken")) || Id==TEXT("SerratedEdge") || Id==TEXT("GrindingHalt") ||
 Id==TEXT("WideOrbit") || Id==TEXT("BreakingWheel") || Id==TEXT("LingeringShuriken");
}
bool ShurikenBuild::IsShrine(FName Id)
{ return Id==TEXT("ShurikenOrbit") || Id==TEXT("ShurikenHunger") || Id==TEXT("ShurikenPulse"); }
float ShurikenBuild::ContactInterval(const UPlayerUpgradeComponent* U)
{ return FMath::Max(.05f,BarrageBuild::Value(U,TEXT("GreatShuriken"),TEXT("HitInterval"),.25f)/(1.f+BarrageBuild::Scaling(U,TEXT("ShurikenTempo"),.15f))); }
float ShurikenBuild::PulseInterval(const UPlayerUpgradeComponent* U)
{ return FMath::Max(.2f,BarrageBuild::Value(U,TEXT("ShurikenPulse"),TEXT("Interval"),1.f)/(1.f+BarrageBuild::Scaling(U,TEXT("ShurikenTempo"),.15f))); }
