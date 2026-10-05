#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerUpgradeComponent.h"
#include "PlayerUpgradeRules.h"
#include "SurvivorPlayerController.h"
#include "HealthComponent.h"
#include "BarrageBuild.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStanceDependenciesTest,"HeavensDivide.Combat.StanceDependencies",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStanceDependenciesTest::RunTest(const FString&)
{
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());
 auto* C=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 auto* PC=W->SpawnActor<ASurvivorPlayerController>(C);
 if (!TestNotNull(TEXT("Saved controller"),PC)) { W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return false; }
 PC->GetPlayerHealthComponent()->RestoreCurrentHealth(100);auto* U=PC->GetPlayerUpgrades();
 const auto& Pool=*FindFProperty<FArrayProperty>(UPlayerUpgradeComponent::StaticClass(),TEXT("UpgradePool"))->ContainerPtrToValuePtr<TArray<TObjectPtr<UUpgradeDefinition>>>(U);
 FPlayerUpgradeRunState Empty;U->CaptureRunState(Empty);
 auto Card=[&](FName Id){return U->FindUpgradeDefinition(Id);};
 auto Grant=[&](FName Id){return U->AcquireUpgrade(Card(Id));};
 auto Reset=[&](FName Id){U->RestoreRunState(Empty);TestTrue(TEXT("Acquire selected stance"),Grant(Id));};
 auto Offered=[&](FName Id){return U->GetCurrentUpgradeChoices().ContainsByPredicate([&](const UUpgradeDefinition* A){return A&&A->UpgradeId==Id;});};
 const FName Stances[]={TEXT("BattleStance"),TEXT("Iaijutsu"),TEXT("BladeWave"),TEXT("ReturningFang"),TEXT("BarrageStance"),TEXT("GreatShuriken")};
 for(FName Stance:Stances)
 {
  Reset(Stance);
  for(UUpgradeDefinition* A:Pool)
   if(A)for(FName Other:Stances)
    if(Other!=Stance&&A->PrerequisiteUpgradeIds.Contains(Other))
     TestFalse(*FString::Printf(TEXT("%s cannot offer %s branch %s"),*Stance.ToString(),*Other.ToString(),*A->UpgradeId.ToString()),U->CanAcquireUpgrade(const_cast<UUpgradeDefinition*>(A)));
 }
 // Explicit mechanic branches, including Shrine cards whose benefit needs an unlock.
 const FName Branches[][3]={
  {TEXT("BattleStance"),TEXT("DoubleCut"),TEXT("DoubleCutFrequency")},
  {TEXT("BattleStance"),TEXT("BloodEcho"),TEXT("BloodEchoChance")},
  {TEXT("BattleStance"),TEXT("BloodCritical"),TEXT("BloodCriticalChance")},
  {TEXT("BattleStance"),TEXT("BloodAssist"),TEXT("BloodAssistChance")},
  {TEXT("Iaijutsu"),TEXT("IaijutsuDoubleCut"),TEXT("IaijutsuDoubleCutFrequency")},
  {TEXT("Iaijutsu"),TEXT("IaijutsuInstant"),TEXT("IaijutsuInstantChance")},
  {TEXT("Iaijutsu"),TEXT("IaijutsuAOE"),TEXT("IaijutsuAOEChance")},
  {TEXT("Iaijutsu"),TEXT("IaijutsuAssist"),TEXT("IaijutsuAssistChance")},
  {TEXT("Iaijutsu"),TEXT("IaijutsuChain"),TEXT("IaijutsuCascadePower")},
  {TEXT("Iaijutsu"),TEXT("IaijutsuDash"),TEXT("IaijutsuDashPower")},
  {TEXT("BladeWave"),TEXT("CrescentDoubleCut"),TEXT("CrescentDoubleCutFrequency")},
  {TEXT("BladeWave"),TEXT("CrescentSplit"),TEXT("CrescentSplitChance")},
  {TEXT("BladeWave"),TEXT("CrescentAssist"),TEXT("CrescentAssistChance")},
  {TEXT("BladeWave"),TEXT("CrescentArc"),TEXT("CrescentArcChance")},
  {TEXT("BladeWave"),TEXT("CrescentField"),TEXT("CrescentFieldPower")},
  {TEXT("BladeWave"),TEXT("CrescentField"),TEXT("CrescentFieldChance")},
  {TEXT("BladeWave"),TEXT("CrescentField"),TEXT("CrescentFieldPact")},
  {TEXT("BladeWave"),TEXT("CrescentField"),TEXT("CrescentEruptionPact")},
  {TEXT("ReturningFang"),TEXT("FangTwin"),TEXT("FangTwinFrequency")},
  {TEXT("ReturningFang"),TEXT("RelentlessFang"),TEXT("FangPressure")},
  {TEXT("ReturningFang"),TEXT("FangResonance"),TEXT("FangResonantReach")},
  {TEXT("ReturningFang"),TEXT("FangDeadeye"),TEXT("FangCriticalChance")},
  {TEXT("ReturningFang"),TEXT("FangAssist"),TEXT("FangAssistChance")},
  {TEXT("ReturningFang"),TEXT("FangSplinter"),TEXT("FangSplinterPower")},
  {TEXT("ReturningFang"),TEXT("FangDeadeye"),TEXT("FangPredator")},
  {TEXT("BarrageStance"),TEXT("NeedleRain"),TEXT("BarrageRainFrequency")},
  {TEXT("BarrageStance"),TEXT("ForkingProjectiles"),TEXT("BarrageSplitChance")},
  {TEXT("BarrageStance"),TEXT("BarrageAssist"),TEXT("BarrageAssistChance")},
  {TEXT("BarrageStance"),TEXT("BarragePool"),TEXT("BarragePoolRadius")},
  {TEXT("BarrageStance"),TEXT("BarrageCritical"),TEXT("BarrageCriticalChance")},
  {TEXT("BarrageStance"),TEXT("BarragePool"),TEXT("BarrageBloom")},
  {TEXT("GreatShuriken"),TEXT("GrindingHalt"),TEXT("ShurikenGrindDuration")},
  {TEXT("GreatShuriken"),TEXT("WideOrbit"),TEXT("ShurikenGrowth")},
  {TEXT("GreatShuriken"),TEXT("BreakingWheel"),TEXT("ShurikenBurstPower")},
  {TEXT("GreatShuriken"),TEXT("ShurikenTwin"),TEXT("ShurikenTwinFrequency")},
  {TEXT("GreatShuriken"),TEXT("ShurikenAssist"),TEXT("ShurikenAssistChance")},
  {TEXT("GreatShuriken"),TEXT("SerratedEdge"),TEXT("ShurikenGrooves")},
  {TEXT("GreatShuriken"),TEXT("BreakingWheel"),TEXT("ShurikenPulse")}
 };
 for(const auto& Row:Branches)
 {
  Reset(Row[0]);if(!TestNotNull(*Row[2].ToString(),Card(Row[2])))continue;
  TestFalse(*FString::Printf(TEXT("%s requires %s"),*Row[2].ToString(),*Row[1].ToString()),U->CanAcquireUpgrade(Card(Row[2])));
  TestTrue(TEXT("Acquire required mechanic"),Grant(Row[1]));
  TestTrue(TEXT("Branch available with mechanic"),U->CanAcquireUpgrade(Card(Row[2])));
 }
 const FName Conflicts[][3]={
  {TEXT("BattleStance"),TEXT("BloodDetonation"),TEXT("BloodTransfer")},
  {TEXT("Iaijutsu"),TEXT("IaijutsuDashPact"),TEXT("IaijutsuMarkDamage")},
  {TEXT("Iaijutsu"),TEXT("IaijutsuDashPact"),TEXT("IaijutsuMarkPact")},
  {TEXT("Iaijutsu"),TEXT("IaijutsuMarkPact"),TEXT("IaijutsuDashPact")},
  {TEXT("BladeWave"),TEXT("CrescentEruptionPact"),TEXT("CrescentSlow")},
  {TEXT("BladeWave"),TEXT("CrescentEruptionPact"),TEXT("CrescentSlowDuration")}
 };
 for(const auto& Row:Conflicts)
 {
  Reset(Row[0]);if(Row[0]==TEXT("BladeWave"))Grant(TEXT("CrescentField"));
  TestTrue(TEXT("Acquire tradeoff"),Grant(Row[1]));
  FPlayerUpgradeRunState Saved;U->CaptureRunState(Saved);
  for(bool bRestored:{false,true})
  {
   if(bRestored)U->RestoreRunState(Saved);
   TestFalse(TEXT("Conflicting card rejected directly"),Grant(Row[2]));
   TestFalse(TEXT("Debug cannot bypass conflict"),U->DebugForceAcquireUpgrade(Card(Row[2])));
   auto* Stale=DuplicateObject<UUpgradeDefinition>(Card(Row[2]),U);Stale->PrerequisiteUpgradeIds.Empty();Stale->PrerequisiteRequirements.Empty();
   TestFalse(TEXT("Stale definition cannot bypass conflict"),U->AcquireUpgrade(Stale));
   U->BeginDirectUpgradeSelection(1000);TestFalse(TEXT("Unrestricted reward excludes conflict"),Offered(Row[2]));
   const bool bNinja=Row[0]==TEXT("BarrageStance");
   TestFalse(TEXT("Normal category excludes conflict"),U->GetEligibleUpgradesForCategory(bNinja?EUpgradeCategory::Ninja:EUpgradeCategory::Samurai).Contains(Card(Row[2])));
   U->BeginDirectCategoryUpgradeSelection(bNinja?EUpgradeCategory::NinjaTrial:EUpgradeCategory::SamuraiTrial,1000);TestFalse(TEXT("Repeat trial excludes conflict"),Offered(Row[2]));
   U->BeginBloodShrineSelection(1000);TestFalse(TEXT("Shrine excludes conflicting pact"),Offered(Row[2]));
  }
 }
 Reset(TEXT("BarrageStance"));Grant(TEXT("ForkingProjectiles"));for(int32 I=0;I<5;++I)Grant(TEXT("BarrageSplitChance"));
 const float Before=BarrageBuild::SplitChance(U);const int32 Mastery=U->GetNinjaMasteryPoints();
 TestTrue(TEXT("Procession remains available after split investment"),Grant(TEXT("BarrageProcession")));
 TestTrue(TEXT("Previous ranks still improve ricochet retention"),FMath::IsNearlyEqual(.75f+BarrageBuild::SplitChance(U)*.2f,.88f,.001f));
 FPlayerUpgradeRunState Saved;U->CaptureRunState(Saved);U->RestoreRunState(Saved);U->RestoreRunState(Saved);
 TestEqual(TEXT("Save restoration preserves split investment"),BarrageBuild::SplitChance(U),Before);
 TestEqual(TEXT("Only pact acquisition adds mastery"),U->GetNinjaMasteryPoints(),Mastery+1);
 // Verify the actual reward presentation, including fixed-rarity rewards and restoration.
 auto CheckCopy=[&](FName Id,const TCHAR* Name,const TCHAR* Fragment) {
  U->BeginDirectCategoryUpgradeSelection(EUpgradeCategory::NinjaTrial,1000);
  if (Id==TEXT("BloodTransferArea") || Id.ToString().StartsWith(TEXT("Crescent")))
   U->BeginDirectCategoryUpgradeSelection(EUpgradeCategory::SamuraiTrial,1000);
  const auto& Choices=U->GetCurrentUpgradeChoices();
  const auto* Found=Choices.FindByPredicate([&](const UUpgradeDefinition* A){return A&&A->UpgradeId==Id;});
  if(TestTrue(TEXT("Converted card remains offered"),Found!=nullptr)) {
   TestEqual(TEXT("Contextual card name"),(*Found)->DisplayName.ToString(),FString(Name));
   TestTrue(TEXT("Contextual benefit described"),(*Found)->Description.ToString().Contains(Fragment));
   TestTrue(TEXT("Saved definition is not changed"),*Found!=Card(Id));
  }
 };
 Reset(TEXT("BarrageStance"));Grant(TEXT("BarrageProcession"));
 CheckCopy(TEXT("ForkingProjectiles"),TEXT("Rebounding Blades"),TEXT("3 percentage points"));
 TestTrue(TEXT("Buy ricochet unlock after pact"),Grant(TEXT("ForkingProjectiles")));
 CheckCopy(TEXT("BarrageSplitChance"),TEXT("Ricochet Mastery"),TEXT("2 percentage points"));
 U->CaptureRunState(Saved);U->RestoreRunState(Saved);
 CheckCopy(TEXT("BarrageSplitChance"),TEXT("Ricochet Mastery"),TEXT("2 percentage points"));
 TestTrue(TEXT("Buy ricochet scaling after restore"),Grant(TEXT("BarrageSplitChance")));
 Reset(TEXT("BarrageStance"));
 TestEqual(TEXT("Base card keeps original name"),Card(TEXT("ForkingProjectiles"))->DisplayName.ToString(),FString(TEXT("Forked Blades")));
 Reset(TEXT("BladeWave"));Grant(TEXT("CrescentField"));Grant(TEXT("CrescentEruptionPact"));
 CheckCopy(TEXT("CrescentFieldPower"),TEXT("Violent Wake"),TEXT("15%"));
 CheckCopy(TEXT("CrescentFieldChance"),TEXT("Restless Eruption"),TEXT("erupting trail"));
 Reset(TEXT("BattleStance"));Grant(TEXT("BloodDetonation"));
 CheckCopy(TEXT("BloodTransferArea"),TEXT("Crimson Reach"),TEXT("Detonation radius"));
 Reset(TEXT("BarrageStance"));Grant(TEXT("BarragePool"));Grant(TEXT("BarrageBloom"));
 CheckCopy(TEXT("BarragePoolRadius"),TEXT("Spreading Bloom"),TEXT("20%"));
 Reset(TEXT("GreatShuriken"));Grant(TEXT("ShurikenHunger"));
 CheckCopy(TEXT("ShurikenSize"),TEXT("Ravenous Growth"),TEXT("size gained per kill"));
 // Mechanic replacements with a retained effect must keep their useful branches.
 Reset(TEXT("BattleStance"));TestFalse(TEXT("Area needs transfer or detonation"),U->CanAcquireUpgrade(Card(TEXT("BloodTransferArea"))));Grant(TEXT("BloodDetonation"));TestTrue(TEXT("Detonation keeps area useful"),Grant(TEXT("BloodTransferArea")));
 Reset(TEXT("BarrageStance"));Grant(TEXT("BarragePool"));Grant(TEXT("BarrageBloom"));for(FName Id:{FName(TEXT("BarragePoolRadius")),FName(TEXT("BarragePoisonDuration")),FName(TEXT("BarragePoisonLoad")),FName(TEXT("BarragePoisonCap"))})TestTrue(TEXT("Bloom retains poison and radius scaling"),U->CanAcquireUpgrade(Card(Id)));
 Reset(TEXT("GreatShuriken"));Grant(TEXT("BreakingWheel"));Grant(TEXT("WideOrbit"));Grant(TEXT("GrindingHalt"));Grant(TEXT("ShurikenOrbit"));Grant(TEXT("ShurikenHunger"));Grant(TEXT("ShurikenPulse"));
 for(FName Id:{FName(TEXT("ShurikenSize")),FName(TEXT("ShurikenGrowth")),FName(TEXT("ShurikenBurstPower")),FName(TEXT("ShurikenGrindDuration")),FName(TEXT("ShurikenTempo")),FName(TEXT("LingeringShuriken"))})TestTrue(TEXT("Combined shuriken pacts retain useful scaling"),U->CanAcquireUpgrade(Card(Id)));
 W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif
