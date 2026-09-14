#include "SurvivorPlayerController.h"
#include "NinjaBuildComponent.h"
#include "PlayerUpgradeComponent.h"
#include "CharacterManagerComponent.h"
#include "NinjaCharacter.h"
#include "AutoAttackComponent.h"

void ASurvivorPlayerController::NinjaBuildPreview(const FString& BuildName)
{
#if !UE_BUILD_SHIPPING
 if(!IsRunInProgress()||!GetPlayerUpgrades())return;
 const bool Fang=BuildName.Equals(TEXT("Fang"),ESearchCase::IgnoreCase),Barrage=BuildName.Equals(TEXT("Barrage"),ESearchCase::IgnoreCase),Wheel=BuildName.Equals(TEXT("Shuriken"),ESearchCase::IgnoreCase);
 if(!Fang&&!Barrage&&!Wheel&&!BuildName.Equals(TEXT("Clear"),ESearchCase::IgnoreCase)){ClientMessage(TEXT("NinjaBuildPreview Fang | Barrage | Shuriken | Clear"));return;}
 auto* U=GetPlayerUpgrades();FPlayerUpgradeRunState State;U->CaptureRunState(State);
 const TArray<FName> All={TEXT("ReturningFang"),TEXT("BarrageStance"),TEXT("GreatShuriken"),TEXT("CuttingReturn"),TEXT("RelentlessFang"),TEXT("FinalPursuit"),TEXT("FocusedVolley"),TEXT("ForkingProjectiles"),TEXT("Crescendo"),TEXT("NeedleRain"),TEXT("SerratedEdge"),TEXT("GrindingHalt"),TEXT("WideOrbit"),TEXT("BreakingWheel"),TEXT("EmbeddedBlades"),TEXT("FragmentDamage"),TEXT("FragmentLoad"),TEXT("FragmentReach")};
 for(FName Id:All){State.NinjaMastery=FMath::Max(0,State.NinjaMastery-State.Levels.FindRef(Id));State.Levels.Remove(Id);State.Definitions.Remove(Id);State.AccumulatedMagnitudes.Remove(Id);}
 if(auto* N=GetCharacterManager()?GetCharacterManager()->GetNinja():nullptr)
  if(auto* B=N->FindComponentByClass<UNinjaBuildComponent>())B->ClearProjectiles();
 U->RestoreRunState(State);
 auto Grant=[U](FName Id){if(auto* Card=U->FindUpgradeDefinition(Id))U->AcquireUpgrade(Card);};
 if(Fang)for(auto Id:{TEXT("ReturningFang"),TEXT("CuttingReturn"),TEXT("RelentlessFang"),TEXT("FinalPursuit")})Grant(Id);
 if(Barrage)for(auto Id:{TEXT("BarrageStance"),TEXT("FocusedVolley"),TEXT("ForkingProjectiles"),TEXT("Crescendo"),TEXT("NeedleRain")})Grant(Id);
 if(Wheel)for(auto Id:{TEXT("GreatShuriken"),TEXT("SerratedEdge"),TEXT("GrindingHalt"),TEXT("WideOrbit"),TEXT("BreakingWheel")})Grant(Id);
 if(Fang||Barrage||Wheel)for(auto Id:{TEXT("EmbeddedBlades"),TEXT("FragmentDamage"),TEXT("FragmentLoad"),TEXT("FragmentReach")})Grant(Id);
 ClientMessage(FString::Printf(TEXT("Ninja build: %s. New Ninja route cards replaced for this run only."),*BuildName));
#endif
}
