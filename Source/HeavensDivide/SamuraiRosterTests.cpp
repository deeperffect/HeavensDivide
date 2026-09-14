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
 for(auto Id:{TEXT("BloodStance"),TEXT("ExecutionStance"),TEXT("WaveStance"),TEXT("BleedingEdge"),TEXT("DeepCuts"),TEXT("SamuraiHeavyBlade"),TEXT("SamuraiArea"),TEXT("DoubleCut"),TEXT("BladeWave"),TEXT("BloodTransfer"),TEXT("OverkillBurst"),TEXT("WaveMultishot")})
  TestNotNull(TEXT("Build-linked card retained"),U->FindUpgradeDefinition(Id));
 TestTrue(TEXT("Legacy Samurai trial still offers rewards"),U->BeginDirectCategoryUpgradeSelection(EUpgradeCategory::SamuraiTrial,3));
 TestEqual(TEXT("Trial uses current Samurai category"),U->GetSelectedCategory(),EUpgradeCategory::Samurai);
 for(auto* Choice:U->GetCurrentUpgradeChoices())
  TestTrue(TEXT("Trial offers eligible current build cards"),Choice&&Choice->Category==EUpgradeCategory::Samurai&&U->CanAcquireUpgrade(Choice));
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
