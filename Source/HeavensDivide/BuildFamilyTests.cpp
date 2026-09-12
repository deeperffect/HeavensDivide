#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SurvivorAbilityComponent.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "CharacterManagerComponent.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
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
 auto Card=[&](int32 Family,const TCHAR* Id){return Load(FString(TEXT("/Game/HeavensDivide/Upgrades/"))+BuildFamilies[Family].Owner+TEXT("/DA_Upgrade_")+BuildFamilies[Family].Owner+Id);};
 TArray<AEnemyBase*> Enemies;
 for(int32 i=0;i<25;++i)
 {
  auto* E=World->SpawnActor<AEnemyBase>(FVector((i%5)*160,(i/5-2)*150,0),FRotator::ZeroRotator,Params);
  E->GetHealthComponent()->SetMaxHealthPreservePercent(100000);E->GetHealthComponent()->RestoreCurrentHealth(100000);Enemies.Add(E);
 }
 for(int32 f=0;f<20;++f)
 {
  Upgrades->RestoreRunState(Empty);const auto& S=BuildFamilies[f];++Counts[FCString::Strcmp(S.Owner,TEXT("Samurai"))==0?0:1];
  auto* Starter=Card(f,S.Id);if(!Starter) continue;
  TestTrue(TEXT("Unique family ID"),!Ids.Contains(Starter->UpgradeId));Ids.Add(Starter->UpgradeId);
  auto* Scaling=Card(f,*FString(FString(S.Id)+TEXT("Power")));
  TestFalse(TEXT("Family scaling locked before starter"),Upgrades->CanAcquireUpgrade(Scaling));
  auto* Synergy=Load(FString(TEXT("/Game/HeavensDivide/Upgrades/Synergy/DA_BuildSynergy_"))+S.Synergy);
  TestFalse(TEXT("Synergy locked before family"),Upgrades->CanAcquireUpgrade(Synergy));
  TestTrue(TEXT("Acquire family"),Upgrades->AcquireUpgrade(Starter));
  for(const TCHAR* Suffix:{TEXT("Power"),TEXT("Area"),TEXT("Haste")})
  {
   const FString Id=f==4&&FCString::Strcmp(Suffix,TEXT("Area"))==0?TEXT("WideArc"):FString(S.Id)+Suffix;
   auto* Scale=Card(f,*Id);if(!Scale)continue;
   TestEqual(TEXT("Each scaling card has five ranks"),Scale->MaxLevel,5);
   TestTrue(TEXT("Scaling supports rolled rarities"),Scale->bUsesRolledRarity&&Scale->RarityMagnitudes.Num()==3);
   TestTrue(TEXT("Unique scaling ID"),!Ids.Contains(Scale->UpgradeId));Ids.Add(Scale->UpgradeId);
   TestTrue(TEXT("Acquire scaling"),Upgrades->AcquireUpgrade(Scale));Upgrades->AcquireUpgrade(Scale);
  }
  FPlayerUpgradeRunState ScaledState;Upgrades->CaptureRunState(ScaledState);
  for(int32 b=0;b<3;++b)
  {
   auto* Branch=Card(f,S.Branches[b]);if(!Branch)continue;
   TestTrue(TEXT("Unique behavior branch"),!Ids.Contains(Branch->UpgradeId));Ids.Add(Branch->UpgradeId);
   TestTrue(TEXT("All three branches can combine"),Upgrades->AcquireUpgrade(Branch));
   TestTrue(TEXT("Runtime recognizes branch asset"),Ability->Branch(f,b));
  }
  TestTrue(TEXT("Synergy unlocks with its family"),Upgrades->AcquireUpgrade(Synergy));
  TestTrue(TEXT("Unique synergy ID"),Synergy&&!Ids.Contains(Synergy->UpgradeId));if(Synergy)Ids.Add(Synergy->UpgradeId);
  // Every family has a real opposite-character reaction and no same-character trigger.
  Ability->BuildMarks.Reset();Ability->Pending.Reset();for(float& G:Ability->ReactionGates)G=0;
  Ability->RegisterFamilyHit(f,Enemies[12],20);
  const auto Own=FCString::Strcmp(S.Owner,TEXT("Samurai"))==0?EPlayerAttackSource::Samurai:EPlayerAttackSource::Ninja;
  Ability->NotifyPartnerHit(Own,Enemies[12]);
  TestTrue(TEXT("Same character cannot cash its own synergy"),Ability->BuildMarks.Num()==1&&!Ability->BuildMarks[0].bSpent);
  Ability->NotifyPartnerHit(Own==EPlayerAttackSource::Samurai?EPlayerAttackSource::Ninja:EPlayerAttackSource::Samurai,Enemies[12]);
  TestTrue(TEXT("Opposite character consumes prepared reaction"),Ability->BuildMarks.Num()==1&&Ability->BuildMarks[0].bSpent);
  const float After=Enemies[12]->GetHealthComponent()->GetCurrentHealth();const int32 Pulses=Ability->Pending.Num();
  Ability->NotifyPartnerHit(Own==EPlayerAttackSource::Samurai?EPlayerAttackSource::Ninja:EPlayerAttackSource::Samurai,Enemies[12]);
  TestEqual(TEXT("Repeated hit cannot duplicate immediate reaction"),Enemies[12]->GetHealthComponent()->GetCurrentHealth(),After);
  TestEqual(TEXT("Repeated hit cannot duplicate delayed reaction"),Ability->Pending.Num(),Pulses);
  Ability->BuildMarks.Reset();Ability->Pending.Reset();
  if(f<5)
  {
   for(auto* E:Enemies) E->GetStatusEffectComponent()->ClearAllStatuses();
   if(f<4)
   {
    Ability->ActivateAbility(f,f<2?static_cast<ACharacterBase*>(Samurai):static_cast<ACharacterBase*>(Ninja));
    if(f==0) TestTrue(TEXT("Tempest branches include remote echo and Bleed"),Ability->Pending.Num()>=2&&Enemies[12]->HasStatus(EEnemyStatusEffect::Bleed));
    if(f==1) TestTrue(TEXT("Seeking Star follows a target and Aftershock adds work"),Ability->Pending.Num()>=2&&Ability->Pending[0].FollowTarget.IsValid());
    if(f==2) TestTrue(TEXT("Venom Thread applies intrinsic Poison"),Enemies.ContainsByPredicate([](const auto* E){return E->HasStatus(EEnemyStatusEffect::Poison);}));
    if(f==3) TestTrue(TEXT("Wandering/Briar Garden combine"),Ability->Pending.Num()==1&&Ability->Pending[0].bFollowOwner&&Ability->Pending[0].bPush);
   }
   else
   {
    const float Before=Enemies[12]->GetHealthComponent()->GetCurrentHealth();Ability->BladeWaveImpact(Enemies[12],20,true);
    TestTrue(TEXT("Splinter Wave adds real Bleed damage"),Enemies[12]->GetHealthComponent()->GetCurrentHealth()<Before&&Enemies[12]->HasStatus(EEnemyStatusEffect::Bleed));
   }
   Ability->Pending.Reset();continue;
  }
  ACharacterBase* Character=Own==EPlayerAttackSource::Samurai?static_cast<ACharacterBase*>(Samurai):static_cast<ACharacterBase*>(Ninja);
  Character->SetCharacterMode(ECharacterMode::Active);Character->SetActorLocation(FVector::ZeroVector);
  FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(),Character);
  for(int32 variant=0;variant<4;++variant)
  {
   Upgrades->RestoreRunState(ScaledState);
   if(variant>0) Upgrades->AcquireUpgrade(Card(f,S.Branches[variant-1]));
   Ability->ClearBuildFamilies();Ability->Pending.Reset();
   for(float& CD:Ability->BuildCooldowns)CD=1000;
   for(float& CD:Ability->Cooldowns)CD=1000;
   for(auto* E:Enemies){E->GetHealthComponent()->RestoreCurrentHealth(100000);E->GetStatusEffectComponent()->ClearAllStatuses();E->ClearMark();}
   // Real acquisition selects each branch independently, including cast-time placement changes.
   if(S.Pattern==EBuildPattern::Trail){Ability->bTrailInitialized=true;Ability->LastTrailPosition=FVector(-500,0,0);}
   TestTrue(*FString::Printf(TEXT("%s activates"),S.Id),Ability->ActivateBuildFamily(f,Character));
   for(int32 tick=0;tick<90;++tick) Ability->UpdateBuildFamilies(Character);
   float Damage=0;for(auto* E:Enemies)Damage+=100000-E->GetHealthComponent()->GetCurrentHealth();
   TestTrue(*FString::Printf(TEXT("%s variant %d deals real damage"),S.Id,variant),Damage>0);
   TestTrue(TEXT("Scheduled ability work terminates"),Ability->BuildCasts.IsEmpty());
  }
 }
 TestEqual(TEXT("Ten Samurai families"),Counts[0],10);TestEqual(TEXT("Ten Ninja families"),Counts[1],10);TestEqual(TEXT("160 unique family cards"),Ids.Num(),160);
 // Behavioral checks beyond catalog coverage: lanes miss off-axis targets and status source restrictions remain enforced.
 Ability->ClearBuildFamilies();USurvivorAbilityComponent::FBuildCast Lane;Lane.Family=10;Lane.Damage=10;
 const float OffAxis=Enemies[0]->GetHealthComponent()->GetCurrentHealth();
 Ability->BuildLine(Lane,FVector::ZeroVector,FVector(1000,0,0),50);
 TestEqual(TEXT("Lance cannot damage off-axis enemies"),Enemies[0]->GetHealthComponent()->GetCurrentHealth(),OffAxis);
 auto Retarget=Lane;Retarget.Family=9;Retarget.Steps=6;Retarget.End=FVector::ZeroVector;
 TestFalse(TEXT("Focused flurry ends without a living target"),Ability->StepBuildCast(Retarget));
 Retarget.Branches=1;TestTrue(TEXT("Passing Sentence reacquires a living target"),Ability->StepBuildCast(Retarget));
 Ability->ClearBuildFamilies();Ability->Pending.Reset();
 auto* EndState=FindFProperty<FEnumProperty>(ASurvivorPlayerController::StaticClass(),TEXT("RunEndState"));
 EndState->GetUnderlyingProperty()->SetIntPropertyValue(EndState->ContainerPtrToValuePtr<void>(PC),static_cast<int64>(ERunEndState::Victory));
 Ability->BuildCasts.Add(Lane);Ability->UpdateAbilities();
 TestTrue(TEXT("Run end clears expanded ability work and reactions"),Ability->BuildCasts.IsEmpty()&&Ability->BuildMarks.IsEmpty());
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
