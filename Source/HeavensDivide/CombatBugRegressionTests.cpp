#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RunReportSubsystem.h"
#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "AutoAttackComponent.h"
#include "AttackProjectileBase.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "NinjaBuildComponent.h"
#include "NinjaBuildProjectile.h"
#include "SamuraiBladeWave.h"
#include "SamuraiIaijutsu.h"
#include "EnemyBase.h"
#include "HealthComponent.h"
#include "LevelAudioSubsystem.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundClass.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/UnrealType.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatBugRegressionTest,"HeavensDivide.Combat.MenuBugRegressions",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCombatBugRegressionTest::RunTest(const FString&)
{
 auto* GI=NewObject<UGameInstance>(GEngine); GI->InitializeStandalone();
 auto* World=GI->GetWorld();World->InitializeActorsForPlay(FURL());
 auto* Class=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 auto* PC=World->SpawnActor<ASurvivorPlayerController>(Class);PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
 auto* Samurai=World->SpawnActor<ASamuraiCharacter>();auto* Ninja=World->SpawnActor<ANinjaCharacter>();
 Samurai->SetOwner(PC);Ninja->SetOwner(PC);
 auto* Manager=PC->GetCharacterManager();
 auto SetParty=[&](const TCHAR* Name,ACharacterBase* Value){FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),Name)->SetObjectPropertyValue_InContainer(Manager,Value);};
 SetParty(TEXT("SamuraiCharacter"),Samurai);SetParty(TEXT("NinjaCharacter"),Ninja);SetParty(TEXT("ActiveCharacter"),Ninja);
 Ninja->SetCharacterMode(ECharacterMode::Active); Samurai->SetCharacterMode(ECharacterMode::Inactive);
 auto* Report=GI->GetSubsystem<URunReportSubsystem>();Report->BeginRun(PC);
 auto* Build=Ninja->FindComponentByClass<UNinjaBuildComponent>();
 auto Enemy=[&]() {auto* E=World->SpawnActor<AEnemyBase>(FVector(10000,0,0),FRotator::ZeroRotator);E->DispatchBeginPlay();E->GetHealthComponent()->SetMaxHealthPreservePercent(1000);E->GetHealthComponent()->RestoreCurrentHealth(1000);return E;};
 auto Damage=[&](FName Source) {auto* Row=Report->DamageRows().FindByPredicate([&](const auto& R){return R.Source==Source;});return Row?Row->Damage:0.;};
 auto* Wheel=World->SpawnActor<ANinjaBuildProjectile>();Wheel->Build=Build;Wheel->Damage=25;Wheel->Kind=ENinjaProjectileKind::GreatShuriken;
 Wheel->ShurikenHit(Enemy(),false);
 TestEqual(TEXT("Actual shuriken contact credits its weapon"),Damage(TEXT("GreatShuriken")),25.);
 TestEqual(TEXT("Shuriken does not become ordinary kunai"),Damage(TEXT("NinjaAttack")),0.);
 Wheel->bAssistProjectile=true;Wheel->AssistDamageSource=TEXT("TagTeam");Wheel->ShurikenHit(Enemy(),false);
 TestEqual(TEXT("Tag Team keeps delayed shuriken damage"),Damage(TEXT("TagTeam")),25.);
 auto* Fang=World->SpawnActor<ANinjaBuildProjectile>();Fang->Build=Build;Fang->Damage=30;
 Build->FangHit(Fang,Enemy());TestEqual(TEXT("Fang contact credits its weapon"),Damage(TEXT("ReturningFang")),30.);
 Fang->bAssistProjectile=true;Fang->AssistDamageSource=TEXT("CrescentAssist");Build->FangHit(Fang,Enemy());
 TestEqual(TEXT("Route assist preserves trigger instead of Tag Team"),Damage(TEXT("CrescentAssist")),30.);
 auto* Slash=World->SpawnActor<ASamuraiIaijutsu>();Slash->SourceUpgrades=PC->GetPlayerUpgrades();Slash->HitDamage=20;
 Slash->HitEnemy(Enemy());TestEqual(TEXT("Iaijutsu credits dash slash"),Damage(TEXT("Iaijutsu")),20.);
 Slash->bAssistAttack=true;Slash->AssistDamageSource=TEXT("TagTeam");Slash->HitEnemy(Enemy());
 auto* Wave=World->SpawnActor<ASamuraiBladeWave>();Wave->SourceSamurai=Samurai;Wave->SourceUpgrades=PC->GetPlayerUpgrades();Wave->Damage=15;
 Wave->HandleOverlap(nullptr,Enemy(),nullptr,0,false,FHitResult());TestEqual(TEXT("Crescent credits moving wave"),Damage(TEXT("Crescent")),15.);
 Wave->DamageSource=TEXT("TagTeam");Wave->HandleOverlap(nullptr,Enemy(),nullptr,0,false,FHitResult());
 TestEqual(TEXT("Tag Team aggregates different assistants once"),Damage(TEXT("TagTeam")),60.);
 auto* Kunai=World->SpawnActor<AAttackProjectileBase>();Kunai->DispatchBeginPlay();Kunai->InitializeProjectile(Ninja,FVector::ForwardVector,10,1000);
 Kunai->HandleProjectileOverlap(nullptr,Enemy(),nullptr,0,false,FHitResult());TestEqual(TEXT("Base Ninja still credits kunai"),Damage(TEXT("NinjaAttack")),10.);
 auto* Barrage=World->SpawnActor<AAttackProjectileBase>();Barrage->DispatchBeginPlay();Barrage->InitializeProjectile(Ninja,FVector::ForwardVector,10,1000);Barrage->bBarrageProjectile=true;
 Barrage->HandleProjectileOverlap(nullptr,Enemy(),nullptr,0,false,FHitResult());TestEqual(TEXT("Barrage credits volleys"),Damage(TEXT("Barrage")),10.);
 auto* Sword=World->SpawnActor<AAttackProjectileBase>();Sword->DispatchBeginPlay();Sword->InitializeProjectile(Samurai,FVector::ForwardVector,10,1000);
 Sword->HandleProjectileOverlap(nullptr,Enemy(),nullptr,0,false,FHitResult());TestEqual(TEXT("Blood base strike retains Samurai category"),Damage(TEXT("SamuraiAttack")),10.);
 // A large movement step must hit the player between its start and end positions.
 Ninja->DispatchBeginPlay();
 Ninja->SetActorLocation(FVector(0,0,200));PC->GetPlayerHealthComponent()->SetMaxHealthPreservePercent(100);PC->GetPlayerHealthComponent()->RestoreCurrentHealth(100);
 auto* FireClass=LoadClass<AAttackProjectileBase>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/BP_FireballProjectile.BP_FireballProjectile_C"));
 TestNotNull(TEXT("Shipped ranged fireball loads"),FireClass);
 auto* Fire=World->SpawnActor<AAttackProjectileBase>(FireClass,FVector(-500,0,200),FRotator::ZeroRotator);Fire->DispatchBeginPlay();
 Fire->InitializeProjectile(Enemy(),FVector::ForwardVector,10,5000,EProjectileTargetType::ActivePlayer);
 TestTrue(TEXT("Shipped Blueprint sweeps its sphere root"),Fire->GetRootComponent()==Fire->CollisionComponent && Fire->ProjectileMovement->UpdatedComponent==Fire->CollisionComponent);
 AddInfo(FString::Printf(TEXT("Fireball start=%s velocity=%s sweep=%d sphereOverlap=%d pawnOverlap=%d sphereMobility=%d simulation=%d physicsScene=%d"),*Fire->GetActorLocation().ToString(),*Fire->ProjectileMovement->Velocity.ToString(),Fire->ProjectileMovement->bSweepCollision,Fire->CollisionComponent->GetGenerateOverlapEvents(),Ninja->GetCapsuleComponent()->GetGenerateOverlapEvents(),int32(Fire->CollisionComponent->Mobility),Fire->ProjectileMovement->bSimulationEnabled,World->GetPhysicsScene()!=nullptr));
 // Primitive overlap delegates are intentionally suppressed before world BeginPlay.
 World->SetBegunPlay(true);
 Fire->ProjectileMovement->TickComponent(.2f,LEVELTICK_All,nullptr);
 AddInfo(FString::Printf(TEXT("Fireball end=%s resolved=%d capsule=%s player0=%s"),*Fire->GetActorLocation().ToString(),Fire->bImpactResolved,*Ninja->GetCapsuleComponent()->GetComponentLocation().ToString(),*GetNameSafe(World->GetFirstPlayerController())));
 TestEqual(TEXT("Fast fireball removes player health across a long frame"),PC->GetPlayerHealthComponent()->GetCurrentHealth(),90.f);
 auto* Audio=World->GetSubsystem<ULevelAudioSubsystem>();
 if(TestNotNull(TEXT("Gameplay audio subsystem exists"),Audio))
 {
  TestNotNull(TEXT("Ambience is cooked-reference reachable"),Audio->LevelOneAmbience.Get());
  if(Audio->LevelOneAmbience) {TestTrue(TEXT("Ambience loops"),Audio->LevelOneAmbience->bLooping);TestTrue(TEXT("Ambience uses effects volume"),Audio->LevelOneAmbience->SoundClassObject==Audio->SoundEffectsClass);}
  TestTrue(TEXT("Music and effects have independent classes"),Audio->MusicClass && Audio->SoundEffectsClass && Audio->MusicClass!=Audio->SoundEffectsClass);
 }
 GI->Shutdown();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
