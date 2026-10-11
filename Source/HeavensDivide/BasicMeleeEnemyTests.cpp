#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "MeleeEnemyBase.h"
#include "MontageMeleeEnemyBase.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "SurvivorPlayerController.h"
#include "HealthComponent.h"
#include "EnemyDeathComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBasicMeleeAttackTest, "HeavensDivide.Enemies.BasicMelee",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBasicMeleeAttackTest::RunTest(const FString& Parameters)
{
 UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());
 auto* Controller = World->SpawnActor<ASurvivorPlayerController>();
 auto* Player = World->SpawnActor<ACharacterBase>();
 Player->SetOwner(Controller);
 auto* Manager = Controller->GetCharacterManager();
 FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(), TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(Manager, Player);
 auto* HP = Controller->GetPlayerHealthComponent();
 // These attack-timing fixtures advance component timers, not world time.
 // Player hit-grace behavior is covered by Balance.FixedHealthPass.
 HP->DamageGracePeriod = 0.f;
 HP->RestoreCurrentHealth(100);
 auto Spawn = [&](UClass* EnemyClass = AMeleeEnemyBase::StaticClass())
 {
  FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Enemy = World->SpawnActor<AMeleeEnemyBase>(EnemyClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
  Enemy->ObservedCharacterManager = Manager;
  Enemy->CachedSurvivorController = Controller;
  Enemy->SetTarget(Player);
  Enemy->bDropsXP = false;
  Enemy->EnemyDeathComponent->DeathNiagaraSystem = nullptr;
  Enemy->EnemyDeathComponent->DeathSound = nullptr;
  Enemy->GetHealthComponent()->OnDeath.AddUniqueDynamic(Enemy, &AMeleeEnemyBase::HandleDeath);
  Player->SetActorLocation(FVector(100,0,0));
  return Enemy;
 };
 auto TickTimers = [&](float Delta) { ++GFrameCounter; World->GetTimerManager().Tick(Delta); };
 auto* Enemy = Spawn();
 const FVector Original = Enemy->GetMesh()->GetRelativeLocation();
 const FQuat OriginalRotation = Enemy->GetMesh()->GetRelativeRotation().Quaternion();
 Player->SetActorLocation(FVector(500,0,0));
 Enemy->UpdateEnemyBehavior(0.05f);
 TestTrue(TEXT("Enemy requests chase movement outside range"), Enemy->bHasDesiredMovementDirection);
 // Existing StopDistance is 150, while some BPs attack at 140: avoid stopping short.
 Enemy->AttackRange = 140;
 Player->SetActorLocation(FVector(145,0,0));
 Enemy->UpdateEnemyBehavior(0.05f);
 TestTrue(TEXT("Chase can reach the attack range"), Enemy->bHasDesiredMovementDirection);
 Player->SetActorLocation(FVector(100,0,0));
 Enemy->StartAttack();
 TestTrue(TEXT("Attack starts without an anim instance"), Enemy->bIsAttacking);
 Enemy->LungeStartTime -= 0.07;
 Enemy->UpdateAttackLunge();
 TestTrue(TEXT("Windup visibly rocks the mesh before damage"), !Enemy->GetMesh()->GetRelativeRotation().Quaternion().Equals(OriginalRotation));
 TestTrue(TEXT("Windup leans away from the target"), Enemy->GetMesh()->GetRelativeLocation().X < Original.X);
 Enemy->LungeStartTime += 0.07;
 TestFalse(TEXT("Chase stops during attack"), Enemy->bHasDesiredMovementDirection);
 TestFalse(TEXT("Locomotion animation is not paused"), Enemy->GetMesh()->bPauseAnims);
 TestNull(TEXT("Basic enemies have no AttackMontage property"), FindFProperty<FProperty>(AMeleeEnemyBase::StaticClass(), TEXT("AttackMontage")));
 TestNull(TEXT("Basic enemies have no notify hit function"), AMeleeEnemyBase::StaticClass()->FindFunctionByName(TEXT("PerformAttackHit")));
 TestNotNull(TEXT("Special enemies retain their montage property"), FindFProperty<FProperty>(AMontageMeleeEnemyBase::StaticClass(), TEXT("AttackMontage")));
 Enemy->StartAttack();
 TickTimers(0); TickTimers(0.10f);
 TestEqual(TEXT("No damage during windup"), HP->GetCurrentHealth(), 100.0f);
 TickTimers(0.11f);
 TestEqual(TEXT("Timed sphere hit applies normal damage once"), HP->GetCurrentHealth(), 90.0f);
 TestTrue(TEXT("Hit enters recovery"), Enemy->BasicAttackPhase == AMeleeEnemyBase::EBasicAttackPhase::Recovery);
 TestTrue(TEXT("Wobble peaks on the impact frame"), Enemy->GetMesh()->GetRelativeLocation().Equals(Original + FVector(Enemy->AttackLungeDistance,0,0)));
 Enemy->FinishAttackWindup();
 TestEqual(TEXT("Repeated impact cannot double hit"), HP->GetCurrentHealth(), 90.0f);
 Enemy->LungeStartTime -= 0.03;
 Enemy->UpdateAttackLunge();
 TestTrue(TEXT("Visual mesh lunges forward"), !Enemy->GetMesh()->GetRelativeLocation().Equals(Original));
 TestTrue(TEXT("Lunge leaves actor position unchanged"), Enemy->GetActorLocation().IsNearlyZero());
 Player->SetActorLocation(FVector(600,0,0));
 TickTimers(0.26f);
 TestFalse(TEXT("Recovery releases attack state"), Enemy->bIsAttacking);
 TestTrue(TEXT("Mesh returns exactly to baseline"), Enemy->GetMesh()->GetRelativeLocation() == Original);
 TestTrue(TEXT("Mesh rotation returns to baseline"), Enemy->GetMesh()->GetRelativeRotation().Quaternion().Equals(OriginalRotation));
 TestTrue(TEXT("Chasing resumes after recovery"), Enemy->bHasDesiredMovementDirection);
 Player->SetActorLocation(FVector(100,0,0));
 Enemy->StartAttack();
 TestFalse(TEXT("Existing cooldown prevents immediate reattack"), Enemy->bIsAttacking);
 Enemy->StopEnemyBehavior();

 for (int32 Repeat=0; Repeat<3; ++Repeat)
 {
  Enemy->NextAttackStartTime=0;
  Enemy->StartAttack();
  Player->SetActorLocation(FVector(1000,0,0));
  Enemy->FinishAttackWindup();
  Enemy->LungeStartTime -= 0.03;
  Enemy->UpdateAttackLunge();
  Enemy->FinishAttackRecovery();
  TestTrue(TEXT("Repeated misses do not accumulate mesh offset"), Enemy->GetMesh()->GetRelativeLocation() == Original);
  TestTrue(TEXT("Repeated misses do not accumulate mesh tilt"), Enemy->GetMesh()->GetRelativeRotation().Quaternion().Equals(OriginalRotation));
  Player->SetActorLocation(FVector(100,0,0));
 }
 TestEqual(TEXT("Escaping during windup avoids damage"), HP->GetCurrentHealth(),90.0f);
 Enemy->StopEnemyBehavior();
 auto* Behind = Spawn(); Behind->StartAttack(); Player->SetActorLocation(FVector(-50,0,0)); Behind->FinishAttackWindup();
 TestEqual(TEXT("Player behind enemy is outside the forward hit"),HP->GetCurrentHealth(),90.0f);
 Behind->StopEnemyBehavior();
 auto* Killed = Spawn(); Killed->StartAttack(); Killed->GetHealthComponent()->ApplyDamage(1000);
 Killed->FinishAttackWindup();
 TestEqual(TEXT("Death during windup prevents delayed damage"),HP->GetCurrentHealth(),90.0f);
 TestFalse(TEXT("Death clears windup timer"),World->GetTimerManager().IsTimerActive(Killed->WindupTimer));
 auto* Lunger = Spawn(); Lunger->StartAttack(); Lunger->FinishAttackWindup(); Lunger->LungeStartTime-=0.03; Lunger->UpdateAttackLunge();
 const FVector LungerBaseline=Lunger->PreAttackMeshLocation;
 Lunger->GetHealthComponent()->ApplyDamage(1000);
 TestTrue(TEXT("Death during lunge restores baseline before dissolve"),Lunger->GetMesh()->GetRelativeLocation()==LungerBaseline);
 TestTrue(TEXT("Death restores the mesh rotation before dissolve"),Lunger->GetMesh()->GetRelativeRotation().Quaternion().Equals(Lunger->PreAttackMeshRotation));
 TestFalse(TEXT("Death clears lunge timer"),World->GetTimerManager().IsTimerActive(Lunger->LungeTimer));
 auto* Lost = Spawn(); Lost->StartAttack(); Lost->FinishAttackWindup(); Lost->SetTarget(nullptr);
 TestFalse(TEXT("Target removal cancels attack immediately"), Lost->bIsAttacking);
 TestFalse(TEXT("Target removal clears recovery"),World->GetTimerManager().IsTimerActive(Lost->RecoveryTimer));
 auto* Disabled = Spawn(); Disabled->SetGameplaySuspended(true); Disabled->StartAttack();
 TestFalse(TEXT("Suspended enemy cannot attack"),Disabled->bIsAttacking);
 auto* First=Spawn(); auto* Second=Spawn(); First->StartAttack(); Second->StartAttack();
 First->SetGameplaySuspended(true);
 TestTrue(TEXT("Other enemy's independent windup remains active"),World->GetTimerManager().IsTimerActive(Second->WindupTimer));
 Second->StopEnemyBehavior();

 // Imported species use different mesh orientations/scales. Apply wobble around
 // actor axes and restore the complete authored pose on cancellation.
 auto* Oriented = Spawn();
 Oriented->SetActorRotation(FRotator(0,90,0));
 Oriented->SetActorScale3D(FVector(1.5f));
 Oriented->GetMesh()->SetRelativeRotation(FRotator(10,-90,5));
 Oriented->GetMesh()->SetRelativeScale3D(FVector(0.8f));
 const FTransform MeshBaseline = Oriented->GetMesh()->GetRelativeTransform();
 const FTransform ActorBaseline = Oriented->GetActorTransform();
 Player->SetActorLocation(FVector(0,100,0));
 Oriented->StartAttack(); Oriented->LungeStartTime-=0.07; Oriented->UpdateAttackLunge();
 TestFalse(TEXT("Authored mesh orientation still wobbles"),Oriented->GetMesh()->GetRelativeRotation().Quaternion().Equals(MeshBaseline.GetRotation()));
 TestTrue(TEXT("Wobble preserves actor/collision transform"),Oriented->GetActorTransform().Equals(ActorBaseline));
 Oriented->SetGameplaySuspended(true);
 TestTrue(TEXT("Cancelling windup restores authored mesh transform"),Oriented->GetMesh()->GetRelativeTransform().Equals(MeshBaseline));
 TestFalse(TEXT("Cancelling windup clears the motion timer"),World->GetTimerManager().IsTimerActive(Oriented->LungeTimer));

 auto* ShortRecovery = Spawn(); ShortRecovery->AttackRecovery=0.01f;
 ShortRecovery->StartAttack(); ShortRecovery->FinishAttackWindup();
 ShortRecovery->LungeStartTime-=0.02; ShortRecovery->UpdateAttackLunge();
 TestFalse(TEXT("Motion fits a shortened recovery"),ShortRecovery->bLungeActive);
 TestTrue(TEXT("Short recovery restores both location and rotation"),ShortRecovery->GetMesh()->GetRelativeTransform().Equals(FTransform(ShortRecovery->PreAttackMeshRotation,ShortRecovery->PreAttackMeshLocation,ShortRecovery->GetMesh()->GetRelativeScale3D())));
 ShortRecovery->StopEnemyBehavior();

 UClass* SlowClass=LoadClass<AMeleeEnemyBase>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyBear.BP_EnemyBear_C"));
 UClass* FastClass=LoadClass<AMeleeEnemyBase>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyGoblinCrawler.BP_EnemyGoblinCrawler_C"));
 if (TestNotNull(TEXT("Slow melee Blueprint loads"),SlowClass) && TestNotNull(TEXT("Fast melee Blueprint loads"),FastClass))
 {
  auto* Slow=Spawn(SlowClass); auto* Fast=Spawn(FastClass);
  const auto* Speed=FindFProperty<FFloatProperty>(AEnemyBase::StaticClass(),TEXT("MoveSpeed"));
  TestTrue(TEXT("Crawler chases faster than bear"),Speed->GetPropertyValue_InContainer(Fast)>Speed->GetPropertyValue_InContainer(Slow));
  TestTrue(TEXT("Faster chaser has a longer windup"),Fast->AttackWindup>Slow->AttackWindup);
  TestTrue(TEXT("Slower chaser repeats its attacks faster"),Slow->AttackInterval<Fast->AttackInterval);
  HP->RestoreCurrentHealth(100);
  Slow->StartAttack(); Fast->StartAttack(); TickTimers(0);
  TickTimers(Slow->AttackWindup+0.01f);
  TestEqual(TEXT("Slow chaser hits first after entering range"),HP->GetCurrentHealth(),100-Slow->AttackDamage);
  TestTrue(TEXT("Fast chaser is still winding up"),Fast->BasicAttackPhase==AMeleeEnemyBase::EBasicAttackPhase::Windup);
  TickTimers(Fast->AttackWindup-Slow->AttackWindup+0.01f);
  TestEqual(TEXT("Fast chaser only hits after its longer windup"),HP->GetCurrentHealth(),100-Slow->AttackDamage-Fast->AttackDamage);
  Slow->StopEnemyBehavior(); Fast->StopEnemyBehavior();
 }
 World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
 return true;
}
#endif
