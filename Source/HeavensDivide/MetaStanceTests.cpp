#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "MetaSkillTree.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "CharacterManagerComponent.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "AutoAttackComponent.h"
#include "AttackProjectileBase.h"
#include "NinjaBuildComponent.h"
#include "IaijutsuBuild.h"
#include "ShurikenBuild.h"
#include "SharedPlayerStatsComponent.h"
#include "BarragePoisonPool.h"
#include "EnemyBase.h"
#include "EnemyStatusEffectComponent.h"
#include "HealthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "HeavensDivideMetaSaveGame.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaStanceTest,"HeavensDivide.Meta.StanceBonuses",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMetaStanceTest::RunTest(const FString&)
{
 const FString Slot=TEXT("Automation_MetaStances_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
 const FString PreviousCommandLine=FCommandLine::Get();
 FCommandLine::Set(*(TEXT("-MetaSaveSlot=")+Slot+TEXT(" ")+PreviousCommandLine));
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();
 FCommandLine::Set(*PreviousCommandLine);
 auto* World=GI->GetWorld();
 auto* Meta=GI->GetSubsystem<USynergyMetaProgressionSubsystem>();
 Meta->TestSaveSlot=Slot;Meta->CreateFreshSave();
 World->InitializeActorsForPlay(FURL());
 auto* PC=World->SpawnActor<ASurvivorPlayerController>();
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Samurai=World->SpawnActor<ASamuraiCharacter>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
 auto* Ninja=World->SpawnActor<ANinjaCharacter>(FVector(0,300,0),FRotator::ZeroRotator,Params);
 Samurai->SetOwner(PC);Ninja->SetOwner(PC);
 auto* Party=PC->GetCharacterManager();
 FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("SamuraiCharacter"))->SetObjectPropertyValue_InContainer(Party,Samurai);
 FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("NinjaCharacter"))->SetObjectPropertyValue_InContainer(Party,Ninja);
 FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(Party,Samurai);
 auto* U=PC->GetPlayerUpgrades();auto* SS=Samurai->GetCharacterStats();auto* NS=Ninja->GetCharacterStats();
 auto* Attack=Ninja->FindComponentByClass<UAutoAttackComponent>();
 auto* Build=Ninja->FindComponentByClass<UNinjaBuildComponent>();
 auto* SteelAttack=Samurai->FindComponentByClass<UAutoAttackComponent>();
 FindFProperty<FObjectProperty>(UAutoAttackComponent::StaticClass(),TEXT("OwnerCharacter"))->SetObjectPropertyValue_InContainer(SteelAttack,Samurai);
 FindFProperty<FObjectProperty>(UAutoAttackComponent::StaticClass(),TEXT("OwnerCharacter"))->SetObjectPropertyValue_InContainer(Attack,Ninja);
 FindFProperty<FClassProperty>(UAutoAttackComponent::StaticClass(),TEXT("ProjectileClass"))->SetObjectPropertyValue_InContainer(Attack,AAttackProjectileBase::StaticClass());
 const FName Stances[]={TEXT("BattleStance"),TEXT("Iaijutsu"),TEXT("BladeWave"),TEXT("ReturningFang"),TEXT("BarrageStance"),TEXT("GreatShuriken")};
 const FName Passives[]={TEXT("Bleed"),TEXT("IaijutsuCharge"),NAME_None,TEXT("FangRange"),NAME_None,TEXT("ShurikenTempo")};
 for(int32 I=0;I<6;++I)
 {
  const FName Stance=Stances[I];
  FPlayerUpgradeRunState State;
  auto* Card=NewObject<UUpgradeDefinition>(U);Card->UpgradeId=Stance;Card->MaxLevel=1;
  // Runtime mechanic lookups search the catalog, not the restored definition map.
  FindFProperty<FArrayProperty>(UPlayerUpgradeComponent::StaticClass(),TEXT("UpgradePool"))
   ->ContainerPtrToValuePtr<TArray<TObjectPtr<UUpgradeDefinition>>>(U)->Add(Card);
  TestEqual(TEXT("Fixture stance is available to mechanic lookups"),U->FindUpgradeDefinition(Stance),Card);
  State.Levels.Add(Stance,1);State.Definitions.Add(Stance,Card);
  U->BalancePreviewMetaProfile=0;U->RestoreRunState(State);
  const float Charge=IaijutsuBuild::Charge(U);
  const float Range=Attack->GetEffectiveTargetingRange();
  const float SteelInterval=SteelAttack->GetEffectiveAttackInterval(),ShadowInterval=Attack->GetEffectiveAttackInterval();
  if(I==1)TestEqual(TEXT("Iaijutsu fixture exercises its authored recovery"),SteelInterval-Charge,.5f,.000001f);
  const float FlightSpeed=Attack->GetEffectiveProjectileSpeed();
  const float Contact=ShurikenBuild::ContactInterval(U),Pulse=ShurikenBuild::PulseInterval(U);
  float BladeRadius=0;
  if(I==5)if(auto* Blade=Build->SpawnShuriken(FVector(0,0,100),FVector::ForwardVector,false)){BladeRadius=Blade->Radius;Blade->Destroy();}
  U->BalancePreviewMetaProfile=1;U->RebuildAllUpgradeModifiers();
  TestEqual(*FString::Printf(TEXT("%s: only chosen Samurai specialization adds damage"),*Stance.ToString()),SS->GetFinalDamageMultiplier(),I<3?1.33f:1.24f,.0001f);
  TestEqual(*FString::Printf(TEXT("%s: only chosen Ninja specialization adds damage"),*Stance.ToString()),NS->GetFinalDamageMultiplier(),I>=3?1.33f:1.24f,.0001f);
  // Accumulating rank magnitudes can differ from decimal literals by one float ULP.
  TestEqual(*FString::Printf(TEXT("%s: melee/wave width stays within its Samurai stance"),*Stance.ToString()),SS->GetFinalAttackAreaMultiplier(),I<3?1.32f:1.2f,.000001f);
  TestEqual(TEXT("Only Barrage receives the extra projectile"),NS->GetFinalProjectileCount(),I==4?2:1);
  TestEqual(TEXT("No stance inherits obsolete pierce"),NS->GetFinalProjectilePierceBonus(),0);
  TestEqual(*FString::Printf(TEXT("%s: only Fang receives flight speed"),*Stance.ToString()),NS->GetFinalProjectileSpeedMultiplier(),I==3?1.43f:1.f,.000001f);
  for(int32 J=0;J<6;++J)if(!Passives[J].IsNone())
   TestTrue(TEXT("Mechanic passives require their matching stance"),I==J?U->GetMetaSkillBonus(Passives[J])>0:U->GetMetaSkillBonus(Passives[J])==0);
  TestEqual(TEXT("Permanent cadence is not also applied globally"),PC->GetSharedPlayerStats()->GetFinalAttackSpeedMultiplier(),1.f);
  TestEqual(TEXT("Samurai cadence converts only for the chosen mechanic"),SS->GetFinalAttackSpeedMultiplier(),I==0?1.31f:I==2?1.43f:1.f,.0001f);
  TestEqual(TEXT("Ninja basic cadence only applies to Barrage"),NS->GetFinalAttackSpeedMultiplier(),I==4?1.4f:1.f,.0001f);
  if(I==0 || I==2)TestTrue(TEXT("Blood and Crescent actual attack intervals shorten once"),FMath::IsNearlyEqual(SteelAttack->GetEffectiveAttackInterval(),SteelInterval/(I==0?1.31f:1.43f),.0001f));
  if(I==1)
  {
   TestTrue(TEXT("Iaijutsu converts cadence plus focus into charge speed"),FMath::IsNearlyEqual(IaijutsuBuild::Charge(U),Charge/1.43f));
   TestEqual(TEXT("Iaijutsu recovery is not sped up a second time"),SteelAttack->GetEffectiveAttackInterval()-IaijutsuBuild::Charge(U),SteelInterval-Charge,.000001f);
  }
  if(I==3)
  {
   TestTrue(TEXT("Fang coverage reaches actual targeting"),FMath::IsNearlyEqual(Attack->GetEffectiveTargetingRange(),Range*1.32f,.01f));
   TestTrue(TEXT("Fang cadence reaches actual flight speed"),FMath::IsNearlyEqual(Attack->GetEffectiveProjectileSpeed(),FlightSpeed*1.43f,.01f));
   TestTrue(TEXT("Fang cadence is not also granted as attack rate"),FMath::IsNearlyEqual(Attack->GetEffectiveAttackInterval(),ShadowInterval));
  }
  if(I==4)
  {
   TestTrue(TEXT("Barrage actual attack interval shortens once"),FMath::IsNearlyEqual(Attack->GetEffectiveAttackInterval(),ShadowInterval/1.4f));
   auto* Projectile=World->SpawnActor<AAttackProjectileBase>(FVector(5000,0,0),FRotator::ZeroRotator,Params);
   auto* Collision=Projectile->FindComponentByClass<USphereComponent>();
   const float OriginalRadius=Collision->GetScaledSphereRadius();
   for(int32 Repeat=0;Repeat<2;++Repeat)
   {
    Projectile->InitializeProjectile(Ninja,FVector::ForwardVector,10,1000,EProjectileTargetType::Enemies,1000);
    TestTrue(TEXT("Barrage width increases once, including reinitialization"),FMath::IsNearlyEqual(Collision->GetScaledSphereRadius(),OriginalRadius*1.2f,.001f));
   }
   Projectile->Destroy();
   auto* EdgeEnemy=World->SpawnActor<AEnemyBase>(FVector(5240,0,60),FRotator::ZeroRotator,Params);
   EdgeEnemy->GetCapsuleComponent()->SetCapsuleSize(1,1);
   EdgeEnemy->ConfigureObjectiveEnemy(1000,EPlayerAttackSource::Other,nullptr,FLinearColor::White);
   EdgeEnemy->GetHealthComponent()->RestoreCurrentHealth(1000);
   U->BalancePreviewMetaProfile=0;
   auto* BasePool=World->SpawnActor<ABarragePoisonPool>(FVector(5000,0,85),FRotator::ZeroRotator,Params);BasePool->Initialize(U,100);BasePool->Pulse();
   TestEqual(TEXT("Standard poison pool does not reach outer target"),EdgeEnemy->GetStatusEffectComponent()->GetStatusStacks(EEnemyStatusEffect::Poison),0);
   BasePool->Destroy();
   U->BalancePreviewMetaProfile=1;
   auto* WidePool=World->SpawnActor<ABarragePoisonPool>(FVector(5000,0,85),FRotator::ZeroRotator,Params);WidePool->Initialize(U,100);WidePool->Pulse();
   TestEqual(TEXT("Purchased coverage extends poison-pool gameplay reach"),EdgeEnemy->GetStatusEffectComponent()->GetStatusStacks(EEnemyStatusEffect::Poison),1);
   WidePool->Destroy();EdgeEnemy->Destroy();
  }
  if(I==5)
  {
   TestTrue(TEXT("Wheel tempo changes actual contact cadence"),FMath::IsNearlyEqual(ShurikenBuild::ContactInterval(U),Contact/1.43f));
   TestTrue(TEXT("Wheel tempo changes actual pulse cadence"),FMath::IsNearlyEqual(ShurikenBuild::PulseInterval(U),Pulse/1.43f));
   auto* Blade=Build->SpawnShuriken(FVector(0,0,100),FVector::ForwardVector,false);
   if(TestNotNull(TEXT("Permanent size reaches a spawned shuriken"),Blade))
   {
    TestTrue(TEXT("Blade collision grows by the purchased size bonus"),BladeRadius>0 && FMath::IsNearlyEqual(Blade->Radius,BladeRadius*1.32f,.01f));
    Blade->UpdateShurikenGrowth();
    TestTrue(TEXT("Growth updates retain the permanent size"),FMath::IsNearlyEqual(Blade->Radius,BladeRadius*1.32f,.01f));
    Blade->Destroy();
   }
  }
  const float SteelCadence=SS->GetFinalAttackSpeedMultiplier(),ShadowCadence=NS->GetFinalAttackSpeedMultiplier();
  const float Size=SS->GetFinalAttackAreaMultiplier(),Flight=NS->GetFinalProjectileSpeedMultiplier();
  U->RebuildAllUpgradeModifiers();
  TestEqual(TEXT("Repeated rebuild keeps Samurai cadence"),SS->GetFinalAttackSpeedMultiplier(),SteelCadence);
  TestEqual(TEXT("Repeated rebuild keeps Ninja cadence"),NS->GetFinalAttackSpeedMultiplier(),ShadowCadence);
  TestEqual(TEXT("Repeated rebuild keeps Samurai coverage"),SS->GetFinalAttackAreaMultiplier(),Size);
  TestEqual(TEXT("Repeated rebuild keeps Fang flight"),NS->GetFinalProjectileSpeedMultiplier(),Flight);
  TestEqual(TEXT("Repeated rebuilds do not stack specialization damage"),NS->GetFinalDamageMultiplier(),I>=3?1.33f:1.24f,.0001f);
 }
 // Both heroes keep the bonus of their own stance, including the inactive assistant.
 FPlayerUpgradeRunState Pair;
 for(FName Id:{FName(TEXT("Iaijutsu")),FName(TEXT("BarrageStance"))})
 {
  auto* Card=NewObject<UUpgradeDefinition>(U);Card->UpgradeId=Id;Pair.Levels.Add(Id,1);Pair.Definitions.Add(Id,Card);
 }
 U->RestoreRunState(Pair);
 TestEqual(TEXT("Samurai keeps its own stance bonus"),SS->GetFinalDamageMultiplier(),1.33f,.0001f);
 TestEqual(TEXT("Inactive Ninja keeps its own stance bonus"),NS->GetFinalDamageMultiplier(),1.33f,.0001f);
 TestEqual(TEXT("Inactive Barrage assistant receives its projectile"),NS->GetFinalProjectileCount(),2);
 TestEqual(TEXT("Inactive Barrage assistant retains its cadence"),NS->GetFinalAttackSpeedMultiplier(),1.4f,.0001f);
 TestEqual(TEXT("Active Iaijutsu does not receive Barrage cadence"),SS->GetFinalAttackSpeedMultiplier(),1.f);
 TestEqual(TEXT("Iaijutsu retains its own charge conversion"),U->GetMetaSkillBonus(TEXT("IaijutsuCharge")),.43f,.0001f);
 U->RestoreRunState(FPlayerUpgradeRunState());
 TestEqual(TEXT("Changing build clears old stance count"),NS->GetFinalProjectileCount(),1);
 TestTrue(TEXT("Changing build clears old stance damage"),FMath::IsNearlyEqual(NS->GetFinalDamageMultiplier(),1.24f));
 U->BalancePreviewMetaProfile=0;U->RebuildAllUpgradeModifiers();
 TestEqual(TEXT("Fresh profile clears permanent damage"),NS->GetFinalDamageMultiplier(),1.f);
 TestEqual(TEXT("Fresh profile clears permanent tempo"),NS->GetFinalAttackSpeedMultiplier(),1.f);
 for(const auto& N:MetaSkillTree::Nodes())
 {
  TestFalse(TEXT("Retired universal Poison and pierce absent"),N.Effect==TEXT("Poison")||N.Effect==TEXT("Pierce"));
  if(N.CharacterStat==ECharacterStatType::ProjectileCountBonus && !N.bPassiveEffect)
   TestEqual(TEXT("Projectile skills explicitly belong to Barrage"),N.RequiredStance,FName(TEXT("BarrageStance")));
 }
 // An old save may retain ranks for a stance that is now locked. They remain dormant.
 U->BalancePreviewMetaProfile=-1;
 Meta->CurrentSave->SkillRanks.Add(TEXT("Iaijutsu.Edge"),3);
 Meta->CurrentSave->SkillRanks.Add(TEXT("Iaijutsu.Width"),3);
 Meta->CurrentSave->SkillRanks.Add(TEXT("Iaijutsu.Focus"),1);
 Meta->RefreshSkillBonuses();U->RestoreRunState(Pair);
 TestEqual(TEXT("Locked legacy stance cannot add damage"),SS->GetFinalDamageMultiplier(),1.f);
 TestEqual(TEXT("Locked legacy stance cannot add area"),SS->GetFinalAttackAreaMultiplier(),1.f);
 TestEqual(TEXT("Locked legacy stance cannot add charge speed"),U->GetMetaSkillBonus(TEXT("IaijutsuCharge")),0.f);
 Meta->CurrentSave->SoulSigils=1000;
 TestFalse(TEXT("Unlock purchases are blocked while combat controller is present"),Meta->PurchaseStance(TEXT("Iaijutsu")));
 // Leave the fixture's combat-controller registry to model returning to the menu.
 World->RemoveController(PC);
 TestTrue(TEXT("Purchase activates legacy stance ranks"),Meta->PurchaseStance(TEXT("Iaijutsu")));
 World->AddController(PC);
 U->RebuildAllUpgradeModifiers();
 TestEqual(TEXT("Purchased stance activates its stored damage"),SS->GetFinalDamageMultiplier(),1.09f,.0001f);
 TestEqual(TEXT("Purchased stance activates its stored width"),SS->GetFinalAttackAreaMultiplier(),1.12f,.0001f);
 TestEqual(TEXT("Purchased stance activates its stored charge speed"),U->GetMetaSkillBonus(TEXT("IaijutsuCharge")),.12f,.0001f);
 Build->ClearProjectiles();World->GetTimerManager().ClearAllTimersForObject(PC);
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GI->Shutdown();
 UGameplayStatics::DeleteGameInSlot(Slot,0);
 return true;
}
#endif
