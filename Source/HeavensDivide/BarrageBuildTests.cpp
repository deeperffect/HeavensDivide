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
#include "BarrageBuild.h"
#include "BarragePoisonPool.h"
#include "EnemyStatusEffectComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBarrageBuildsTest,"HeavensDivide.Combat.BarrageBuilds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBarrageBuildsTest::RunTest(const FString&)
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
 auto Reset=[&](){B->ClearProjectiles();for(auto* E:Enemies)if(IsValid(E))E->Destroy();Enemies.Reset();U->RestoreRunState(Empty);Grant(TEXT("BarrageStance"));};

 const FName Unlocks[]={TEXT("NeedleRain"),TEXT("ForkingProjectiles"),TEXT("BarragePool"),TEXT("BarrageAssist"),TEXT("BarrageCritical"),TEXT("BarrageRush")};
 for(FName Id:Unlocks)TestFalse(TEXT("Barrage mechanics unavailable before stance"),U->CanAcquireUpgrade(U->FindUpgradeDefinition(Id)));
 Grant(TEXT("NinjaDamage"));Grant(TEXT("NinjaSpeed"));Grant(TEXT("NinjaCoverage"));
 FPlayerUpgradeRunState Basic;U->CaptureRunState(Basic);Basic.AccumulatedMagnitudes[TEXT("NinjaDamage")]=.4f;U->RestoreRunState(Basic);
 const int32 Mastery=U->GetNinjaMasteryPoints();Grant(TEXT("BarrageStance"));
 TestEqual(TEXT("Damage rank converts"),U->GetUpgradeLevelById(TEXT("BarrageDamage")),1);
 TestEqual(TEXT("Rarity strength survives conversion"),U->GetAccumulatedUpgradeMagnitude(TEXT("BarrageDamage")),.4f);
 TestEqual(TEXT("Speed converts"),U->GetUpgradeLevelById(TEXT("BarrageSpeed")),1);TestEqual(TEXT("Coverage converts"),U->GetUpgradeLevelById(TEXT("BarrageRange")),1);
 TestEqual(TEXT("Conversion adds no extra mastery"),U->GetNinjaMasteryPoints(),Mastery+1);
 FPlayerUpgradeRunState Legacy;U->CaptureRunState(Legacy);Legacy.Levels.Add(TEXT("Crescendo"),1);Legacy.Levels.Add(TEXT("FragmentDamage"),2);Legacy.BanishedUpgrades.Add(TEXT("NinjaCoverage"));U->RestoreRunState(Legacy);
 TestTrue(TEXT("Legacy ranks retain their investment"),FMath::IsNearlyEqual(U->GetAccumulatedUpgradeMagnitude(TEXT("BarrageDamage")),1.f,.001f));
 FPlayerUpgradeRunState Restored;U->CaptureRunState(Restored);TestTrue(TEXT("Banishment converts with Coverage"),Restored.BanishedUpgrades.Contains(TEXT("BarrageRange")));
 TestTrue(TEXT("Deep Venom has no Rush prerequisite"),U->CanAcquireUpgrade(U->FindUpgradeDefinition(TEXT("BarragePoisonCap"))));
 TestFalse(TEXT("Bloom requires Toxic Ground"),U->CanAcquireUpgrade(U->FindUpgradeDefinition(TEXT("BarrageBloom"))));
 TestFalse(TEXT("Old shared supports excluded"),U->CanAcquireUpgrade(U->FindUpgradeDefinition(TEXT("EmbeddedBlades"))));
 Reset();auto* E=Spawn(FVector(300,0,0),100000);auto* S=E->GetStatusEffectComponent();
 S->ApplyBarragePoison(U,100);TestEqual(TEXT("One base poison stack"),S->GetStatusStacks(EEnemyStatusEffect::Poison),1);
 TestTrue(TEXT("Five percent over five seconds"),FMath::IsNearlyEqual(S->CalculateRemainingStatusDamage(EEnemyStatusEffect::Poison),5.f,.001f));
 for(int I=0;I<30;++I)S->ApplyBarragePoison(U,100);
 TestEqual(TEXT("Base cap twenty"),S->GetStatusStacks(EEnemyStatusEffect::Poison),20);
 TestTrue(TEXT("Capped applications add no damage"),FMath::IsNearlyEqual(S->CalculateRemainingStatusDamage(EEnemyStatusEffect::Poison),100.f,.001f));
 S->TickPoison();S->ApplyBarragePoison(U,9000);TestTrue(TEXT("Cap application refreshes existing damage budget"),FMath::IsNearlyEqual(S->CalculateRemainingStatusDamage(EEnemyStatusEffect::Poison),100.f,.001f));
 Grant(TEXT("BarragePoisonDuration"));S->ApplyBarragePoison(U,100);TestTrue(TEXT("Duration adds ticks at unchanged rate"),FMath::IsNearlyEqual(S->CalculateRemainingStatusDamage(EEnemyStatusEffect::Poison),140.f,.001f));
 S->ClearAllStatuses();Grant(TEXT("BarragePoisonLoad"));S->ApplyBarragePoison(U,100);TestEqual(TEXT("Load adds two stacks"),S->GetStatusStacks(EEnemyStatusEffect::Poison),3);
 for(int I=0;I<5;++I)Grant(TEXT("BarragePoisonCap"));for(int I=0;I<20;++I)S->ApplyBarragePoison(U,100);TestEqual(TEXT("Rare cap reaches forty"),S->GetStatusStacks(EEnemyStatusEffect::Poison),40);
 Reset();E=Spawn(FVector(300,0,0),100000);S=E->GetStatusEffectComponent();Grant(TEXT("NeedleRain"));
 int32 Count=3;float Spread=10;B->VolleyCount=3;B->ModifyVolley(Count,Spread);TestEqual(TEXT("Fourth volley doubles"),Count,6);
 for(int I=0;I<3;++I)Grant(TEXT("BarrageRainFrequency"));TestFalse(TEXT("No redundant fourth frequency"),Grant(TEXT("BarrageRainFrequency")));
 Count=3;B->ModifyVolley(Count,Spread);TestEqual(TEXT("Maximum frequency doubles every volley"),Count,6);
 auto Shoot=[&](AEnemyBase* Target,float Damage,int Split=0){auto* P=World->SpawnActor<AAttackProjectileBase>(FVector(5000,0,50),FRotator::ZeroRotator,Params);P->InitializeProjectile(N,FVector::ForwardVector,Damage,1000,EProjectileTargetType::Enemies,1000,nullptr,true,0,0,Split);P->HandleProjectileOverlap(nullptr,Target,nullptr,0,false,FHitResult());return P;};
 Grant(TEXT("BarrageCritical"));auto* Crit=U->FindUpgradeDefinition(TEXT("BarrageCritical"));auto OldCrit=Crit->BalanceParameters;Crit->BalanceParameters.Add(TEXT("Chance"),1.f);
 Shoot(E,100);TestTrue(TEXT("Critical poison uses actual doubled hit"),FMath::IsNearlyEqual(S->CalculateRemainingStatusDamage(EEnemyStatusEffect::Poison),10.f,.001f));Crit->BalanceParameters=OldCrit;
 Grant(TEXT("ForkingProjectiles"));auto* Fork=U->FindUpgradeDefinition(TEXT("ForkingProjectiles"));auto OldFork=Fork->BalanceParameters;Fork->BalanceParameters.Add(TEXT("Chance"),1.f);
 TSet<AAttackProjectileBase*> Before;for(TActorIterator<AAttackProjectileBase> I(World);I;++I)Before.Add(*I);
 auto* Parent=Shoot(E,100,1);int32 Children=0;for(TActorIterator<AAttackProjectileBase> I(World);I;++I)if(*I!=Parent&&!Before.Contains(*I)){++Children;TestFalse(TEXT("Child cannot split"),I->bCanTriggerSplit);TestEqual(TEXT("Child damage half"),I->ProjectileDamage,50.f);}
 TestEqual(TEXT("Split emits two children"),Children,2);Fork->BalanceParameters=OldFork;
 Grant(TEXT("BarrageProcession"));auto* Ricochet=World->SpawnActor<AAttackProjectileBase>(FVector(5000,0,50),FRotator::ZeroRotator,Params);Ricochet->InitializeProjectile(N,FVector::ForwardVector,100,1000,EProjectileTargetType::Enemies,1000,nullptr,true,0,0,1);
 TestFalse(TEXT("Procession never splits"),Ricochet->bCanTriggerSplit);TestEqual(TEXT("Three additional bounce targets"),Ricochet->RemainingBounces,3);
 TestTrue(TEXT("Split unlock improves ricochet retention"),FMath::IsNearlyEqual(Ricochet->BarrageBounceRetention,.78f,.001f));
 for(int I=0;I<5;++I)Grant(TEXT("BarrageSplitChance"));TestTrue(TEXT("Split investment remains useful with Procession"),FMath::IsNearlyEqual(.75f+BarrageBuild::SplitChance(U)*.2f,.88f,.001f));
 const float Range=A->GetEffectiveTargetingRange(),Interval=A->GetEffectiveAttackInterval();Grant(TEXT("BarragePointBlank"));TestTrue(TEXT("Point-blank range cost"),FMath::IsNearlyEqual(A->GetEffectiveTargetingRange(),Range*.3f,.001f));TestTrue(TEXT("Point-blank attack-speed benefit"),FMath::IsNearlyEqual(A->GetEffectiveAttackInterval(),Interval/1.5f,.001f));
 Reset();Grant(TEXT("BarragePool"));Grant(TEXT("BarrageRush"));E=Spawn(FVector(300,0,0),1);
 auto Pools=[&](){int C=0;for(TActorIterator<ABarragePoisonPool> I(World);I;++I)++C;return C;};
 const float Walk=N->GetCharacterMovement()->MaxWalkSpeed;const int BeforePools=Pools();Shoot(E,100);
 TestEqual(TEXT("One-hit kill leaves a pool"),Pools(),BeforePools+1);TestTrue(TEXT("Poison kill grants movement"),FMath::IsNearlyEqual(N->GetCharacterMovement()->MaxWalkSpeed,Walk*1.2f,.01f));N->ClearViperRush();
 auto* PuddleOnly=Spawn(FVector(1000,0,0),1);PuddleOnly->GetStatusEffectComponent()->ApplyBarragePoison(U,100,true);const int BeforeChain=Pools();PuddleOnly->ApplyStatusDamage(100,EPlayerAttackSource::Ninja);TestEqual(TEXT("Puddle-only poison cannot chain pools"),Pools(),BeforeChain);
 E=Spawn(FVector(300,0,0),100000);S=E->GetStatusEffectComponent();S->ApplyBarragePoison(U,100);
 Grant(TEXT("BarrageBloom"));auto* Pool=World->SpawnActor<ABarragePoisonPool>(FVector(300,0,0),FRotator::ZeroRotator,Params);Pool->Initialize(U,100);
 const float HP=E->GetHealthComponent()->GetCurrentHealth(),Budget=S->CalculateRemainingStatusDamage(EEnemyStatusEffect::Poison);Pool->Pulse();
 TestTrue(TEXT("Bloom deals ten percent remaining damage"),FMath::IsNearlyEqual(HP-E->GetHealthComponent()->GetCurrentHealth(),Budget*.1f,.01f));TestEqual(TEXT("Bloom does not consume poison"),S->CalculateRemainingStatusDamage(EEnemyStatusEffect::Poison),Budget);TestEqual(TEXT("Bloom adds no stack"),S->GetStatusStacks(EEnemyStatusEffect::Poison),1);
 for(int I=0;I<5;++I)Pool->Pulse();TestTrue(TEXT("Pool ends after six pulses"),Pool->IsActorBeingDestroyed());
 for(auto* C:U->RollUpgradeChoices(EUpgradeCategory::Ninja,100))TestFalse(TEXT("Shrines excluded from ordinary rewards"),BarrageBuild::IsShrine(C->UpgradeId));
 // A serial volley emits at intervals and cancellation removes all pending throws.
 auto Kunais=[&](){int C=0;for(TActorIterator<AAttackProjectileBase> I(World);I;++I)if(I->GetActorLocation().X>=7999)++C;return C;};
 Grant(TEXT("BarrageProcession"));const int BeforeSerial=Kunais();A->SpawnBarrageSequence(FVector(8000,0,50),FVector::ForwardVector,3,10,100,0);
 TestEqual(TEXT("Serial volley starts with one projectile"),Kunais(),BeforeSerial+1);
 ++GFrameCounter;World->GetTimerManager().Tick(.001f);++GFrameCounter;World->GetTimerManager().Tick(.2f);
 TestEqual(TEXT("Serial volley completes all three projectiles"),Kunais(),BeforeSerial+3);
 A->SpawnBarrageSequence(FVector(8000,0,50),FVector::ForwardVector,3,10,100,0);const int BeforeCancel=Kunais();A->StopAutoAttack();++GFrameCounter;World->GetTimerManager().Tick(.2f);
 TestEqual(TEXT("Stopping attacks cancels serial followups"),Kunais(),BeforeCancel);
 Reset();E=Spawn(FVector(300,0,0),100000);
 auto* SamuraiClass=Cast<UClass>(FindFProperty<FClassProperty>(UCharacterManagerComponent::StaticClass(),TEXT("SamuraiClass"))->GetObjectPropertyValue_InContainer(Manager));
 auto* Samurai=World->SpawnActor<ASamuraiCharacter>(SamuraiClass,FVector(-500,0,0),FRotator::ZeroRotator,Params);Samurai->SetOwner(PC);Samurai->SetCharacterMode(ECharacterMode::Inactive);
 FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("SamuraiCharacter"))->SetObjectPropertyValue_InContainer(Manager,Samurai);Samurai->FindComponentByClass<UAutoAttackComponent>()->OwnerCharacter=Samurai;
 Grant(TEXT("BarrageAssist"));auto* AssistCard=U->FindUpgradeDefinition(TEXT("BarrageAssist"));const auto SavedAssist=AssistCard->BalanceParameters;AssistCard->BalanceParameters.Add(TEXT("Chance"),1.f);
 A->bAutoAttackEnabled=true;A->bIsAttacking=true;A->bAttackNotifyConsumed=false;A->SpawnAutoAttackProjectile();
 auto* Assist=PC->FindComponentByClass<UInactiveCharacterAssistComponent>();TestFalse(TEXT("Volley-triggered assist is busy and cannot overlap"),Assist->TryBarrageAssist());
 TestEqual(TEXT("Samurai is assisting after the volley"),Samurai->GetCharacterMode(),ECharacterMode::Assisting);Assist->DeactivateAssistEffect(true);AssistCard->BalanceParameters=SavedAssist;
 B->ClearProjectiles();PC->Destroy();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
