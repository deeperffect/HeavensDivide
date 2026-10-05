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
 TArray<FName> All={TEXT("ReturningFang"),TEXT("BarrageStance"),TEXT("GreatShuriken"),TEXT("CuttingReturn"),TEXT("RelentlessFang"),TEXT("FinalPursuit"),TEXT("FocusedVolley"),TEXT("ForkingProjectiles"),TEXT("Crescendo"),TEXT("NeedleRain"),TEXT("SerratedEdge"),TEXT("GrindingHalt"),TEXT("WideOrbit"),TEXT("BreakingWheel"),TEXT("HeavyShuriken"),TEXT("LingeringShuriken"),TEXT("EmbeddedBlades"),TEXT("FragmentDamage"),TEXT("FragmentLoad"),TEXT("FragmentReach")};
 for (const auto& Pair : State.Levels)
  if (Pair.Key.ToString().StartsWith(TEXT("Fang")) || Pair.Key.ToString().StartsWith(TEXT("Barrage")) || Pair.Key.ToString().StartsWith(TEXT("Shuriken"))) All.AddUnique(Pair.Key);
 for(FName Id:All){State.NinjaMastery=FMath::Max(0,State.NinjaMastery-State.Levels.FindRef(Id));State.Levels.Remove(Id);State.Definitions.Remove(Id);State.AccumulatedMagnitudes.Remove(Id);}
 if(auto* N=GetCharacterManager()?GetCharacterManager()->GetNinja():nullptr)
  if(auto* B=N->FindComponentByClass<UNinjaBuildComponent>())B->ClearProjectiles();
 U->RestoreRunState(State);
 auto Grant=[U](FName Id){if(auto* Card=U->FindUpgradeDefinition(Id))U->AcquireUpgrade(Card);};
 if(Fang)for(auto Id:{TEXT("ReturningFang"),TEXT("FangTwin"),TEXT("RelentlessFang"),TEXT("FangResonance"),TEXT("FangDeadeye"),TEXT("FangAssist"),TEXT("FangSplinter"),TEXT("FangDamage"),TEXT("FangSpeed"),TEXT("FangRange"),TEXT("FangCloseQuarters"),TEXT("FangKillingEdge"),TEXT("FangTwinFrequency"),TEXT("FangPressure"),TEXT("FangResonantReach"),TEXT("FangCriticalChance"),TEXT("FangAssistChance"),TEXT("FangSplinterPower")})Grant(Id);
 if(Barrage)for(auto Id:{TEXT("BarrageStance"),TEXT("NeedleRain"),TEXT("ForkingProjectiles"),TEXT("BarragePool"),TEXT("BarrageAssist"),TEXT("BarrageCritical"),TEXT("BarrageRush"),TEXT("BarrageDamage"),TEXT("BarrageSpeed"),TEXT("BarrageRange"),TEXT("BarragePoisonDuration"),TEXT("BarragePoisonLoad"),TEXT("BarrageRainFrequency"),TEXT("BarrageSplitChance"),TEXT("BarrageAssistChance"),TEXT("BarragePoolRadius"),TEXT("BarrageCriticalChance"),TEXT("BarragePoisonCap")})Grant(Id);
 if(Wheel)for(auto Id:{TEXT("GreatShuriken"),TEXT("GrindingHalt"),TEXT("WideOrbit"),TEXT("BreakingWheel"),TEXT("ShurikenTwin"),TEXT("ShurikenAssist"),TEXT("SerratedEdge"),TEXT("ShurikenDamage"),TEXT("ShurikenSpeed"),TEXT("ShurikenSize"),TEXT("LingeringShuriken"),TEXT("ShurikenTempo"),TEXT("ShurikenGrindDuration"),TEXT("ShurikenGrowth"),TEXT("ShurikenBurstPower"),TEXT("ShurikenTwinFrequency"),TEXT("ShurikenAssistChance"),TEXT("ShurikenGrooves")})Grant(Id);
 ClientMessage(FString::Printf(TEXT("Ninja build: %s. New Ninja route cards replaced for this run only."),*BuildName));
#endif
}
