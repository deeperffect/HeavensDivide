#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SurvivorAbilityComponent.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "CharacterManagerComponent.h"
#include "AutoAttackComponent.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "NinjaBuildComponent.h"
#include "EnemyStatusEffectComponent.h"
#include "HealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildFamiliesTest,"HeavensDivide.Abilities.BuildFamilies",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildFamiliesTest::RunTest(const FString&)
{
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 const auto Class=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 auto* PC=World->SpawnActor<ASurvivorPlayerController>(Class);
 if(!TestNotNull(TEXT("Saved controller"),PC)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 auto* Ability=PC->FindComponentByClass<USurvivorAbilityComponent>();auto* Upgrades=PC->GetPlayerUpgrades();
 Ability->Controller=PC;Ability->Upgrades=Upgrades;PC->GetPlayerHealthComponent()->RestoreCurrentHealth(100);
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Samurai=World->SpawnActor<ASamuraiCharacter>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
 auto* Ninja=World->SpawnActor<ANinjaCharacter>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
 Samurai->SetOwner(PC);Ninja->SetOwner(PC);
 FPlayerUpgradeRunState Empty;Upgrades->CaptureRunState(Empty);
 TSet<FName> Ids;int32 Counts[2]={};
 auto Load=[&](const FString& Path){auto* U=LoadObject<UUpgradeDefinition>(nullptr,*Path);TestNotNull(*Path,U);return U;};
 auto Card=[&](int32 Family,const TCHAR* Id){return Load(FString(TEXT("/Game/HeavensDivide/Upgrades/"))+BuildFamilies[Family].Owner+TEXT("/DA_Upgrade_")+BuildFamilies[Family].Owner+(FCString::Strcmp(Id,TEXT("BladeWave"))==0?TEXT("CrescentStance"):Id));};
 // Saved soft references must still resolve after the stance DA packages and objects are renamed.
 for (const TCHAR* Id : {TEXT("BattleStance"), TEXT("Iaijutsu"), TEXT("BladeWave"), TEXT("ReturningFang"), TEXT("BarrageStance"), TEXT("GreatShuriken")})
 {
  auto* Stance = Upgrades->FindUpgradeDefinition(FName(Id));
  if (TestNotNull(TEXT("Saved stance definition"), Stance))
   TestTrue(TEXT("Every stance DA includes Stance in its name"), Stance->GetName().Contains(TEXT("Stance")));
 }
 for (const TCHAR* Id : {TEXT("BladeWave"), TEXT("Iaijutsu"), TEXT("ReturningFang"), TEXT("GreatShuriken")})
 {
  const FString Owner = FCString::Strcmp(Id,TEXT("BladeWave"))==0 || FCString::Strcmp(Id,TEXT("Iaijutsu"))==0 ? TEXT("Samurai") : TEXT("Ninja");
  const FString Name = TEXT("DA_Upgrade_") + Owner + Id;
  const FSoftObjectPath LegacyPath(TEXT("/Game/HeavensDivide/Upgrades/") + Owner + TEXT("/") + Name + TEXT(".") + Name);
  TestTrue(TEXT("Old stance reference resolves to the renamed pool asset"), LegacyPath.TryLoad() == Upgrades->FindUpgradeDefinition(FName(Id)));
 }
 TArray<AEnemyBase*> Enemies;
 for(int32 i=0;i<25;++i)
 {
  auto* E=World->SpawnActor<AEnemyBase>(FVector((i%5)*160,(i/5-2)*150,0),FRotator::ZeroRotator,Params);
  E->GetHealthComponent()->SetMaxHealthPreservePercent(100000);E->GetHealthComponent()->RestoreCurrentHealth(100000);Enemies.Add(E);
 }
 for(int32 f=0;f<BuildFamilyCount;++f)
 {
  if(!BuildFamilies[f].Available)continue;
  Upgrades->RestoreRunState(Empty);const auto& S=BuildFamilies[f];++Counts[FCString::Strcmp(S.Owner,TEXT("Samurai"))==0?0:1];
  auto* Starter=Card(f,S.Id);if(!Starter) continue;
  TestTrue(TEXT("Unique family ID"),!Ids.Contains(Starter->UpgradeId));Ids.Add(Starter->UpgradeId);
  auto* Scaling=Card(f,*FString(FString(S.Id)+TEXT("Power")));
  TestFalse(TEXT("Family scaling locked before starter"),Upgrades->CanAcquireUpgrade(Scaling));
  TestTrue(TEXT("Acquire family"),Upgrades->AcquireUpgrade(Starter));
  for(const TCHAR* Suffix:{TEXT("Power"),TEXT("Area"),TEXT("Haste")})
  {
   const FString Id=f==4&&FCString::Strcmp(Suffix,TEXT("Area"))==0?TEXT("WideArc"):FString(S.Id)+Suffix;
   auto* Scale=Card(f,*Id);if(!Scale)continue;
   TestEqual(TEXT("Each scaling card has five ranks"),Scale->MaxLevel,5);
   TestTrue(TEXT("Scaling supports rolled rarities"),Scale->bUsesRolledRarity&&Scale->RarityMagnitudes.Num()==3);
   TestTrue(TEXT("Unique scaling ID"),!Ids.Contains(Scale->UpgradeId));Ids.Add(Scale->UpgradeId);
   TestFalse(TEXT("Saved scaling is temporarily disabled"),Upgrades->AcquireUpgrade(Scale));
  }
  FPlayerUpgradeRunState ScaledState;Upgrades->CaptureRunState(ScaledState);
  for(int32 b=0;b<3;++b)
  {
   auto* Branch=Card(f,S.Branches[b]);if(!Branch)continue;
   TestTrue(TEXT("Unique behavior branch"),!Ids.Contains(Branch->UpgradeId));Ids.Add(Branch->UpgradeId);
   const bool bReturning = f==4 && b==0;
   TestEqual(TEXT("Only Returning Blade is enabled among legacy wave branches"),Upgrades->AcquireUpgrade(Branch),bReturning);
   TestEqual(TEXT("Returning Blade activation preserves the stable family branch"),Ability->Branch(f,b),bReturning);
  }
  const float Before=Enemies[12]->GetHealthComponent()->GetCurrentHealth();Ability->BladeWaveImpact(Enemies[12],20,true);
  TestEqual(TEXT("Disabled Splinter cannot deal burst damage"),Enemies[12]->GetHealthComponent()->GetCurrentHealth(),Before);
  TestFalse(TEXT("Crescent Splinter cannot apply Bleed"),Enemies[12]->HasStatus(EEnemyStatusEffect::Bleed));
 }
 TestEqual(TEXT("One Samurai family"),Counts[0],1);TestEqual(TEXT("No Ninja ability families"),Counts[1],0);TestEqual(TEXT("Seven Blade Wave cards"),Ids.Num(),7);
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
