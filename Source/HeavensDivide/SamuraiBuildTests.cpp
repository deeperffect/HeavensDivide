#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerUpgradeComponent.h"
#include "AutoAttackComponent.h"
#include "CharacterManagerComponent.h"
#include "SurvivorPlayerController.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "SamuraiBladeWave.h"
#include "EnemyStatusEffectComponent.h"
#include "EnemyBase.h"
#include "HealthComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSamuraiBuildsTest,"HeavensDivide.Combat.SamuraiBuilds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSamuraiBuildsTest::RunTest(const FString&)
{
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 auto Class=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 auto* PC=World->SpawnActor<ASurvivorPlayerController>(Class);
 if(!TestNotNull(TEXT("Saved controller"),PC)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 PC->GetPlayerHealthComponent()->RestoreCurrentHealth(100);
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Samurai=World->SpawnActor<ASamuraiCharacter>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
 auto* Ninja=World->SpawnActor<ANinjaCharacter>(FVector(0,500,0),FRotator::ZeroRotator,Params);
 Samurai->SetOwner(PC);Ninja->SetOwner(PC);Samurai->SetCharacterMode(ECharacterMode::Active);
 auto* Manager=PC->GetCharacterManager();
 FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("SamuraiCharacter"))->SetObjectPropertyValue_InContainer(Manager,Samurai);
 FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("NinjaCharacter"))->SetObjectPropertyValue_InContainer(Manager,Ninja);
 FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(Manager,Samurai);
 auto* U=PC->GetPlayerUpgrades();auto* Stats=Samurai->GetCharacterStats();
 auto* Attack=Samurai->FindComponentByClass<UAutoAttackComponent>();Attack->OwnerCharacter=Samurai;Attack->ImpactFeedback.bEnableCameraShake=false;
 FPlayerUpgradeRunState Empty;U->CaptureRunState(Empty);
 auto Card=[&](const TCHAR* Id){auto* C=U->FindUpgradeDefinition(Id);TestNotNull(Id,C);return C;};
 const TCHAR* Stances[]={TEXT("BloodStance"),TEXT("ExecutionStance"),TEXT("WaveStance")};
 const auto Factor=[&](const TCHAR* Id,ECharacterStatType Stat)
 {
  float Value=1.f;
  for(const auto& M:Card(Id)->StatModifiers)if(M.Target==EUpgradeStatTarget::Samurai&&M.CharacterStat==Stat&&M.Operation==EStatModifierOperation::Multiply)Value*=M.ValuePerLevel;
  return Value;
 };
 for(int32 i=0;i<3;++i)
 {
  U->RestoreRunState(Empty);
  TestTrue(TEXT("Acquire stance"),U->AcquireUpgrade(Card(Stances[i])));
  TestTrue(TEXT("Correct damage tradeoff"),FMath::IsNearlyEqual(Stats->GetFinalDamageMultiplier(),Factor(Stances[i],ECharacterStatType::DamageMultiplier)));
  TestTrue(TEXT("Correct speed tradeoff"),FMath::IsNearlyEqual(Stats->GetFinalAttackSpeedMultiplier(),Factor(Stances[i],ECharacterStatType::AttackSpeedMultiplier)));
  TestTrue(TEXT("Correct area tradeoff"),FMath::IsNearlyEqual(Stats->GetFinalAttackAreaMultiplier(),Factor(Stances[i],ECharacterStatType::AttackAreaMultiplier)));
  TestEqual(TEXT("Samurai stance leaves Ninja unchanged"),Ninja->GetCharacterStats()->GetFinalDamageMultiplier(),1.f);
  for(int32 j=0;j<3;++j)TestFalse(TEXT("Only one stance per run"),U->CanAcquireUpgrade(Card(Stances[j])));
  TestTrue(TEXT("Bleed remains mixable"),U->CanAcquireUpgrade(Card(TEXT("BleedingEdge"))));
  TestTrue(TEXT("Explosion remains mixable"),U->CanAcquireUpgrade(Card(TEXT("OverkillBurst"))));
  TestTrue(TEXT("Wave remains mixable"),U->CanAcquireUpgrade(Card(TEXT("BladeWave"))));
  U->AcquireUpgrade(Card(TEXT("SamuraiTempo")));
  const float Tempo=U->GetAccumulatedUpgradeMagnitude(TEXT("SamuraiTempo"));
  TestTrue(TEXT("Speed scaling retains stance multiplier"),FMath::IsNearlyEqual(Stats->GetFinalAttackSpeedMultiplier(),Factor(Stances[i],ECharacterStatType::AttackSpeedMultiplier)*(1+Tempo)));
  FPlayerUpgradeRunState Saved;U->CaptureRunState(Saved);U->RestoreRunState(Saved);
  TestTrue(TEXT("Restoring stance does not stack modifiers"),FMath::IsNearlyEqual(Stats->GetFinalAttackSpeedMultiplier(),Factor(Stances[i],ECharacterStatType::AttackSpeedMultiplier)*(1+Tempo)));
 }
 U->RestoreRunState(Empty);TestEqual(TEXT("Removing old snapshot clears stance"),Stats->GetFinalDamageMultiplier(),1.f);
 auto Spawn=[&](FVector Position,float Health){auto* E=World->SpawnActor<AEnemyBase>(Position,FRotator::ZeroRotator,Params);E->ConfigureObjectiveEnemy(Health,EPlayerAttackSource::Other,nullptr,FLinearColor::White);E->GetHealthComponent()->RestoreCurrentHealth(Health);FScriptDelegate Death;Death.BindUFunction(E,TEXT("HandleDeath"));E->GetHealthComponent()->OnDeath.AddUnique(Death);return E;};
 auto* Source=Spawn(FVector(5000,0,0),10000);auto* Status=Source->GetStatusEffectComponent();
 TestFalse(TEXT("Normal hit cannot invent Bleed without starter"),Status->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,100));
 TestFalse(TEXT("Transfer gated by Bleed"),U->CanAcquireUpgrade(Card(TEXT("BloodTransfer"))));
 U->AcquireUpgrade(Card(TEXT("BleedingEdge")));
 Status->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,20);
 const float Light=Status->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed);Status->ClearAllStatuses();
 Status->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,100);
 TestTrue(TEXT("Applying hit damage strengthens Bleed"),Status->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed)>Light);
 U->AcquireUpgrade(Card(TEXT("Bloodletting")));Status->ClearAllStatuses();
 Status->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,20);
 TestEqual(TEXT("Bloodletting adds a stack per hit"),Status->GetStatusStacks(EEnemyStatusEffect::Bleed),2);
 const float BeforeDuration=Status->BleedState.RemainingDuration;
 U->AcquireUpgrade(Card(TEXT("LingeringWounds")));Status->ClearAllStatuses();Status->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,20);
 TestTrue(TEXT("Duration upgrade extends newly applied Bleed"),Status->BleedState.RemainingDuration>BeforeDuration);
 U->AcquireUpgrade(Card(TEXT("BloodTransfer")));
 auto* NearA=Spawn(FVector(5100,0,0),10000);auto* NearB=Spawn(FVector(5200,0,0),10000);
 auto* Restricted=Spawn(FVector(5100,70,0),10000);Restricted->ConfigureObjectiveEnemy(10000,EPlayerAttackSource::Ninja,nullptr,FLinearColor::White);
 const float Budget=Status->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed)*.5f;
 Source->ApplyPlayerDamage(20000,EPlayerAttackSource::Samurai);
 const float Shared=NearA->GetStatusEffectComponent()->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed)+NearB->GetStatusEffectComponent()->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed);
 TestTrue(TEXT("Death distributes half the remaining budget without free stack damage"),FMath::IsNearlyEqual(Shared,Budget,.02f));
 TestFalse(TEXT("Transfer respects damage-source restrictions"),Restricted->HasStatus(EEnemyStatusEffect::Bleed));
 const float BeforeRefresh=NearA->GetStatusEffectComponent()->BleedState.TransferredDamageRemaining;
 NearA->GetStatusEffectComponent()->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,20);
 TestEqual(TEXT("Refreshing duration does not multiply transferred budget"),NearA->GetStatusEffectComponent()->BleedState.TransferredDamageRemaining,BeforeRefresh);
 auto* DotSource=Spawn(FVector(10000,0,0),1);auto* DotTarget=Spawn(FVector(10100,0,0),10000);
 auto* Dot=DotSource->GetStatusEffectComponent();Dot->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,20);
 const float RemainingBeforeTick=Dot->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed);
 const float TickDamage=Dot->CalculateStatusDamagePerTick(EEnemyStatusEffect::Bleed,Dot->BleedState);
 Dot->TickBleed();
 TestTrue(TEXT("Lethal Bleed tick transfers only unspent ticks"),FMath::IsNearlyEqual(DotTarget->GetStatusEffectComponent()->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed),(RemainingBeforeTick-TickDamage)*.5f,.02f));
 U->RestoreRunState(Empty);U->AcquireUpgrade(Card(TEXT("OverkillBurst")));
 auto* Victim=Spawn(FVector(100,0,0),40);auto* Splash=Spawn(FVector(300,50,0),10000);auto* ChainVictim=Spawn(FVector(300,0,0),1);auto* Beyond=Spawn(FVector(500,0,0),10000);
 Attack->AttackDamage=150;Attack->AttackRadius=100;Attack->AttackForwardOffset=100;Attack->SamuraiPushbackDistance=0;
 const float ExpectedBurst=Attack->GetEffectiveAttackDamage()-40;
 Attack->bIsAttacking=true;Attack->bAttackNotifyConsumed=false;Attack->PerformAttackTrace();
 TestTrue(TEXT("Direct normal killing blow produces actual overkill splash"),FMath::IsNearlyEqual(10000-Splash->GetHealthComponent()->GetCurrentHealth(),ExpectedBurst,.02f));
 TestTrue(TEXT("Explosion can kill another enemy"),ChainVictim->IsDead());
 TestEqual(TEXT("Explosion kills cannot chain explosions"),Beyond->GetHealthComponent()->GetCurrentHealth(),10000.f);
 TestFalse(TEXT("Explosion never invents Bleed"),Splash->HasStatus(EEnemyStatusEffect::Bleed));
 U->RestoreRunState(Empty);U->AcquireUpgrade(Card(TEXT("WaveStance")));U->AcquireUpgrade(Card(TEXT("BladeWave")));
 for(int32 i=0;i<3;++i)TestTrue(TEXT("Acquire additional wave rank"),U->AcquireUpgrade(Card(TEXT("WaveMultishot"))));
 TestFalse(TEXT("Extra waves have a rank cap"),U->CanAcquireUpgrade(Card(TEXT("WaveMultishot"))));
 U->AcquireUpgrade(Card(TEXT("CrossingBlades")));U->AcquireUpgrade(Card(TEXT("ReturningBlade")));
 Samurai->SetActorLocation(FVector(20000,0,0));Attack->CrossingBladesAttackCounter=0;
 auto Waves=[&](){TArray<ASamuraiBladeWave*> Out;for(TActorIterator<ASamuraiBladeWave> It(World);It;++It)Out.Add(*It);return Out;};
 for(int32 Swing=0;Swing<3;++Swing)
 {
  const int32 Before=Waves().Num();Attack->bIsAttacking=true;Attack->bAttackNotifyConsumed=false;Attack->PerformAttackTrace();
  TestEqual(TEXT("Committed swing adds waves and Crossing Blades combines"),Waves().Num()-Before,Swing==2?6:4);
 }
 for(auto* Wave:Waves())
 {
  TestTrue(TEXT("Area penalty narrows wave collision"),FMath::IsNearlyEqual(Wave->Collision->GetUnscaledBoxExtent().Y*2,300.f*.65f,.02f));
  TestTrue(TEXT("Additional waves inherit return behavior"),Wave->bReturns);
 }
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
