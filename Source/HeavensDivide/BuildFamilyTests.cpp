#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SurvivorAbilityComponent.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "CharacterManagerComponent.h"
#include "AutoAttackComponent.h"
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
  // Every ability family shares one baseline preparation and payout.
  Ability->BuildMarks.Reset();Ability->Pending.Reset();
  for(auto* E:Enemies) E->GetStatusEffectComponent()->ClearAllStatuses();
  Ability->RegisterFamilyHit(f,Enemies[12],20);
  const auto Own=FCString::Strcmp(S.Owner,TEXT("Samurai"))==0?EPlayerAttackSource::Samurai:EPlayerAttackSource::Ninja;
  const auto Partner=Own==EPlayerAttackSource::Samurai?EPlayerAttackSource::Ninja:EPlayerAttackSource::Samurai;
  Ability->NotifyPartnerHit(Own,Enemies[12],true);
  TestEqual(TEXT("Same source cannot consume Prepare"),Ability->BuildMarks.Num(),1);
  Ability->NotifyPartnerHit(Partner,Enemies[12]);
  TestEqual(TEXT("Active partner normal/ability hits cannot consume Prepare"),Ability->BuildMarks.Num(),1);
  const float BeforePrepare=Enemies[12]->GetHealthComponent()->GetCurrentHealth();
  Ability->NotifyPartnerHit(Partner,Enemies[12],true);
  TestTrue(TEXT("Partner assist consumes universal Prepare"),Ability->BuildMarks.IsEmpty());
  TestTrue(TEXT("All families use the same bonus damage"),FMath::IsNearlyEqual(BeforePrepare-Enemies[12]->GetHealthComponent()->GetCurrentHealth(),12,0.03f));
  Ability->NotifyPartnerHit(Partner,Enemies[12],true);
  TestTrue(TEXT("Consumption cannot pay twice"),FMath::IsNearlyEqual(BeforePrepare-Enemies[12]->GetHealthComponent()->GetCurrentHealth(),12,0.03f));
  for(auto* E:Enemies) TestFalse(TEXT("Prepare invents no Bleed or Poison"),E->HasStatus(EEnemyStatusEffect::Bleed)||E->HasStatus(EEnemyStatusEffect::Poison));
  TestTrue(TEXT("Normal hits never prepare enemies"),Ability->BuildMarks.IsEmpty());
  Ability->Pending.Reset();
  const float Before=Enemies[12]->GetHealthComponent()->GetCurrentHealth();Ability->BladeWaveImpact(Enemies[12],20,true);
  TestTrue(TEXT("Splinter Wave adds real Bleed damage"),Enemies[12]->GetHealthComponent()->GetCurrentHealth()<Before&&Enemies[12]->HasStatus(EEnemyStatusEffect::Bleed));
 }
 TestEqual(TEXT("One Samurai family"),Counts[0],1);TestEqual(TEXT("No Ninja ability families"),Counts[1],0);TestEqual(TEXT("Seven Blade Wave cards"),Ids.Num(),7);
 // Shared preparation, expiration, assist selection and status spread.
 Upgrades->RestoreRunState(Empty);Ability->ClearBuildFamilies();Ability->Pending.Reset();
 for(auto* E:Enemies){E->GetStatusEffectComponent()->ClearAllStatuses();E->GetHealthComponent()->RestoreCurrentHealth(100000);}
 Ability->RegisterFamilyHit(4,Enemies[24],20);
 Ability->RegisterFamilyHit(4,Enemies[24],30);
 TestEqual(TEXT("Multiple abilities share one preparation per enemy"),Ability->BuildMarks.Num(),1);
 TestEqual(TEXT("Latest same-source ability refreshes damage snapshot"),Ability->BuildMarks[0].Damage,30.f);
 Ability->RegisterFamilyHit(2,Enemies[24],40);
 TestEqual(TEXT("Retired abilities cannot overwrite Prepare"),Ability->BuildMarks[0].Source,EPlayerAttackSource::Samurai);
 TArray<AEnemyBase*> Targets={Enemies[10],Enemies[12],Enemies[24]};
 Ability->PrioritizePreparedTargets(EPlayerAttackSource::Ninja,Targets);
 TestTrue(TEXT("Assist prioritizes prepared targets and preserves fallback order"),Targets[0]==Enemies[24]&&Targets[1]==Enemies[10]&&Targets[2]==Enemies[12]);
 Ability->BuildMarks[0].Remaining=0;
 Ability->NotifyPartnerHit(EPlayerAttackSource::Ninja,Enemies[24],true);
 TestEqual(TEXT("Expired preparation deals no damage"),Enemies[24]->GetHealthComponent()->GetCurrentHealth(),100000.f);
 TestFalse(TEXT("Expired preparation is not prioritized"),Ability->HasTriggerablePreparation(EPlayerAttackSource::Ninja,Enemies[24]));
 Targets={Enemies[10],Enemies[12],Enemies[24]};Ability->PrioritizePreparedTargets(EPlayerAttackSource::Ninja,Targets);
 TestTrue(TEXT("No valid preparation preserves normal target order"),Targets[0]==Enemies[10]&&Targets[2]==Enemies[24]);
 Ability->RegisterFamilyHit(4,Enemies[24],20);
 auto* Attack=Ninja->FindComponentByClass<UAutoAttackComponent>();
 if(TestNotNull(TEXT("Ninja assist component"),Attack))
 {
  FindFProperty<FObjectProperty>(UAutoAttackComponent::StaticClass(),TEXT("OwnerCharacter"))->SetObjectPropertyValue_InContainer(Attack,Ninja);
  TestEqual(TEXT("Assist chooses prepared victim"),Attack->FindAssistTargetNearLocation(FVector::ZeroVector,1000),Enemies[24]);
  TestTrue(TEXT("Assist selection respects range"),Attack->FindAssistTargetNearLocation(FVector::ZeroVector,100)!=Enemies[24]);
 }
 Ninja->SetCharacterMode(ECharacterMode::Assisting);Ninja->SetVisualFacingRotation(FRotator::ZeroRotator);
 TestTrue(TEXT("Assist executes with prepared victim beyond nearest five"),Ability->ExecuteSetupAssist(Ninja));
 TestTrue(TEXT("Limited assist hit list includes prepared victim"),Ability->BuildMarks.IsEmpty());
 for(auto* E:Enemies) E->GetStatusEffectComponent()->ClearAllStatuses();
 auto* Status=Enemies[12]->GetStatusEffectComponent();
 Status->ApplyStatus(EEnemyStatusEffect::Bleed,Upgrades,EPlayerAttackSource::Samurai,true);
 Status->ApplyStatus(EEnemyStatusEffect::Poison,Upgrades,EPlayerAttackSource::Ninja,true);
 Enemies[11]->ConfigureObjectiveEnemy(100000,EPlayerAttackSource::Ninja,nullptr,FLinearColor::White);
 Ability->RegisterFamilyHit(4,Enemies[12],20);Ability->RegisterFamilyHit(4,Enemies[13],20);
 const float NearbyHealth=Enemies[13]->GetHealthComponent()->GetCurrentHealth();
 Ability->NotifyPartnerHit(EPlayerAttackSource::Ninja,Enemies[12],true);
 TestEqual(TEXT("Prepare spreads existing Bleed"),Enemies[13]->GetStatusEffectComponent()->GetStatusStacks(EEnemyStatusEffect::Bleed),1);
 TestEqual(TEXT("Prepare spreads existing Poison"),Enemies[13]->GetStatusEffectComponent()->GetStatusStacks(EEnemyStatusEffect::Poison),1);
 TestFalse(TEXT("Spread respects Bleed source restriction"),Enemies[11]->HasStatus(EEnemyStatusEffect::Bleed));
 TestTrue(TEXT("Spread permits matching Poison source"),Enemies[11]->HasStatus(EEnemyStatusEffect::Poison));
 TestEqual(TEXT("Bonus damage is single-target"),Enemies[13]->GetHealthComponent()->GetCurrentHealth(),NearbyHealth);
 TestTrue(TEXT("Spread does not recursively consume nearby Prepare"),Ability->HasTriggerablePreparation(EPlayerAttackSource::Ninja,Enemies[13]));
 TestFalse(TEXT("Spread respects radius"),Enemies[0]->HasStatus(EEnemyStatusEffect::Bleed)||Enemies[0]->HasStatus(EEnemyStatusEffect::Poison));
 TestEqual(TEXT("Spread retains source victim statuses"),Status->GetStatusStacks(EEnemyStatusEffect::Bleed),1);
 Ability->ClearBuildFamilies();
 for(auto* E:Enemies) E->GetStatusEffectComponent()->ClearAllStatuses();
 Enemies[24]->GetStatusEffectComponent()->ApplyStatus(EEnemyStatusEffect::Bleed,Upgrades,EPlayerAttackSource::Samurai,true);
 Ability->RegisterFamilyHit(4,Enemies[24],20);
 Enemies[24]->ConfigureObjectiveEnemy(100000,EPlayerAttackSource::Other,nullptr,FLinearColor::White);
 FScriptDelegate DeathHandler;DeathHandler.BindUFunction(Enemies[24],TEXT("HandleDeath"));
 Enemies[24]->GetHealthComponent()->OnDeath.AddUnique(DeathHandler);
 Enemies[24]->GetHealthComponent()->RestoreCurrentHealth(1);
 Ability->ExecuteSetupAssist(Ninja);
 TestTrue(TEXT("Lethal assist still consumes Prepare"),Ability->BuildMarks.IsEmpty()&&Enemies[24]->IsDead());
 TestTrue(TEXT("Lethal assist spreads pre-hit status snapshot"),Enemies[23]->HasStatus(EEnemyStatusEffect::Bleed));
 TestFalse(TEXT("Absent Poison is not invented by Prepare"),Enemies[23]->HasStatus(EEnemyStatusEffect::Poison));
 Ninja->SetCharacterMode(ECharacterMode::Inactive);
 USurvivorAbilityComponent::FBuildCast Lane;Lane.Family=4;
 Ability->ClearBuildFamilies();Ability->Pending.Reset();
 auto* EndState=FindFProperty<FEnumProperty>(ASurvivorPlayerController::StaticClass(),TEXT("RunEndState"));
 EndState->GetUnderlyingProperty()->SetIntPropertyValue(EndState->ContainerPtrToValuePtr<void>(PC),static_cast<int64>(ERunEndState::Victory));
 Ability->BuildCasts.Add(Lane);Ability->UpdateAbilities();
 TestTrue(TEXT("Run end clears expanded ability work and reactions"),Ability->BuildCasts.IsEmpty()&&Ability->BuildMarks.IsEmpty());
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
