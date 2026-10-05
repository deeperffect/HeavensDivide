#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "NinjaBuildComponent.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "InactiveCharacterAssistComponent.h"
#include "AutoAttackComponent.h"
#include "AttackProjectileBase.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "EnemyBase.h"
#include "HealthComponent.h"
#include "FangBuild.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFangBuildsTest,"HeavensDivide.Combat.FangBuilds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFangBuildsTest::RunTest(const FString&)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 auto* Class=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 auto* PC=World->SpawnActor<ASurvivorPlayerController>(Class);
 if(!TestNotNull(TEXT("Saved controller"),PC)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 PC->GetPlayerHealthComponent()->RestoreCurrentHealth(100);
 auto* Manager=PC->GetCharacterManager();
 auto* NinjaClass=Cast<UClass>(FindFProperty<FClassProperty>(UCharacterManagerComponent::StaticClass(),TEXT("NinjaClass"))->GetObjectPropertyValue_InContainer(Manager));
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* N=World->SpawnActor<ANinjaCharacter>(NinjaClass,FVector::ZeroVector,FRotator::ZeroRotator,Params);
 N->SetOwner(PC);N->SetCharacterMode(ECharacterMode::Active);
 FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("NinjaCharacter"))->SetObjectPropertyValue_InContainer(Manager,N);
 FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(Manager,N);
 auto* B=N->FindComponentByClass<UNinjaBuildComponent>();auto* A=N->FindComponentByClass<UAutoAttackComponent>();auto* U=PC->GetPlayerUpgrades();
 A->OwnerCharacter=N;A->ProjectileClass=AAttackProjectileBase::StaticClass();
 FPlayerUpgradeRunState Empty;U->CaptureRunState(Empty);
 auto Grant=[&](FName Id){auto* Card=U->FindUpgradeDefinition(Id);TestNotNull(*Id.ToString(),Card);return U->AcquireUpgrade(Card);};
 TArray<AEnemyBase*> Enemies;
 auto Spawn=[&](FVector Pos,float HP){auto* E=World->SpawnActor<AEnemyBase>(Pos,FRotator::ZeroRotator,Params);E->ConfigureObjectiveEnemy(HP,EPlayerAttackSource::Other,nullptr,FLinearColor::White);E->GetHealthComponent()->RestoreCurrentHealth(HP);FScriptDelegate D;D.BindUFunction(E,TEXT("HandleDeath"));E->GetHealthComponent()->OnDeath.AddUnique(D);Enemies.Add(E);return E;};
 auto Reset=[&](){B->ClearProjectiles();for(auto* E:Enemies)if(IsValid(E))E->Destroy();Enemies.Reset();U->RestoreRunState(Empty);Grant(TEXT("ReturningFang"));};
 const FName OneTime[]={TEXT("FangTwin"),TEXT("RelentlessFang"),TEXT("FangResonance"),TEXT("FangDeadeye"),TEXT("FangAssist"),TEXT("FangSplinter")};
 const FName Rare[]={TEXT("FangTwinFrequency"),TEXT("FangPressure"),TEXT("FangResonantReach"),TEXT("FangCriticalChance"),TEXT("FangAssistChance"),TEXT("FangSplinterPower")};
 for(int32 i=0;i<6;++i)
 {
  TestFalse(TEXT("Unlock unavailable before stance"),U->CanAcquireUpgrade(U->FindUpgradeDefinition(OneTime[i])));
  TestFalse(TEXT("Rare unavailable before unlock"),U->CanAcquireUpgrade(U->FindUpgradeDefinition(Rare[i])));
 }
 Grant(TEXT("NinjaDamage"));Grant(TEXT("NinjaSpeed"));Grant(TEXT("NinjaCoverage"));
 FPlayerUpgradeRunState Basic;U->CaptureRunState(Basic);Basic.AccumulatedMagnitudes[TEXT("NinjaDamage")]=.4f;U->RestoreRunState(Basic);
 const int32 Mastery=U->GetNinjaMasteryPoints();Grant(TEXT("ReturningFang"));
 TestEqual(TEXT("Damage rank converts"),U->GetUpgradeLevelById(TEXT("FangDamage")),1);
 TestEqual(TEXT("Rarity strength preserved"),U->GetAccumulatedUpgradeMagnitude(TEXT("FangDamage")),.4f);
 TestEqual(TEXT("Speed rank converts"),U->GetUpgradeLevelById(TEXT("FangSpeed")),1);
 TestEqual(TEXT("Coverage rank converts"),U->GetUpgradeLevelById(TEXT("FangRange")),1);
 TestEqual(TEXT("Conversion adds no mastery"),U->GetNinjaMasteryPoints(),Mastery+1);
 FPlayerUpgradeRunState Migrated;U->CaptureRunState(Migrated);Migrated.Levels.Add(TEXT("CuttingReturn"),1);Migrated.Levels.Add(TEXT("FinalPursuit"),1);
 Migrated.Levels.Add(TEXT("FragmentDamage"),2);Migrated.BanishedUpgrades.Add(TEXT("NinjaCoverage"));
 U->RestoreRunState(Migrated);
 TestTrue(TEXT("Old returning-damage card becomes Resonance"),U->HasUpgradeId(TEXT("FangResonance")));
 TestTrue(TEXT("Old pursuit card becomes Splinter"),U->HasUpgradeId(TEXT("FangSplinter")));
 TestTrue(TEXT("Old support ranks retain damage investment"),FMath::IsNearlyEqual(U->GetAccumulatedUpgradeMagnitude(TEXT("FangDamage")),.8f));
 FPlayerUpgradeRunState RoundTrip;U->CaptureRunState(RoundTrip);TestTrue(TEXT("Banishment follows converted Coverage"),RoundTrip.BanishedUpgrades.Contains(TEXT("FangRange")));
 // Restore the pre-migration tree for unlock/prerequisite checks below.
 U->RestoreRunState(Basic);Grant(TEXT("ReturningFang"));
 TestFalse(TEXT("Basic investments excluded after stance"),U->CanAcquireUpgrade(U->FindUpgradeDefinition(TEXT("NinjaSpeed"))));
 TestFalse(TEXT("Legacy shared tree excluded from Fang"),U->CanAcquireUpgrade(U->FindUpgradeDefinition(TEXT("EmbeddedBlades"))));
 for(int32 i=0;i<6;++i)
 {
  TestTrue(TEXT("Unlock acquired"),Grant(OneTime[i]));
  TestTrue(TEXT("Matching rare becomes available"),U->CanAcquireUpgrade(U->FindUpgradeDefinition(Rare[i])));
 }
 for(int32 i=0;i<3;++i)TestTrue(TEXT("Frequency useful rank"),Grant(TEXT("FangTwinFrequency")));
 TestFalse(TEXT("No redundant fourth frequency rank"),Grant(TEXT("FangTwinFrequency")));
 U->BeginBloodShrineSelection(3);TestEqual(TEXT("Three Fang Shrine rewards"),U->GetCurrentUpgradeChoices().Num(),3);
 for(auto* Card:U->GetCurrentUpgradeChoices())TestTrue(TEXT("Only Fang Shrine cards"),FangBuild::IsShrine(Card->UpgradeId));
 for(int32 i=0;i<10;++i)
  for(auto* Card:U->RollUpgradeChoices(EUpgradeCategory::Ninja,100))TestFalse(TEXT("Shrine cards excluded from ordinary offers"),FangBuild::IsShrine(Card->UpgradeId));

 Reset();auto* E=Spawn(FVector(300,0,0),100000);auto* Other=Spawn(FVector(300,200,0),100000);auto* Third=Spawn(FVector(300,-200,0),100000);
 Grant(TEXT("FangTwin"));B->FangLaunchCount=3;
 auto* P=B->SpawnBlade(ENinjaProjectileKind::ReturningFang,FVector(0,0,50));P->LaunchFang();
 TSet<AEnemyBase*> Targets;Targets.Add(P->Target.Get());int32 Spectral=0;
 for(auto Blade:B->Projectiles)if(Blade.IsValid()&&Blade->bSpectralFang){++Spectral;Targets.Add(Blade->Target.Get());}
 TestEqual(TEXT("Twin creates two extras"),Spectral,2);TestEqual(TEXT("Three distinct targets when possible"),Targets.Num(),3);
 const int32 Launches=B->FangLaunchCount;
 for(auto Blade:TArray<TWeakObjectPtr<ANinjaBuildProjectile>>(B->Projectiles))if(Blade.IsValid()&&Blade->bSpectralFang)
  {Blade->bReturning=true;Blade->SetActorLocation(FVector(0,0,50));Blade->Tick(.01f);TestFalse(TEXT("Spectral Fang ends on return"),Blade.IsValid());}
 TestEqual(TEXT("Spectral returns never advance main launch counter"),B->FangLaunchCount,Launches);

 Reset();E=Spawn(FVector(300,0,0),100000);Other=Spawn(FVector(150,0,0),100000);
 P=B->SpawnBlade(ENinjaProjectileKind::ReturningFang,FVector(300,0,50));P->Damage=100;P->Speed=1000;P->bReturning=true;
 const float BeforeReturn=Other->GetHealthComponent()->GetCurrentHealth();
 P->Tick(.1f);P->Tick(.1f);
 TestEqual(TEXT("Return path deals no damage"),Other->GetHealthComponent()->GetCurrentHealth(),BeforeReturn);
 P->Damage=100;Grant(TEXT("RelentlessFang"));
 TestEqual(TEXT("First hit has no pressure"),B->FangHit(P,E),100.f);
 TestTrue(TEXT("Second hit builds pressure"),FMath::IsNearlyEqual(B->FangHit(P,E),115.f));
 TestEqual(TEXT("Other target resets pressure"),B->FangHit(P,Other),100.f);
  Grant(TEXT("FangPressure"));const float PressureHit=B->FangHit(P,Other);
  TestTrue(FString::Printf(TEXT("Rare scales pressure to 118 (actual %.6f)"),PressureHit),FMath::IsNearlyEqual(PressureHit,118.f,.001f));
 for(int32 i=0;i<10;++i)B->FangHit(P,Other);
  const float CappedHit=B->FangHit(P,Other);
  TestTrue(FString::Printf(TEXT("Pressure remains capped at 190 (actual %.6f)"),CappedHit),FMath::IsNearlyEqual(CappedHit,190.f,.001f));

 Reset();E=Spawn(FVector(150,0,0),100000);P=B->SpawnBlade(ENinjaProjectileKind::ReturningFang,FVector(0,0,50));P->Damage=100;
 Grant(TEXT("FangResonance"));const float BeforeBurst=E->GetHealthComponent()->GetCurrentHealth();
 B->FangReturned(P);TestEqual(TEXT("Canceled trips give no return burst"),E->GetHealthComponent()->GetCurrentHealth(),BeforeBurst);
 P->bFangHitThisTrip=true;B->FangReturned(P);
 TestEqual(TEXT("Return burst deals half Fang damage"),E->GetHealthComponent()->GetCurrentHealth(),BeforeBurst-50);
 TestEqual(TEXT("Burst does not count as direct Fang hit"),B->FangVictimHits.Num(),0);

 Reset();E=Spawn(FVector(150,0,0),250);Other=Spawn(FVector(400,0,0),100000);
 Grant(TEXT("FangSplinter"));P=B->SpawnBlade(ENinjaProjectileKind::ReturningFang,FVector(0,0,50));P->Damage=100;
 B->FangHit(P,E);B->FangHit(P,E);const int32 BeforeSplit=B->Projectiles.Num();B->FangHit(P,E);
 TestTrue(TEXT("Third hit kills"),E->IsDead());TestEqual(TEXT("Kill creates three ordinary kunai"),B->Projectiles.Num()-BeforeSplit,3);
 for(auto Blade:B->Projectiles)if(Blade.IsValid()&&Blade->Kind==ENinjaProjectileKind::Fragment)TestEqual(TEXT("Kunai base damage"),Blade->Damage,40.f);
 E=Spawn(FVector(150,200,0),1);const int32 BeforeOne=B->Projectiles.Num();B->FangHit(P,E);
 TestEqual(TEXT("One-hit kill creates one kunai"),B->Projectiles.Num()-BeforeOne,1);
 E=Spawn(FVector(150,-200,0),1);const int32 BeforeProc=B->Projectiles.Num();B->Hit(E,100,false);
 TestEqual(TEXT("Non-Fang kill cannot trigger splinter"),B->Projectiles.Num(),BeforeProc);
 B->ClearProjectiles();B->ScatterFangKunai(FVector(150,0,45),31,40);B->ProcessFangScatter();
 TestEqual(TEXT("Large kill counts are emitted exactly, not truncated to the legacy 24 cap"),B->Projectiles.Num(),31);

 Reset();E=Spawn(FVector(150,0,0),100000);P=B->SpawnBlade(ENinjaProjectileKind::ReturningFang,FVector(0,0,50));P->Damage=100;
 Grant(TEXT("FangFarstrider"));P->FangOutwardDistance=1000;
 TestEqual(TEXT("Farstrider max-distance damage"),B->FangHit(P,E),250.f);P->FangOutwardDistance=5000;
 TestEqual(TEXT("Farstrider distance bonus capped"),B->FangHit(P,E),250.f);
 TestEqual(TEXT("Farstrider flight cost"),B->FangSpeedMultiplier(),.75f);
 Grant(TEXT("FangRedline"));TestTrue(TEXT("Shrine speed tradeoffs combine"),FMath::IsNearlyEqual(B->FangSpeedMultiplier(),1.2f));
 TestEqual(TEXT("Redline damage cost"),B->FangDamageMultiplier(),.7f);
 TestFalse(TEXT("Critical pact requires Deadeye"),U->CanAcquireUpgrade(U->FindUpgradeDefinition(TEXT("FangPredator"))));
 Grant(TEXT("FangDeadeye"));Grant(TEXT("FangPredator"));auto* Crit=U->FindUpgradeDefinition(TEXT("FangDeadeye"));const auto SavedBalance=Crit->BalanceParameters;
 Crit->BalanceParameters.Add(TEXT("Chance"),1.f);P->FangOutwardDistance=0;TestEqual(TEXT("Critical pact amplified crit"),B->FangHit(P,E),300.f);
 Crit->BalanceParameters.Add(TEXT("Chance"),0.f);TestEqual(TEXT("Critical pact noncritical penalty"),B->FangHit(P,E),75.f);Crit->BalanceParameters=SavedBalance;
 Reset();E=Spawn(FVector(300,0,0),100000);
 auto* SamuraiClass=Cast<UClass>(FindFProperty<FClassProperty>(UCharacterManagerComponent::StaticClass(),TEXT("SamuraiClass"))->GetObjectPropertyValue_InContainer(Manager));
 auto* Samurai=World->SpawnActor<ASamuraiCharacter>(SamuraiClass,FVector(-500,0,0),FRotator::ZeroRotator,Params);
 Samurai->SetOwner(PC);Samurai->SetCharacterMode(ECharacterMode::Inactive);
 FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("SamuraiCharacter"))->SetObjectPropertyValue_InContainer(Manager,Samurai);
 Samurai->FindComponentByClass<UAutoAttackComponent>()->OwnerCharacter=Samurai;
 Grant(TEXT("FangAssist"));auto* AssistCard=U->FindUpgradeDefinition(TEXT("FangAssist"));const auto SavedAssist=AssistCard->BalanceParameters;
 AssistCard->BalanceParameters.Add(TEXT("Chance"),1.f);
 P=B->SpawnBlade(ENinjaProjectileKind::ReturningFang,FVector(0,0,50));P->bFangHitThisTrip=true;B->FangReturned(P);
 TestTrue(TEXT("Main return requests Samurai assist"),B->NextFangAssistTime>World->GetTimeSeconds());
 auto* Assist=PC->FindComponentByClass<UInactiveCharacterAssistComponent>();
 TestFalse(TEXT("Busy Fang assist cannot overlap"),Assist->TryFangAssist());Assist->DeactivateAssistEffect(true);
 B->NextFangAssistTime=0;P->bSpectralFang=true;P->bFangHitThisTrip=true;B->FangReturned(P);
 TestEqual(TEXT("Spectral return does not roll an assist"),B->NextFangAssistTime,0.f);
 AssistCard->BalanceParameters=SavedAssist;
 B->ClearProjectiles();PC->Destroy();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
