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
#include "Components/StaticMeshComponent.h"
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
  Wall->SetRevealProgress(1.f);
  TestFalse(TEXT("The reveal cannot restore a broken wall's collision"),Wall->GetActorEnableCollision());
  TestFalse(TEXT("Broken wall cannot pay out or take repeated damage"),Wall->ApplyPlayerDamage(40,Source));
  Wall->Destroy();
 }
 auto* Zone=W->SpawnActor<ASurvivalFieldObjective>();Zone->InitializeHoldZone();
 auto* CircleRing=Cast<UStaticMeshComponent>(FindFProperty<FObjectProperty>(ASurvivalFieldObjective::StaticClass(),TEXT("Ring"))->GetObjectPropertyValue_InContainer(Zone));
 TestFalse(TEXT("Unactivated survival circle hides its progress ring"),CircleRing->IsVisible());
 TestTrue(TEXT("Circle boundary included"),Zone->ContainsPlayer(FVector(1200,0,100)));
 TestFalse(TEXT("Outside circle excluded"),Zone->ContainsPlayer(FVector(1201,0,100)));
 TestFalse(TEXT("Different floor excluded"),Zone->ContainsPlayer(FVector(0,0,500)));
 Zone->AdvanceHold(10,true);TestEqual(TEXT("Walking inside does not start the circle"),Zone->GetProgress(),0.f);
 Zone->Interact_Implementation(Player);
 TestTrue(TEXT("Interacting reveals the survival progress circle"),CircleRing->IsVisible());
 TestTrue(TEXT("Circle interaction claims the objective"),PC->GetActiveObjective()==Zone);
 Zone->AdvanceHold(10,true);Zone->AdvanceHold(8,false);
 TestEqual(TEXT("Leaving drains progress at the survival rate"),Zone->GetProgress(),2.f);
 Zone->AdvanceHold(-100,true);TestEqual(TEXT("Negative delta rejected"),Zone->GetProgress(),2.f);
 Zone->AdvanceHold(8,false);TestEqual(TEXT("Progress drains no lower than zero"),Zone->GetProgress(),0.f);
 Zone->AdvanceHold(10,true);
 auto* XP=PC->GetExperienceComponent();const int32 BeforeLevel=XP->GetCurrentLevel();
 Zone->AdvanceHold(10,true);TestTrue(TEXT("20 seconds completes objective"),Zone->IsComplete());
 TestFalse(TEXT("Circle completion releases the objective"),PC->IsAnyObjectiveActive());
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
 TestFalse(TEXT("Wall event has no orange ring"),Cast<UStaticMeshComponent>(FindFProperty<FObjectProperty>(ASurvivalFieldObjective::StaticClass(),TEXT("Ring"))->GetObjectPropertyValue_InContainer(Trap))->IsVisible());
 Trap->DispatchBeginPlay();
 for(TActorIterator<AEncounterWall> It(W);It;++It) if(*It!=Blocker) It->DispatchBeginPlay();
 auto VisibleRocks=[&]() { int32 N=0;for(TActorIterator<AEncounterWall> It(W);It;++It) if(*It!=Blocker && It->Stone->IsVisible()) ++N;return N; };
 Trap->Tick(1.5f);
 TestEqual(TEXT("Rocks stay buried through the initial warning"),VisibleRocks(),0);
 Trap->Tick(.1f);
 const int32 Early=VisibleRocks();
 TestTrue(TEXT("First rocks rise before the rest of the ring"),Early>0 && Early<51);
 AEncounterWall* LaterRock=nullptr;
 for(TActorIterator<AEncounterWall> It(W);It;++It)
    if(*It!=Blocker && FMath::IsNearlyEqual(float(It->GetActorRotation().Yaw),20*360.f/51.f,.1f)) { LaterRock=*It;break; }
 TestNotNull(TEXT("A later rock in the wave is available"),LaterRock);
 const FVector PlayerPosition=Player->GetActorLocation();
 if(LaterRock)Player->SetActorLocation(LaterRock->GetActorLocation());
 Trap->Tick(.4f);
 if(LaterRock)TestEqual(TEXT("Later rocks do not rise through the player"),LaterRock->Barrier->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
 Player->SetActorLocation(PlayerPosition);
 TestTrue(TEXT("Wave progresses around the ring"),VisibleRocks()>Early && VisibleRocks()<51);
 Trap->Tick(.5f);
 TestEqual(TEXT("Whole rock wall rises in under one second"),VisibleRocks(),51);
 TSet<UStaticMesh*> Variants;
 for(TActorIterator<AEncounterWall> It(W);It;++It) if(*It!=Blocker)
 {
  Variants.Add(It->Stone->GetStaticMesh());
  TestTrue(TEXT("Every rock remains an active player barrier"),It->Barrier->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);
  TestTrue(TEXT("Rock base is embedded in the floor"),FMath::IsNearlyEqual(float(It->Stone->Bounds.GetBox().Min.Z),-8.f,1.f));
 }
 TestEqual(TEXT("Ring uses all six authored rock meshes"),Variants.Num(),6);
 Trap->Tick(7.5f);
 TestFalse(TEXT("Wall trap survives the former ten-second expiry"),Trap->IsActorBeingDestroyed());
 Trap->Tick(9.99f);
 TestFalse(TEXT("Wall trap lasts until twenty gameplay seconds"),Trap->IsActorBeingDestroyed());
 Trap->Tick(.02f);
 TestTrue(TEXT("Wall trap expires after twenty gameplay seconds"),Trap->IsActorBeingDestroyed());
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
