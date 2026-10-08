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
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "SwapPresentationComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
 TestNotNull(TEXT("Dedicated Fang projectile assigned"),B->FangProjectileClass.Get());
 TestTrue(TEXT("Dedicated Fang projectile is user asset"),B->FangProjectileClass && B->FangProjectileClass->GetName()==TEXT("BP_NinjaProjectileFang_C"));
 TestNotNull(TEXT("Fang montage assigned"),A->FangMontage.Get());
 TestNotNull(TEXT("Mirrored Fang montage assigned"),A->FangAlternateMontage.Get());
 TestTrue(TEXT("Fang montages differ"),A->FangMontage!=A->FangAlternateMontage);
 if(auto* Anim=N->GetMesh()->GetAnimInstance())
 {
  A->PlayFangMontage(FRotator::ZeroRotator);
  TestTrue(TEXT("First Fang uses original montage"),Anim->Montage_IsPlaying(A->FangMontage));
  A->PlayFangMontage(FRotator::ZeroRotator);
  TestTrue(TEXT("Second Fang uses mirrored montage"),Anim->Montage_IsPlaying(A->FangAlternateMontage));
  A->PlayFangMontage(FRotator::ZeroRotator);
  TestTrue(TEXT("Third Fang alternates back"),Anim->Montage_IsPlaying(A->FangMontage));
  A->StopAutoAttack();
  TestFalse(TEXT("Stopping attacks stops cosmetic Fang montage"),Anim->Montage_IsPlaying(A->FangMontage));
 }
 else AddError(TEXT("Saved Ninja has no animation instance for Fang presentation"));
 FPlayerUpgradeRunState Empty;U->CaptureRunState(Empty);
 auto Grant=[&](FName Id){auto* Card=U->FindUpgradeDefinition(Id);TestNotNull(*Id.ToString(),Card);return U->AcquireUpgrade(Card);};
 TArray<AEnemyBase*> Enemies;
 auto Spawn=[&](FVector Pos,float HP){auto* E=World->SpawnActor<AEnemyBase>(Pos,FRotator::ZeroRotator,Params);E->ConfigureObjectiveEnemy(HP,EPlayerAttackSource::Other,nullptr,FLinearColor::White);E->GetHealthComponent()->RestoreCurrentHealth(HP);FScriptDelegate D;D.BindUFunction(E,TEXT("HandleDeath"));E->GetHealthComponent()->OnDeath.AddUnique(D);Enemies.Add(E);return E;};
 auto Reset=[&](){B->ClearProjectiles();for(auto* E:Enemies)if(IsValid(E))E->Destroy();Enemies.Reset();U->RestoreRunState(Empty);Grant(TEXT("ReturningFang"));};
 // Exercise the real saved arrival while Fang's independent scheduler has a target.
 Reset();Spawn(FVector(100,0,0),10000);
 auto* Arrival=N->SwapPresentation.Get();
 TestNotNull(TEXT("Saved Ninja arrival montage assigned"),Arrival->EntranceMontage.Get());
 Arrival->PrepareArrival();
 B->TickComponent(.2f,LEVELTICK_All,nullptr);
 TestFalse(TEXT("Fang waits for pending arrival"),B->Fang.IsValid());
 Arrival->PlayArrival();
 auto* EntranceAnim=N->GetMesh()->GetSingleNodeInstance();
 if(TestNotNull(TEXT("Saved arrival owns Ninja pose"),EntranceAnim))
 {
  B->TickComponent(.2f,LEVELTICK_All,nullptr);
  TestFalse(TEXT("Fang does not launch during arrival"),B->Fang.IsValid());
  A->PlayFangMontage(FRotator::ZeroRotator);A->PlayFangSlashMontage();
  Arrival->UpdateEntrance(.1f);
  TestNotNull(TEXT("Fang cannot interrupt arrival montage"),EntranceAnim->GetActiveInstanceForMontage(Arrival->EntranceMontage));
  TestFalse(TEXT("Throw does not play on arrival instance"),EntranceAnim->Montage_IsPlaying(A->FangMontage));
  TestFalse(TEXT("Burst does not play on arrival instance"),EntranceAnim->Montage_IsPlaying(A->FangSlashMontage));
  auto* Early=B->SpawnBlade(ENinjaProjectileKind::ReturningFang,FVector(0,0,50));Early->LaunchFang();
  TestTrue(TEXT("Direct Fang launch also respects arrival"),Early->IsActorBeingDestroyed());
  Arrival->UpdateSwapFreeze(10.f);
  Arrival->UpdateEntrance(10.f);
  B->TickComponent(.2f,LEVELTICK_All,nullptr);
  TestFalse(TEXT("Final arrival pose remains protected"),B->Fang.IsValid());
  Arrival->UpdateEntrance(0.f);
  B->TickComponent(.2f,LEVELTICK_All,nullptr);
  TestTrue(TEXT("Fang launches after arrival settles"),B->Fang.IsValid());
 }
 Arrival->FinishSwapFreeze();Reset();U->RestoreRunState(Empty);
 const FName OneTime[]={TEXT("FangTwin"),TEXT("RelentlessFang"),TEXT("FangResonance"),TEXT("FangDeadeye"),TEXT("FangAssist"),TEXT("FangSplinter"),TEXT("FangKillingEdge")};
 const FName Rare[]={TEXT("FangTwinFrequency"),TEXT("FangPressure"),TEXT("FangResonantReach"),TEXT("FangCriticalChance"),TEXT("FangAssistChance"),TEXT("FangSplinterPower"),TEXT("FangPursuitChance")};
 for(int32 i=0;i<7;++i)
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
 for(int32 i=0;i<7;++i)
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

 Reset();
 TestTrue(TEXT("Fang range halved"),FMath::IsNearlyEqual(A->GetEffectiveTargetingRange(),A->TargetingRange*.5f));
 auto* CloseEnemy=Spawn(FVector(30,0,0),100000);
 auto* Capped=B->SpawnBlade(ENinjaProjectileKind::ReturningFang,FVector(0,0,50));Capped->LaunchFang();
 const float ExpectedSpeed=.5f*A->GetEffectiveProjectileSpeed()*A->GetBaseAttackInterval()/A->GetEffectiveAttackInterval();
 TestTrue(TEXT("Fang flight speed halved"),FMath::IsNearlyEqual(Capped->Speed,ExpectedSpeed,.01f));
 // Artificially huge travel speed must not bypass the launch-rate limit.
 Capped->Speed=100000.f;const int32 InitialLaunches=B->FangLaunchCount;
 for(int32 i=0;i<20;++i)Capped->Tick(.01f);
 TestEqual(TEXT("Point-blank return waits before relaunch"),B->FangLaunchCount,InitialLaunches);
 TestTrue(TEXT("Waiting Fang has returned"),Capped->bReturning);
 for(int32 i=0;i<7;++i)Capped->Tick(.01f);
 TestEqual(TEXT("Relaunch allowed after quarter second"),B->FangLaunchCount,InitialLaunches+1);
 const int32 BeforeSustained=B->FangLaunchCount;
 for(int32 i=0;i<100;++i){Capped->Speed=100000.f;Capped->Tick(.01f);}
 TestTrue(TEXT("Sustained point-blank rate capped at four launches per second"),B->FangLaunchCount-BeforeSustained<=4);
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
 A->StopAutoAttack();
 Grant(TEXT("FangResonance"));const float BeforeBurst=E->GetHealthComponent()->GetCurrentHealth();
 B->FangReturned(P);TestEqual(TEXT("Canceled trips give no return burst"),E->GetHealthComponent()->GetCurrentHealth(),BeforeBurst);
 TestFalse(TEXT("Canceled return does not play slash"),N->GetMesh()->GetAnimInstance()->Montage_IsPlaying(A->FangSlashMontage));
 P->bFangHitThisTrip=true;B->FangReturned(P);
 TestNotNull(TEXT("Saved Fang slash montage assigned"),A->FangSlashMontage.Get());
 TestTrue(TEXT("Return burst plays slash montage"),N->GetMesh()->GetAnimInstance()->Montage_IsPlaying(A->FangSlashMontage));
 A->PlayFangMontage(FRotator::ZeroRotator);
 TestTrue(TEXT("Immediate relaunch preserves slash montage"),N->GetMesh()->GetAnimInstance()->Montage_IsPlaying(A->FangSlashMontage));
 TestEqual(TEXT("Return burst deals half Fang damage"),E->GetHealthComponent()->GetCurrentHealth(),BeforeBurst-50);
 TestEqual(TEXT("Burst does not count as direct Fang hit"),B->FangVictimHits.Num(),0);
 auto* BurstAnim=N->GetMesh()->GetAnimInstance();
 BurstAnim->Montage_SetPosition(A->FangSlashMontage,.4f);
 TestTrue(TEXT("Slash progressed before second burst"),BurstAnim->Montage_GetPosition(A->FangSlashMontage)>.3f);
 P->bFangHitThisTrip=true;B->FangReturned(P);
 TestTrue(TEXT("Second burst restarts slash"),BurstAnim->Montage_IsPlaying(A->FangSlashMontage));
 TestTrue(TEXT("Restart begins at zero"),FMath::IsNearlyZero(BurstAnim->Montage_GetPosition(A->FangSlashMontage)));
 TestEqual(TEXT("Restarted burst still deals damage once"),E->GetHealthComponent()->GetCurrentHealth(),BeforeBurst-100);

 // Saved pool offers Reach after Resonance, and its rank changes the real damage radius.
 auto Offers=U->RollUpgradeChoices(EUpgradeCategory::Ninja,100);
 TestTrue(TEXT("Resonant Reach present in eligible ordinary offers"),Offers.Contains(U->FindUpgradeDefinition(TEXT("FangResonantReach"))));
 Other=Spawn(FVector(400,0,0),100000);
 const float OutsideHealth=Other->GetHealthComponent()->GetCurrentHealth();
 P->bFangHitThisTrip=true;B->FangReturned(P);
 TestEqual(TEXT("Outside base burst radius"),Other->GetHealthComponent()->GetCurrentHealth(),OutsideHealth);
 for(int32 i=0;i<5;++i)TestTrue(TEXT("Reach rank acquired"),Grant(TEXT("FangResonantReach")));
 TestFalse(TEXT("Reach capped at five ranks"),Grant(TEXT("FangResonantReach")));
 P->bFangHitThisTrip=true;B->FangReturned(P);
 TestEqual(TEXT("Reach hits at 400cm after five ranks"),Other->GetHealthComponent()->GetCurrentHealth(),OutsideHealth-50);
 for(int32 i=0;i<10;++i)TestTrue(TEXT("Burst power rank acquired"),Grant(TEXT("FangBurstPower")));
 TestFalse(TEXT("Burst power capped at ten ranks"),Grant(TEXT("FangBurstPower")));
 P->bFangHitThisTrip=true;B->FangReturned(P);
 TestEqual(TEXT("Ten burst ranks triple burst damage"),Other->GetHealthComponent()->GetCurrentHealth(),OutsideHealth-200);

 // Deterministically exercise a kill followed by two nonlethal target hits.
 Reset();Grant(TEXT("FangKillingEdge"));
 auto* Edge=U->FindUpgradeDefinition(TEXT("FangKillingEdge"));const auto EdgeBalance=Edge->BalanceParameters;
 Edge->BalanceParameters.Add(TEXT("Chance"),1.f);
 E=Spawn(FVector(300,0,0),1);Other=Spawn(FVector(500,0,0),10000);Third=Spawn(FVector(700,0,0),10000);
 P=B->SpawnBlade(ENinjaProjectileKind::ReturningFang,FVector(300,0,45));P->Target=E;P->Damage=100;P->Speed=1000;
 P->Tick(.01f);
 TestTrue(TEXT("Kill starts pursuit"),E->IsDead()&&!P->bReturning&&P->Target==Other);
 P->SetActorLocation(Other->GetActorLocation()+FVector(0,0,45));P->Tick(.01f);
 TestTrue(TEXT("Nonlethal follow-up continues to second target"),!P->bReturning&&P->Target==Third);
 P->SetActorLocation(Third->GetActorLocation()+FVector(0,0,45));P->Tick(.01f);
 TestTrue(TEXT("Exactly two follow-ups then return"),P->bReturning&&P->LastHits.Num()==3);
 TestEqual(TEXT("First follow-up takes full Fang damage"),Other->GetHealthComponent()->GetCurrentHealth(),9900.f);
 TestEqual(TEXT("Second follow-up takes full Fang damage"),Third->GetHealthComponent()->GetCurrentHealth(),9900.f);
 Edge->BalanceParameters.Add(TEXT("Chance"),0.f);
 E=Spawn(FVector(300,200,0),1);P->Target=E;P->bReturning=false;P->bPursued=false;P->FangPursuitTargetsRemaining=0;
 P->SetActorLocation(E->GetActorLocation()+FVector(0,0,45));P->Tick(.01f);
 TestTrue(TEXT("Failed roll returns immediately"),P->bReturning);
 Edge->BalanceParameters=EdgeBalance;
 for(int32 i=0;i<5;++i)TestTrue(TEXT("Pursuit chance rank acquired"),Grant(TEXT("FangPursuitChance")));
 TestFalse(TEXT("Pursuit chance capped at five"),Grant(TEXT("FangPursuitChance")));
 TestTrue(TEXT("Pursuit chance reaches 65 percent"),FMath::IsNearlyEqual(.15f+FangBuild::Scaling(U,TEXT("FangPursuitChance"),.1f),.65f));
 FPlayerUpgradeRunState OldEdge;U->CaptureRunState(OldEdge);OldEdge.Levels[TEXT("FangKillingEdge")]=5;
 OldEdge.Levels.Remove(TEXT("FangPursuitChance"));OldEdge.AccumulatedMagnitudes.Add(TEXT("FangKillingEdge"),.75f);
 U->RestoreRunState(OldEdge);
 TestEqual(TEXT("Old Killing Edge becomes unlock"),U->GetUpgradeLevelById(TEXT("FangKillingEdge")),1);
 TestEqual(TEXT("Old extra ranks preserved as chance"),U->GetUpgradeLevelById(TEXT("FangPursuitChance")),4);
 FPlayerUpgradeRunState NewEdge;U->CaptureRunState(NewEdge);U->RestoreRunState(NewEdge);
 TestEqual(TEXT("Conversion is idempotent"),U->GetUpgradeLevelById(TEXT("FangPursuitChance")),4);
 TestEqual(TEXT("Conversion preserves mastery"),U->GetNinjaMasteryPoints(),OldEdge.NinjaMastery);
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
 TestEqual(TEXT("Farstrider max-distance damage"),B->FangHit(P,E),500.f);P->FangOutwardDistance=5000;
 TestEqual(TEXT("Farstrider distance bonus capped"),B->FangHit(P,E),500.f);
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
