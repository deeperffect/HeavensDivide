#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSamuraiRosterTest,"HeavensDivide.Combat.SamuraiRoster",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSamuraiRosterTest::RunTest(const FString&)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 auto Class=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 auto* PC=World->SpawnActor<ASurvivorPlayerController>(Class);
 if(!TestNotNull(TEXT("Saved controller"),PC)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 auto* U=PC->GetPlayerUpgrades();
 for(auto Id:{TEXT("SamuraiTechnique.Cleaver"),TEXT("SamuraiTechnique.Duelist"),TEXT("SamuraiTechnique.Deathblow")})
 {
  TestNull(TEXT("Retired technique absent from pool"),U->FindUpgradeDefinition(Id));
  auto* Old=NewObject<UUpgradeDefinition>();Old->UpgradeId=Id;Old->Category=EUpgradeCategory::SamuraiTrial;
  TestFalse(TEXT("Stale technique reference cannot be acquired"),U->CanAcquireUpgrade(Old));
 }
 for(auto Id:{TEXT("BattleStance"),TEXT("Iaijutsu"),TEXT("BloodEcho"),TEXT("BloodCapacity"),TEXT("SamuraiHeavyBlade"),TEXT("SamuraiArea"),TEXT("DoubleCut"),TEXT("BladeWave"),TEXT("BloodTransfer"),TEXT("OverkillBurst"),TEXT("WaveMultishot")})
  TestNotNull(TEXT("Build-linked card retained"),U->FindUpgradeDefinition(Id));
 TestTrue(TEXT("Legacy Samurai trial still offers rewards"),U->BeginDirectCategoryUpgradeSelection(EUpgradeCategory::SamuraiTrial,3));
 TestEqual(TEXT("Trial uses current Samurai category"),U->GetSelectedCategory(),EUpgradeCategory::Samurai);
 for(auto* Choice:U->GetCurrentUpgradeChoices())
  TestTrue(TEXT("Trial offers eligible current build cards"),Choice&&Choice->Category==EUpgradeCategory::Samurai&&U->CanAcquireUpgrade(Choice));
 const TSet<FName> Disabled={TEXT("OverkillBurst"),TEXT("BurstRadius"),TEXT("WaveMultishot"),TEXT("CrossingBlades"),TEXT("SplinterWave"),TEXT("BladeWavePower"),TEXT("WideArc"),TEXT("BladeWaveHaste")};
 FPlayerUpgradeRunState Legacy;
 Legacy.Levels.Add(TEXT("BladeWave"),1);Legacy.Definitions.Add(TEXT("BladeWave"),U->FindUpgradeDefinition(TEXT("BladeWave")));
 Legacy.SamuraiMastery=42;
 auto* OldReturn=DuplicateObject<UUpgradeDefinition>(U->FindUpgradeDefinition(TEXT("ReturningBlade")),U);
 OldReturn->PrerequisiteUpgradeIds.Empty();
 Legacy.Levels.Add(TEXT("ReturningBlade"),1);Legacy.Definitions.Add(TEXT("ReturningBlade"),OldReturn);
 for(FName Id:Disabled)
 {
  auto* Card=U->FindUpgradeDefinition(Id);
  TestNotNull(TEXT("Disabled card asset retained"),Card);
  TestFalse(TEXT("Disabled card cannot be acquired"),U->AcquireUpgrade(Card));
  TestFalse(TEXT("Debug acquisition cannot bypass disabled cards"),U->DebugForceAcquireUpgrade(Card));
  Legacy.Levels.Add(Id,1);Legacy.AccumulatedMagnitudes.Add(Id,.5f);Legacy.Definitions.Add(Id,Card);
 }
 U->RestoreRunState(Legacy);
 for(FName Id:Disabled)
 {
  TestFalse(TEXT("Saved disabled effects are inactive"),U->HasUpgradeId(Id));
  TestEqual(TEXT("Saved disabled scaling is inactive"),U->GetAccumulatedUpgradeMagnitude(Id),0.f);
  TestEqual(TEXT("Definition lookup also reports no active rank"),U->GetUpgradeLevel(U->FindUpgradeDefinition(Id)),0);
 }
 TestTrue(TEXT("Crescent stance remains active"),U->HasUpgradeId(TEXT("BladeWave")));
 TestEqual(TEXT("Previously disabled Returning Blade rank becomes active again"),U->GetUpgradeLevelById(TEXT("ReturningBlade")),1);
 TestEqual(TEXT("Mastery survives temporary disable"),U->GetSamuraiMasteryPoints(),42);
 FPlayerUpgradeRunState Retained;U->CaptureRunState(Retained);
 TestTrue(TEXT("Old return reference is replaced by the current saved definition"),Retained.Definitions.FindRef(TEXT("ReturningBlade"))==U->FindUpgradeDefinition(TEXT("ReturningBlade")));
 TestEqual(TEXT("Disabled acquired ranks remain stored for future reuse"),Retained.Levels.FindRef(TEXT("WideArc")),1);
 U->BeginDirectUpgradeSelection(1000);
 for(auto* Choice:U->GetCurrentUpgradeChoices()) TestFalse(TEXT("Disabled cards excluded from unrestricted rewards"),Disabled.Contains(Choice->UpgradeId));
 U->BeginDirectCategoryUpgradeSelection(EUpgradeCategory::SamuraiTrial,1000);
 for(auto* Choice:U->GetCurrentUpgradeChoices()) TestFalse(TEXT("Disabled cards excluded from repeat trials"),Disabled.Contains(Choice->UpgradeId));
 for(auto* Choice:U->GetEligibleUpgradesForCategory(EUpgradeCategory::Samurai)) TestFalse(TEXT("Disabled cards excluded from normal offers"),Disabled.Contains(Choice->UpgradeId));
 for(auto Id:{TEXT("SamuraiHeavyBlade"),TEXT("SamuraiTempo"),TEXT("SamuraiArea")}) TestFalse(TEXT("Crescent replaces generic attack stats"),U->CanAcquireUpgrade(U->FindUpgradeDefinition(Id)));
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
