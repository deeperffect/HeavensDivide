#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "EncounterWall.h"
#include "SurvivalFieldObjective.h"
#include "EnemySpawner.h"
#include "EnemySpawnArea.h"
#include "CharacterBase.h"
#include "SurvivorPlayerController.h"
#include "ExperienceComponent.h"
#include "HealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "CombatGround.h"
#include "HealingPickup.h"
#include "MagnetPickup.h"
#include "MinimapMarkerComponent.h"
#include "MinimapRegistrySubsystem.h"
#include "BloodShrine.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayPolishTest,"HeavensDivide.Encounters.FieldObjectives",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameplayPolishTest::RunTest(const FString&)
{
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());
 auto* PC=W->SpawnActor<ASurvivorPlayerController>();
 auto* Player=W->SpawnActor<ACharacterBase>();PC->Possess(Player);
 Player->SetCharacterMode(ECharacterMode::Active);
 auto* Mesh=Player->GetMesh();Mesh->SetComponentTickEnabled(true);Mesh->ResumeClothingSimulation();
 Player->SetMenuSimulationPaused(true);Player->SetMenuSimulationPaused(true);
 TestFalse(TEXT("Menu stops animation graph and hair ticks"),Mesh->IsComponentTickEnabled());
 TestTrue(TEXT("Menu suspends separate cloth simulation"),Mesh->IsClothingSimulationSuspended());
 Player->SetMenuSimulationPaused(false);
 TestTrue(TEXT("Repeated pause preserves original mesh tick state"),Mesh->IsComponentTickEnabled());
 TestFalse(TEXT("Menu restores active cloth"),Mesh->IsClothingSimulationSuspended());
 Player->SetCharacterMode(ECharacterMode::Inactive);Player->SetMenuSimulationPaused(true);Player->SetMenuSimulationPaused(false);
 TestTrue(TEXT("Hidden character cloth stays suspended"),Mesh->IsClothingSimulationSuspended());

 FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 for (auto Source:{EPlayerAttackSource::Samurai,EPlayerAttackSource::Ninja,EPlayerAttackSource::Other})
 {
  auto* Wall=W->SpawnActor<AEncounterWall>(FVector(1000,0,120),FRotator::ZeroRotator,P);
  Wall->DispatchBeginPlay();Wall->SetSealed(true);
  TestEqual(TEXT("Closed wall blocks player movement"),Wall->Barrier->GetCollisionResponseToChannel(ECC_Pawn),ECR_Block);
  TestTrue(TEXT("Every character damage source can chip wall"),Wall->ApplyPlayerDamage(20,Source));
  TestEqual(TEXT("Wall HP remains fixed"),Wall->GetHealthComponent()->GetCurrentHealth(),40.f);
  Wall->ApplyPlayerDamage(40,Source);
  TestTrue(TEXT("Destroyed wall opens escape immediately"),Wall->IsDead() && !Wall->GetActorEnableCollision());
  TestFalse(TEXT("Broken wall cannot pay out or take repeated damage"),Wall->ApplyPlayerDamage(40,Source));
  Wall->Destroy();
 }
 auto* Zone=W->SpawnActor<ASurvivalFieldObjective>();Zone->InitializeHoldZone();
 TestTrue(TEXT("Circle boundary included"),Zone->ContainsPlayer(FVector(800,0,100)));
 TestFalse(TEXT("Outside circle excluded"),Zone->ContainsPlayer(FVector(801,0,100)));
 TestFalse(TEXT("Different floor excluded"),Zone->ContainsPlayer(FVector(0,0,500)));
 Zone->AdvanceHold(10,true);Zone->AdvanceHold(8,false);
 TestEqual(TEXT("Leaving pauses without erasing progress"),Zone->GetProgress(),10.f);
 Zone->AdvanceHold(-100,true);TestEqual(TEXT("Negative delta rejected"),Zone->GetProgress(),10.f);
 auto* XP=PC->GetExperienceComponent();const int32 BeforeLevel=XP->GetCurrentLevel();
 Zone->AdvanceHold(10,true);TestTrue(TEXT("20 seconds completes objective"),Zone->IsComplete());
 TestTrue(TEXT("Completion pays XP"),XP->GetCurrentLevel()>BeforeLevel);
 const int32 PaidLevel=XP->GetCurrentLevel(),PaidXP=XP->GetCurrentXP();
 Zone->AdvanceHold(100,true);TestEqual(TEXT("Repeated completion cannot pay levels twice"),XP->GetCurrentLevel(),PaidLevel);
 TestEqual(TEXT("Repeated completion cannot pay XP twice"),XP->GetCurrentXP(),PaidXP);

 auto* Floor=W->SpawnActor<AActor>();auto* Ground=NewObject<UBoxComponent>(Floor);
 Floor->SetRootComponent(Ground);Ground->SetBoxExtent(FVector(5000,5000,10));Ground->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
 Ground->SetCollisionResponseToAllChannels(ECR_Block);Ground->RegisterComponent();Floor->SetActorLocation(FVector(0,0,-10));
 auto* Bounds=W->SpawnActor<AEnemySpawnArea>();Bounds->FindComponentByClass<UBoxComponent>()->SetBoxExtent(FVector(2000,2000,500));
 auto* Spawner=W->SpawnActor<AEnemySpawner>();
 FindFProperty<FObjectProperty>(AEnemySpawner::StaticClass(),TEXT("ActiveSpawnArea"))->SetObjectPropertyValue_InContainer(Spawner,Bounds);
 FVector Found;
 TestTrue(TEXT("Encounter accepts normal floor inside spawn bounds"),Spawner->ValidateEncounterLocation(FVector(800,800,120),AEncounterWall::StaticClass(),Found));
 TestFalse(TEXT("Encounter rejects floor beyond arena even when geometry exists"),Spawner->ValidateEncounterLocation(FVector(3000,0,120),AEncounterWall::StaticClass(),Found));
 TestFalse(TEXT("Encounter rejects missing ground"),Spawner->ValidateEncounterLocation(FVector(7000,0,120),AEncounterWall::StaticClass(),Found));
 // A live enemy blocks the ground trace channel but must not hide the floor.
 auto* Blocker=W->SpawnActor<AEncounterWall>(FVector(550,0,120),FRotator::ZeroRotator,P);
 Blocker->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Block);
 FVector Projected;
 TestTrue(TEXT("Ground can be found through a transient enemy"),Spawner->ProjectEncounterGround(FVector(550,0,120),Projected));
 TestTrue(TEXT("Projection reaches floor instead of enemy top"),FMath::IsNearlyZero(Projected.Z,1.f));
 auto* Trap=W->SpawnActor<ASurvivalFieldObjective>(FVector(0,0,5),FRotator::ZeroRotator,P);
 TestTrue(TEXT("Complete wall ring can spawn with an enemy over its floor"),Trap->InitializeWallTrap(Spawner));
 TestEqual(TEXT("Wall trap has doubled radius"),Trap->Radius,1600.f);
 int32 WallCount=0;
 // AI possession can replace a wall's Owner with its controller.
 for(TActorIterator<AEncounterWall> It(W);It;++It) if(*It!=Blocker && !It->IsActorBeingDestroyed()) ++WallCount;
 TestEqual(TEXT("Larger trap retains a sealed ring"),WallCount,51);
 Trap->DispatchBeginPlay();
 Trap->Tick(9.99f);
 TestFalse(TEXT("Wall trap lasts until ten gameplay seconds"),Trap->IsActorBeingDestroyed());
 Trap->Tick(.02f);
 TestTrue(TEXT("Wall trap expires after ten gameplay seconds"),Trap->IsActorBeingDestroyed());
 int32 RemainingWalls=0;
 for(TActorIterator<AEncounterWall> It(W);It;++It) if(*It!=Blocker && !It->IsActorBeingDestroyed()) ++RemainingWalls;
 TestEqual(TEXT("Expiry removes all wall segments"),RemainingWalls,0);
 TestFalse(TEXT("Objects outside combat bounds are rejected"),CombatGround::IsInsideArena(W,FVector(2100,0,0),60));
 const FVector Clamped=CombatGround::ClampInsideArena(W,FVector(3000,3000,100),90);
 TestTrue(TEXT("Scattered loot stays fully inside bounds"),CombatGround::IsInsideArena(W,Clamped,90));
 Bounds->SetActorScale3D(FVector(.5,.5,1));
 TestFalse(TEXT("Footprint padding uses world centimeters on scaled bounds"),CombatGround::IsInsideArena(W,FVector(950,0,0),90));
 Bounds->SetActorScale3D(FVector::OneVector);
 auto* Registry=W->GetSubsystem<UMinimapRegistrySubsystem>();const int32 BeforeMarkers=Registry->GetMarkers().Num();
 auto* Heal=W->SpawnActor<AHealingPickup>();auto* Magnet=W->SpawnActor<AMagnetPickup>();
 Heal->DispatchBeginPlay();Magnet->DispatchBeginPlay();
 TestEqual(TEXT("Heals and magnets register minimap markers"),Registry->GetMarkers().Num(),BeforeMarkers+2);
 TestEqual(TEXT("Heal marker is distinct"),Heal->FindComponentByClass<UMinimapMarkerComponent>()->GetMarkerType(),EMinimapMarkerType::HealingPickup);
 TestEqual(TEXT("Magnet marker is distinct"),Magnet->FindComponentByClass<UMinimapMarkerComponent>()->GetMarkerType(),EMinimapMarkerType::MagnetPickup);
 Heal->Destroy();Magnet->Destroy();
 TestEqual(TEXT("Removed pickups leave no stale markers"),Registry->GetMarkers().Num(),BeforeMarkers);
 auto* Shrine=W->SpawnActor<ABloodShrine>();Shrine->ShrineState=EBloodShrineState::Active;
 ++GFrameCounter;W->GetTimerManager().Tick(0.f); // Prime the fixture's timer manager before scheduling.
 Shrine->ResultDisplayDuration=.01f;Shrine->SucceedChallenge();
 TestTrue(TEXT("Completed shrine disappears immediately"),Shrine->IsHidden());
 TestFalse(TEXT("Completed shrine stops blocking movement"),Shrine->GetActorEnableCollision());
 ++GFrameCounter;W->GetTimerManager().Tick(.05f);
 TestTrue(TEXT("Shrine requests its reward before removal"),Shrine->bRewardRequested);
 TestTrue(TEXT("Completed shrine is removed after rewarding"),Shrine->IsActorBeingDestroyed());
 W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif
