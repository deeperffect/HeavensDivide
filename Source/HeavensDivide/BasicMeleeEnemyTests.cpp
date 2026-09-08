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
 HP->RestoreCurrentHealth(100);
 auto Spawn = [&]()
 {
  FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Enemy = World->SpawnActor<AMeleeEnemyBase>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
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
 Enemy->FinishAttackWindup();
 TestEqual(TEXT("Repeated impact cannot double hit"), HP->GetCurrentHealth(), 90.0f);
 Enemy->LungeStartTime -= 0.06;
 Enemy->UpdateAttackLunge();
 TestTrue(TEXT("Visual mesh lunges forward"), !Enemy->GetMesh()->GetRelativeLocation().Equals(Original));
 TestTrue(TEXT("Lunge leaves actor position unchanged"), Enemy->GetActorLocation().IsNearlyZero());
 Player->SetActorLocation(FVector(600,0,0));
 TickTimers(0.26f);
 TestFalse(TEXT("Recovery releases attack state"), Enemy->bIsAttacking);
 TestTrue(TEXT("Mesh returns exactly to baseline"), Enemy->GetMesh()->GetRelativeLocation() == Original);
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
  Enemy->LungeStartTime -= 0.06;
  Enemy->UpdateAttackLunge();
  Enemy->FinishAttackRecovery();
  TestTrue(TEXT("Repeated misses do not accumulate mesh offset"), Enemy->GetMesh()->GetRelativeLocation() == Original);
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
 auto* Lunger = Spawn(); Lunger->StartAttack(); Lunger->FinishAttackWindup(); Lunger->LungeStartTime-=0.06; Lunger->UpdateAttackLunge();
 const FVector LungerBaseline=Lunger->PreAttackMeshLocation;
 Lunger->GetHealthComponent()->ApplyDamage(1000);
 TestTrue(TEXT("Death during lunge restores baseline before dissolve"),Lunger->GetMesh()->GetRelativeLocation()==LungerBaseline);
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
 World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
 return true;
}
#endif
