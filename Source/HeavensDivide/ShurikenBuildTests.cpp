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
#include "ShurikenBuild.h"
#include "BarragePoisonPool.h"
#include "EnemyStatusEffectComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShurikenBuildsTest,"HeavensDivide.Combat.ShurikenBuilds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShurikenBuildsTest::RunTest(const FString&)
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
 auto Reset=[&](){B->ClearProjectiles();for(auto* E:Enemies)if(IsValid(E))E->Destroy();Enemies.Reset();U->RestoreRunState(Empty);Grant(TEXT("GreatShuriken"));};


 const FName Unlocks[]={TEXT("GrindingHalt"),TEXT("WideOrbit"),TEXT("BreakingWheel"),TEXT("ShurikenTwin"),TEXT("ShurikenAssist"),TEXT("SerratedEdge")};
 const FName Rare[]={TEXT("ShurikenGrindDuration"),TEXT("ShurikenGrowth"),TEXT("ShurikenBurstPower"),TEXT("ShurikenTwinFrequency"),TEXT("ShurikenAssistChance"),TEXT("ShurikenGrooves")};
 for(FName Id:Unlocks) TestFalse(TEXT("Mechanics require stance"),U->CanAcquireUpgrade(U->FindUpgradeDefinition(Id)));
 Grant(TEXT("NinjaDamage"));Grant(TEXT("NinjaSpeed"));Grant(TEXT("NinjaCoverage"));
 FPlayerUpgradeRunState Basic;U->CaptureRunState(Basic);Basic.AccumulatedMagnitudes[TEXT("NinjaDamage")]=.4f;U->RestoreRunState(Basic);
 const int32 Mastery=U->GetNinjaMasteryPoints();Grant(TEXT("GreatShuriken"));
 TestEqual(TEXT("Damage rank converts"),U->GetUpgradeLevelById(TEXT("ShurikenDamage")),1);
 TestEqual(TEXT("Rarity strength preserved"),U->GetAccumulatedUpgradeMagnitude(TEXT("ShurikenDamage")),.4f);
 TestEqual(TEXT("Speed converts"),U->GetUpgradeLevelById(TEXT("ShurikenSpeed")),1);
 TestEqual(TEXT("Coverage converts"),U->GetUpgradeLevelById(TEXT("ShurikenSize")),1);
 TestEqual(TEXT("Conversion preserves mastery"),U->GetNinjaMasteryPoints(),Mastery+1);
 for(FName Id:Rare) TestFalse(TEXT("Rare branches require unlock"),U->CanAcquireUpgrade(U->FindUpgradeDefinition(Id)));
 for(FName Id:Unlocks) Grant(Id);
 for(FName Id:Rare) TestTrue(TEXT("Rare branches become eligible"),U->CanAcquireUpgrade(U->FindUpgradeDefinition(Id)));
 for(int I=0;I<3;++I) Grant(TEXT("ShurikenTwinFrequency"));
 TestFalse(TEXT("Frequency has no redundant fourth rank"),Grant(TEXT("ShurikenTwinFrequency")));
 B->ClearProjectiles();B->ReplaceVolley(FVector::ForwardVector);TestEqual(TEXT("Every throw spawns two blades"),B->Projectiles.Num(),2);
 B->ClearProjectiles();B->ReplaceVolley(FVector::ForwardVector,true);TestEqual(TEXT("Assist does not advance Twin counter"),B->ShurikenThrowCount,0);
 for(auto* Card:U->RollUpgradeChoices(EUpgradeCategory::Ninja,100)) TestFalse(TEXT("Ordinary rewards exclude shrines"),ShurikenBuild::IsShrine(Card->UpgradeId));
 U->BeginBloodShrineSelection(100);for(auto* Card:U->GetCurrentUpgradeChoices()) TestTrue(TEXT("Shuriken shrine only offers its tradeoffs"),ShurikenBuild::IsShrine(Card->UpgradeId));
 Reset();FPlayerUpgradeRunState Old;U->CaptureRunState(Old);Old.Levels.Add(TEXT("HeavyShuriken"),3);Old.NinjaMastery=10;U->RestoreRunState(Old);
 TestEqual(TEXT("Old Heavy ranks become contact frequency"),U->GetUpgradeLevelById(TEXT("ShurikenTempo")),3);
 TestEqual(TEXT("Old rank migration preserves mastery"),U->GetNinjaMasteryPoints(),10);
 Reset();const float ThrowInterval=A->GetEffectiveAttackInterval(),ContactInterval=ShurikenBuild::ContactInterval(U);
 Grant(TEXT("ShurikenSpeed"));TestTrue(TEXT("Attack speed shortens throw interval"),A->GetEffectiveAttackInterval()<ThrowInterval);
 TestEqual(TEXT("Attack speed does not change contact interval"),ShurikenBuild::ContactInterval(U),ContactInterval);
 const float ThrowAfter=A->GetEffectiveAttackInterval();Grant(TEXT("ShurikenTempo"));TestEqual(TEXT("Tempo does not change throw interval"),A->GetEffectiveAttackInterval(),ThrowAfter);
 TestTrue(TEXT("Tempo speeds contact"),ShurikenBuild::ContactInterval(U)<ContactInterval);
 Reset();auto* E=Spawn(FVector(100,0,0),100000);auto* P=B->SpawnShuriken(FVector(100,0,40),FVector::ForwardVector,false);P->Speed=0;P->Damage=100;
 Grant(TEXT("WideOrbit"));const float Initial=P->Radius;P->Tick(.5f);TestTrue(TEXT("Growth works without travel"),P->Radius>Initial);
 Grant(TEXT("SerratedEdge"));P->HitCounts.Reset();P->ShurikenHit(E,false);TestTrue(TEXT("First contact starts serration"),FMath::IsNearlyEqual(P->SerrationBonus(E),1.15f));
 auto* Other=Spawn(FVector(300,0,0),100000);P->ShurikenHit(Other,false);TestTrue(TEXT("Different target retains previous stacks"),FMath::IsNearlyEqual(P->SerrationBonus(E),1.15f));
 Grant(TEXT("BreakingWheel"));const int32 Hits=P->HitCounts.FindRef(E);P->ShurikenBurst();TestEqual(TEXT("Burst cannot advance serration"),P->HitCounts.FindRef(E),Hits);
 auto* Elite=Spawn(FVector(400,0,0),100000);FindFProperty<FEnumProperty>(AEnemyBase::StaticClass(),TEXT("DropCategory"))->GetUnderlyingProperty()->SetIntPropertyValue(FindFProperty<FEnumProperty>(AEnemyBase::StaticClass(),TEXT("DropCategory"))->ContainerPtrToValuePtr<void>(Elite),int64(1));
 Grant(TEXT("GrindingHalt"));P->ShurikenHit(Elite,false);TestEqual(TEXT("First elite contact slows for one second"),P->SlowRemaining,1.f);P->SlowRemaining=0;P->ShurikenHit(Elite,false);TestEqual(TEXT("Blade cannot repeatedly grind"),P->SlowRemaining,0.f);
 Reset();Grant(TEXT("ShurikenSize"));Grant(TEXT("ShurikenHunger"));Grant(TEXT("WideOrbit"));Grant(TEXT("ShurikenGrowth"));Grant(TEXT("ShurikenOrbit"));Grant(TEXT("BreakingWheel"));Grant(TEXT("ShurikenPulse"));
 P=B->SpawnShuriken(FVector(65,0,50),FVector::ForwardVector,false);P->Damage=100;const float Small=P->InitialRadius;
 P->ShurikenKills=1;P->UpdateShurikenGrowth();TestTrue(TEXT("Size investment increases kill growth"),FMath::IsNearlyEqual(P->Radius/Small,1.115f,.001f));
 P->ShurikenKills=100;P->UpdateShurikenGrowth();TestTrue(TEXT("Shared cap bounds growth"),FMath::IsNearlyEqual(P->Radius/Small,2.2f,.001f));
 auto* Fresh=B->SpawnShuriken(FVector(65,0,50),FVector::ForwardVector,false);TestEqual(TEXT("Each blade starts with no kills"),Fresh->ShurikenKills,0);Fresh->Destroy();
 N->SetActorLocation(FVector(200,100,0));P->Tick(.1f);TestTrue(TEXT("Orbit follows active Ninja"),P->OrbitOrigin.Equals(N->GetActorLocation()+FVector(0,0,50),.01f));
 const float Angle=P->OrbitAngle;P->SlowRemaining=1;P->Tick(.1f);const float SlowAngle=FMath::Abs(P->OrbitAngle-Angle);P->SlowRemaining=0;const float BeforeAngle=P->OrbitAngle;P->Tick(.1f);TestTrue(TEXT("Grinding reduces orbit motion"),FMath::Abs(P->OrbitAngle-BeforeAngle)>SlowAngle*5);
 P->Speed=0;P->PulseElapsed=0;E=Spawn(P->GetActorLocation()+FVector(P->Radius*1.6f,0,-50),100000);P->HitCounts.Reset();P->Tick(1.f);TestTrue(TEXT("Combined orbit hunger and pulse reaches beyond contact radius"),FMath::IsNearlyEqual(E->GetHealthComponent()->GetCurrentHealth(),99925.f,.01f));
 TestTrue(TEXT("Pulse cadence remains bounded"),P->PulseElapsed<.01f);
 const int32 BeforeCount=B->Projectiles.Num();const float HP=E->GetHealthComponent()->GetCurrentHealth();P->Finish();const float After=E->GetHealthComponent()->GetCurrentHealth();P->Finish();
 TestTrue(TEXT("Expiry burst deals damage"),After<HP);TestEqual(TEXT("Expiry cannot burst twice"),E->GetHealthComponent()->GetCurrentHealth(),After);TestEqual(TEXT("Burst does not spawn fragments"),B->Projectiles.Num(),BeforeCount);
 Reset();Grant(TEXT("BreakingWheel"));Grant(TEXT("ShurikenHunger"));P=B->SpawnShuriken(FVector(100,0,50),FVector::ForwardVector,false);P->Damage=100;E=Spawn(FVector(100,0,0),1);P->ShurikenBurst();TestEqual(TEXT("Burst kill feeds Blood Hunger"),P->ShurikenKills,1);TestTrue(TEXT("Kill immediately grows blade"),P->Radius>P->InitialRadius);
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
