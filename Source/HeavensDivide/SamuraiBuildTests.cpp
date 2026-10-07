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
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"

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
 const TCHAR* Stances[]={TEXT("BladeWave"),TEXT("Iaijutsu"),TEXT("BattleStance")};
 const TCHAR* MeleeScaling[]={TEXT("SamuraiHeavyBlade"),TEXT("SamuraiTempo"),TEXT("SamuraiArea")};
 const TCHAR* IaijutsuScaling[]={TEXT("IaijutsuDamage"),TEXT("IaijutsuChargeSpeed"),TEXT("IaijutsuWidth"),TEXT("IaijutsuMarkDamage")};
 for(auto Id:MeleeScaling) TestTrue(TEXT("Shared scaling is available before choosing a stance"),U->CanAcquireUpgrade(Card(Id)));
 for(auto Id:IaijutsuScaling) TestFalse(TEXT("Iaijutsu scaling requires its stance"),U->CanAcquireUpgrade(Card(Id)));
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
  TestEqual(TEXT("Blood supports are exclusive"),U->CanAcquireUpgrade(Card(TEXT("Bloodletting"))), i==2);
  TestFalse(TEXT("Overkill is temporarily disabled"),U->CanAcquireUpgrade(Card(TEXT("OverkillBurst"))));
  TestFalse(TEXT("Wave cannot mix with another stance or duplicate itself"),U->CanAcquireUpgrade(Card(TEXT("BladeWave"))));
  for(auto Id:MeleeScaling)
  {
   TestEqual(TEXT("Melee scaling belongs only to Blood"),U->CanAcquireUpgrade(Card(Id)),i==2);
   if(i!=2)
   {
    TestFalse(TEXT("Non-Blood stance cannot acquire melee stats"),U->AcquireUpgrade(Card(Id)));
    TestFalse(TEXT("Debug grants cannot mix melee stats into other stances"),U->DebugForceAcquireUpgrade(Card(Id)));
    auto* Stale=NewObject<UUpgradeDefinition>(U);Stale->UpgradeId=Id;Stale->MaxLevel=1;
    TestFalse(TEXT("Stale melee card cannot bypass stance routing"),U->CanAcquireUpgrade(Stale));
   }
  }
  for(auto Id:IaijutsuScaling) TestEqual(TEXT("Iaijutsu scaling stays in Iaijutsu"),U->CanAcquireUpgrade(Card(Id)),i==1);
  TestEqual(TEXT("Attack speed acquisition respects stance"),U->AcquireUpgrade(Card(TEXT("SamuraiTempo"))),i==2);
  const float Tempo=U->GetAccumulatedUpgradeMagnitude(TEXT("SamuraiTempo"));
  TestTrue(TEXT("Speed scaling retains stance multiplier"),FMath::IsNearlyEqual(Stats->GetFinalAttackSpeedMultiplier(),Factor(Stances[i],ECharacterStatType::AttackSpeedMultiplier)*(1+Tempo)));
  FPlayerUpgradeRunState Saved;U->CaptureRunState(Saved);U->RestoreRunState(Saved);
  TestTrue(TEXT("Restoring stance does not stack modifiers"),FMath::IsNearlyEqual(Stats->GetFinalAttackSpeedMultiplier(),Factor(Stances[i],ECharacterStatType::AttackSpeedMultiplier)*(1+Tempo)));
 }
 U->RestoreRunState(Empty);TestEqual(TEXT("Removing old snapshot clears stance"),Stats->GetFinalDamageMultiplier(),1.f);
 for (FName OldId : {FName(TEXT("BloodStance")), FName(TEXT("ExecutionStance")), FName(TEXT("WaveStance"))})
 {
  auto* Stale=NewObject<UUpgradeDefinition>(U);Stale->UpgradeId=OldId;Stale->MaxLevel=1;
  TestFalse(TEXT("Retired stances reject stale acquisition"),U->CanAcquireUpgrade(Stale));
  FPlayerUpgradeRunState Legacy=Empty;Legacy.Levels.Add(OldId,1);Legacy.Definitions.Add(OldId,Stale);
  U->RestoreRunState(Legacy);
  TestFalse(TEXT("Retired stance is removed on restore"),U->HasUpgradeId(OldId));
  TestTrue(TEXT("Legacy stance maps to a current route"),U->HasUpgradeId(OldId==TEXT("WaveStance")?TEXT("BladeWave"):TEXT("BattleStance")));
 }
 FPlayerUpgradeRunState Mixed=Empty;
 Mixed.SamuraiMastery=19;
 for(auto Id:MeleeScaling)
 {
  Mixed.Levels.Add(Id,1);Mixed.Definitions.Add(Id,Card(Id));Mixed.AccumulatedMagnitudes.Add(Id,.25f);
 }
 for (FName Id : {FName(TEXT("Iaijutsu")),FName(TEXT("BladeWave")),FName(TEXT("WideArc"))})
 {Mixed.Levels.Add(Id,1);Mixed.Definitions.Add(Id,U->FindUpgradeDefinition(Id));}
 U->RestoreRunState(Mixed);
 TestTrue(TEXT("Legacy mixed run preserves Iaijutsu"),U->HasUpgradeId(TEXT("Iaijutsu")));
 for(auto Id:MeleeScaling)
 {
  TestEqual(TEXT("Legacy Iaijutsu removes melee scaling ranks"),U->GetUpgradeLevel(Card(Id)),0);
  TestEqual(TEXT("Legacy Iaijutsu removes melee scaling magnitudes"),U->GetAccumulatedUpgradeMagnitude(Id),0.f);
 }
 TestEqual(TEXT("Legacy melee damage cannot affect Iaijutsu"),Stats->GetFinalDamageMultiplier(),1.f);
 TestEqual(TEXT("Legacy melee speed cannot affect Iaijutsu"),Stats->GetFinalAttackSpeedMultiplier(),1.f);
 TestEqual(TEXT("Legacy melee area cannot affect Iaijutsu"),Stats->GetFinalAttackAreaMultiplier(),1.f);
 for(int32 Stat=0;Stat<3;++Stat)TestEqual(TEXT("Legacy scaling is converted to Iaijutsu"),U->GetUpgradeLevelById(IaijutsuScaling[Stat]),1);
 TestEqual(TEXT("Converting scaling preserves mastery"),U->GetSamuraiMasteryPoints(),19);
 TestFalse(TEXT("Legacy mix cannot retain wave stance"),U->HasUpgradeId(TEXT("BladeWave")));
 TestFalse(TEXT("Legacy mix removes unusable wave support"),U->HasUpgradeId(TEXT("WideArc")));
 auto* StaleWave=NewObject<UUpgradeDefinition>(U);StaleWave->UpgradeId=TEXT("BladeWave");StaleWave->MaxLevel=1;
 TestFalse(TEXT("Old standalone wave asset cannot bypass stance exclusivity"),U->CanAcquireUpgrade(StaleWave));
 U->RestoreRunState(Empty);
 auto Spawn=[&](FVector Position,float Health){auto* E=World->SpawnActor<AEnemyBase>(Position,FRotator::ZeroRotator,Params);E->ConfigureObjectiveEnemy(Health,EPlayerAttackSource::Other,nullptr,FLinearColor::White);E->GetHealthComponent()->RestoreCurrentHealth(Health);FScriptDelegate Death;Death.BindUFunction(E,TEXT("HandleDeath"));E->GetHealthComponent()->OnDeath.AddUnique(Death);return E;};
 auto* Source=Spawn(FVector(5000,0,0),10000);auto* Status=Source->GetStatusEffectComponent();
 TestFalse(TEXT("Normal hit cannot invent Bleed without starter"),Status->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,100));
 TestFalse(TEXT("Transfer gated by Bleed"),U->CanAcquireUpgrade(Card(TEXT("BloodTransfer"))));
 U->AcquireUpgrade(Card(TEXT("BattleStance")));
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
 const int32 TransferredStacks=Status->GetStatusStacks(EEnemyStatusEffect::Bleed);
 Source->ApplyPlayerDamage(20000,EPlayerAttackSource::Samurai);
 TestEqual(TEXT("Death spreads stacks to each neighbor"),NearA->GetStatusEffectComponent()->GetStatusStacks(EEnemyStatusEffect::Bleed),TransferredStacks);
 TestEqual(TEXT("Second neighbor receives same stacks"),NearB->GetStatusEffectComponent()->GetStatusStacks(EEnemyStatusEffect::Bleed),TransferredStacks);
 TestFalse(TEXT("Transfer respects source restrictions"),Restricted->HasStatus(EEnemyStatusEffect::Bleed));
 auto* Capped=NearA->GetStatusEffectComponent();
 for(int32 i=0;i<8;++i)Capped->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,80);
 TestEqual(TEXT("Base cap is five"),Capped->GetStatusStacks(EEnemyStatusEffect::Bleed),5);
 Capped->TickBleed();
 Capped->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,80);
 TestEqual(TEXT("Application refreshes duration even at cap"),Capped->BleedState.RemainingDuration,4.f);
 for(int32 i=0;i<5;++i)U->AcquireUpgrade(Card(TEXT("BloodCapacity")));
 for(int32 i=0;i<8;++i)Capped->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,80);
 TestEqual(TEXT("Upgraded cap is ten"),Capped->GetStatusStacks(EEnemyStatusEffect::Bleed),10);
 U->RestoreRunState(Empty);U->AcquireUpgrade(Card(TEXT("BattleStance")));
 auto* Exact=Spawn(FVector(10000,0,0),10000)->GetStatusEffectComponent();
 Exact->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,80);
 TestTrue(TEXT("One stack deals 12.5 percent total over six ticks"),FMath::IsNearlyEqual(Exact->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed),10.f,.01f));
 TestEqual(TEXT("Bleed tick interval"),Exact->BleedState.ActiveTickInterval,.5f);
 U->AcquireUpgrade(Card(TEXT("BloodDetonation")));
 TestFalse(TEXT("Detonation stops offering disabled Blood Transfer"),U->CanAcquireUpgrade(Card(TEXT("BloodTransfer"))));
 TestTrue(TEXT("Detonation alone unlocks radius upgrades"),U->CanAcquireUpgrade(Card(TEXT("BloodTransferArea"))));
 auto* Bomb=Spawn(FVector(14000,0,0),10000);auto* BlastTarget=Spawn(FVector(14100,0,0),10000);
 for(int32 i=0;i<5;++i)Bomb->GetStatusEffectComponent()->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,80);
 TestFalse(TEXT("Explosion consumes capped Bleed"),Bomb->HasStatus(EEnemyStatusEffect::Bleed));
 TestTrue(TEXT("Explosion hits origin for twice remaining damage"),FMath::IsNearlyEqual(Bomb->GetHealthComponent()->GetCurrentHealth(),9900.f,.02f));
 TestTrue(TEXT("Explosion hits neighbors"),FMath::IsNearlyEqual(BlastTarget->GetHealthComponent()->GetCurrentHealth(),9900.f,.02f));
 ++GFrameCounter;World->Tick(LEVELTICK_All,.001f); // Prime the timer manager before scheduling.
 Attack->BloodDetonationExplosionDelay=.3f;
 auto* DelayedBomb=Spawn(FVector(50000,0,0),10000);
 auto* Leaving=Spawn(FVector(50100,0,0),10000);
 auto* Entering=Spawn(FVector(52000,0,0),10000);
 for(int32 i=0;i<5;++i)DelayedBomb->GetStatusEffectComponent()->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,80);
 TestFalse(TEXT("Delayed explosion consumes Bleed immediately"),DelayedBomb->HasStatus(EEnemyStatusEffect::Bleed));
 TestEqual(TEXT("Damage waits for explosion delay"),Leaving->GetHealthComponent()->GetCurrentHealth(),10000.f);
 DelayedBomb->Destroy(); // A committed blast must survive its original victim.
 Leaving->SetActorLocation(FVector(52000,0,0));Entering->SetActorLocation(FVector(50100,0,0));
 ++GFrameCounter;World->Tick(LEVELTICK_All,.15f);
 TestEqual(TEXT("No early detonation"),Entering->GetHealthComponent()->GetCurrentHealth(),10000.f);
 ++GFrameCounter;World->Tick(LEVELTICK_All,.2f);
 TestTrue(TEXT("Delayed blast hits enemies entering original radius"),FMath::IsNearlyEqual(Entering->GetHealthComponent()->GetCurrentHealth(),9900.f,.02f));
 TestEqual(TEXT("Enemies leaving before detonation escape"),Leaving->GetHealthComponent()->GetCurrentHealth(),10000.f);
 Attack->BloodDetonationExplosionDelay=0.f;
 U->RestoreRunState(Empty);U->AcquireUpgrade(Card(TEXT("BattleStance")));U->AcquireUpgrade(Card(TEXT("DoubleCut")));
 Attack->DoubleCutPrimaryAttackCounter=0;
 TestEqual(TEXT("Double Cut starts every four attacks"),Attack->GetDoubleCutThreshold(),4);
 Samurai->SetActorLocation(FVector(18000,0,0));Samurai->SetVisualFacingRotation(FRotator::ZeroRotator);
 Attack->AttackRadius=100;Attack->AttackForwardOffset=160;Attack->AttackDamage=80;Attack->SamuraiPushbackDistance=0;
 auto* Behind=Spawn(FVector(17900,0,0),10000);
 for(int32 i=0;i<3;++i){Attack->bIsAttacking=true;Attack->bAttackNotifyConsumed=false;Attack->PerformAttackTrace();}
 TestEqual(TEXT("Normal first three swings miss rear target"),Behind->GetHealthComponent()->GetCurrentHealth(),10000.f);
 Attack->bIsAttacking=true;Attack->bAttackNotifyConsumed=false;Attack->PerformAttackTrace();
 TestTrue(TEXT("Fourth swing hits rear target for full damage"),FMath::IsNearlyEqual(10000-Behind->GetHealthComponent()->GetCurrentHealth(),Attack->GetEffectiveAttackDamage(),.02f));
 TestFalse(TEXT("Double Cut schedules no extra attack"),Attack->bDoubleCutFollowUpPending);
 auto* EchoTarget=Spawn(FVector(18200,0,0),10000);
 const FVector BeforeEcho=Samurai->GetActorLocation();
 Attack->ExecuteBloodEcho(BeforeEcho,FVector::ForwardVector,40.f,100.f,false);
 TestEqual(TEXT("Echo deals captured half damage"),EchoTarget->GetHealthComponent()->GetCurrentHealth(),9960.f);
 TestTrue(TEXT("Echo applies Blood Stance Bleed"),EchoTarget->HasStatus(EEnemyStatusEffect::Bleed));
 TestEqual(TEXT("Echo leaves player position unchanged"),Samurai->GetActorLocation(),BeforeEcho);
 for(int32 Rank=1;Rank<=3;++Rank)
 {
  TestTrue(TEXT("Useful frequency rank is acquired"),U->AcquireUpgrade(Card(TEXT("DoubleCutFrequency"))));
  TestEqual(TEXT("Frequency ranks clamp at every hit"),Attack->GetDoubleCutThreshold(),FMath::Max(1,4-Rank));
 }
 TestFalse(TEXT("Redundant fourth frequency rank cannot be acquired"),U->AcquireUpgrade(Card(TEXT("DoubleCutFrequency"))));
 TestTrue(TEXT("Blood Shrine offers tradeoffs"),U->BeginBloodShrineSelection(3));
 for(auto* Choice:U->GetCurrentUpgradeChoices()) TestTrue(TEXT("Shrine reward is Blood tradeoff"),Choice->UpgradeId==TEXT("BloodPactPower")||Choice->UpgradeId==TEXT("BloodPactSpeed")||Choice->UpgradeId==TEXT("BloodDetonation"));
 U->BeginDirectUpgradeSelection(100);
 for(auto* Choice:U->GetCurrentUpgradeChoices()) TestFalse(TEXT("Unrestricted rewards exclude Shrine tradeoffs"),Choice->UpgradeId==TEXT("BloodPactPower")||Choice->UpgradeId==TEXT("BloodPactSpeed")||Choice->UpgradeId==TEXT("BloodDetonation"));
 U->RestoreRunState(Empty);U->AcquireUpgrade(Card(TEXT("Iaijutsu")));
 TestFalse(TEXT("Intrinsic effects cannot bypass Blood exclusivity"),Exact->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,true,80));
 U->RestoreRunState(Empty);U->AcquireUpgrade(Card(TEXT("BladeWave")));
 Samurai->SetActorLocation(FVector(20000,0,0));
 Attack->StopAutoAttack();
 Attack->SpawnBladeWavesForAttack(Attack->GetEffectiveAttackDamage());
 int32 WaveCount=0;
 for(TActorIterator<ASamuraiBladeWave> It(World);It;++It)
 {
  ++WaveCount;
  TestFalse(TEXT("Base stance does not return without Returning Blade"),It->bReturns);
  TestTrue(TEXT("Base Crescent wave retains authored width"),FMath::IsNearlyEqual(It->Collision->GetUnscaledBoxExtent().Y*2,Card(TEXT("BladeWave"))->GetBalanceValue(TEXT("WaveWidth"),300.f),.02f));
 }
 TestEqual(TEXT("Base Crescent attack still launches one wave"),WaveCount,1);
 for(TActorIterator<ASamuraiBladeWave> It(World);It;++It)It->Destroy();
 for(TActorIterator<AEnemyBase> It(World);It;++It)It->GetStatusEffectComponent()->ClearAllStatuses();
 U->RestoreRunState(Empty);U->AcquireUpgrade(Card(TEXT("BattleStance")));U->AcquireUpgrade(Card(TEXT("BloodRush")));
 auto* Rush=Card(TEXT("BloodRush"));const auto RushBalance=Rush->BalanceParameters;
 Rush->BalanceParameters.Add(TEXT("Duration"),.2f);
 Samurai->ApplySharedMoveSpeedMultiplier(1.f);
 const float BaseMove=Samurai->GetCharacterMovement()->MaxWalkSpeed;
 const float NinjaMove=Ninja->GetCharacterMovement()->MaxWalkSpeed;
 Samurai->ApplySharedMoveSpeedMultiplier(1.3f);
 auto KillForRush=[&](bool Bleeding)
 {
  auto* Victim=Spawn(FVector(30000,0,0),100);
  if(Bleeding)Victim->GetStatusEffectComponent()->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,20);
  Victim->ApplyPlayerDamage(1000,EPlayerAttackSource::Samurai);
 };
 KillForRush(false);
 TestTrue(TEXT("Non-bleeding kills do not grant Blood Rush"),FMath::IsNearlyEqual(Samurai->GetCharacterMovement()->MaxWalkSpeed,BaseMove*1.3f,.01f));
 KillForRush(true);
 TestTrue(TEXT("Bleeding kill grants 20 percent movement on top of shared upgrades"),FMath::IsNearlyEqual(Samurai->GetCharacterMovement()->MaxWalkSpeed,BaseMove*1.3f*1.2f,.01f));
 ++GFrameCounter;World->Tick(LEVELTICK_All,.12f);KillForRush(true);
 ++GFrameCounter;World->Tick(LEVELTICK_All,.12f);
 TestTrue(TEXT("Blood Rush refreshes rather than stacks"),FMath::IsNearlyEqual(Samurai->GetCharacterMovement()->MaxWalkSpeed,BaseMove*1.3f*1.2f,.01f));
 Samurai->ApplySharedMoveSpeedMultiplier(1.5f);
 TestTrue(TEXT("Shared movement changes retain the temporary Blood bonus"),FMath::IsNearlyEqual(Samurai->GetCharacterMovement()->MaxWalkSpeed,BaseMove*1.5f*1.2f,.01f));
 ++GFrameCounter;World->Tick(LEVELTICK_All,.09f);
 TestTrue(TEXT("Blood Rush expires back to current shared movement"),FMath::IsNearlyEqual(Samurai->GetCharacterMovement()->MaxWalkSpeed,BaseMove*1.5f,.01f));
 U->AcquireUpgrade(Card(TEXT("BloodDetonation")));
 auto* DetonationVictim=Spawn(FVector(31000,0,0),1);
 for(int32 Stack=0;Stack<5;++Stack)DetonationVictim->GetStatusEffectComponent()->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,100);
 TestTrue(TEXT("Lethal detonation still grants Blood Rush after consuming Bleed"),DetonationVictim->IsDead()&&FMath::IsNearlyEqual(Samurai->GetCharacterMovement()->MaxWalkSpeed,BaseMove*1.5f*1.2f,.01f));
 Samurai->SetCharacterMode(ECharacterMode::Inactive);
 TestTrue(TEXT("Swapping away clears Blood Rush"),FMath::IsNearlyEqual(Samurai->GetCharacterMovement()->MaxWalkSpeed,BaseMove*1.5f,.01f));
 TestEqual(TEXT("Blood Rush does not change Ninja movement"),Ninja->GetCharacterMovement()->MaxWalkSpeed,NinjaMove);
 Samurai->SetCharacterMode(ECharacterMode::Active);KillForRush(true);
 U->RestoreRunState(Empty);
 TestTrue(TEXT("Restoring without Blood Rush clears its temporary boost"),FMath::IsNearlyEqual(Samurai->GetCharacterMovement()->MaxWalkSpeed,BaseMove*1.5f,.01f));
 Rush->BalanceParameters=RushBalance;
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
