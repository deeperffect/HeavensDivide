#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "EliteRewardChest.h"
#include "ExperiencePickup.h"
#include "ExperienceComponent.h"
#include "EnemyBase.h"
#include "SurvivorPlayerController.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEliteRewardTest,"HeavensDivide.Rewards.EliteChest",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEliteRewardTest::RunTest(const FString&)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->SetGameInstance(NewObject<UGameInstance>(GEngine));World->SetGameMode(FURL());
 World->InitializeActorsForPlay(FURL());
 auto* XP=NewObject<UExperienceComponent>(World);
 XP->RestoreRunState(20,0);
 auto* Chest=World->SpawnActor<AEliteRewardChest>();
 TestNotNull(TEXT("Chest skeletal mesh is packaged"),Chest->ChestMesh->GetSkinnedAsset());
 TestNotNull(TEXT("Golden loot Niagara exists"),Chest->RewardGlow->GetAsset());
 TestTrue(TEXT("Animation runs while world is paused"),Chest->PrimaryActorTick.bTickEvenWhenPaused);
 TestTrue(TEXT("Lid hinge exists"),Chest->ChestMesh->GetBoneIndex(Chest->LidBone)!=INDEX_NONE);
 TestTrue(TEXT("Front edge of lid opens upward"),Chest->LidOpenRotation.Quaternion().RotateVector(FVector(0,-1,0)).Z>0);
 Chest->InitializeReward(123,AExperiencePickup::StaticClass(),XP,nullptr);
 Chest->FinishOpening();
 int32 Count=0,Sum=0;
 TArray<AExperiencePickup*> Orbs;
 for(TActorIterator<AExperiencePickup> It(World);It;++It)
 {
  auto* Orb=*It;Orbs.Add(Orb);++Count;Sum+=Orb->XPValue;
  TestTrue(TEXT("Orbs begin airborne"),Orb->bRewardFlight);
  TestEqual(TEXT("Airborne orb does not overlap player"),Orb->PickupCollision->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
  TestFalse(TEXT("Orb does not collect before initialization"),Orb->bCollected);
 }
 TestEqual(TEXT("Shower contains forty orbs"),Count,40);
 TestEqual(TEXT("Remainder XP is conserved"),Sum,123);
 TestEqual(TEXT("No direct XP during opening"),XP->GetCurrentXP(),0);
 Chest->FinishOpening();
 Count=0;for(TActorIterator<AExperiencePickup> It(World);It;++It)++Count;
 TestEqual(TEXT("Repeated completion cannot duplicate reward"),Count,40);
 for(auto* Orb:Orbs)
 {
  Orb->Tick(2.f);
  TestFalse(TEXT("Flight terminates"),Orb->bRewardFlight);
  TestTrue(TEXT("Flight lands at traced position"),Orb->GetActorLocation().Equals(Orb->FlightLanding,.01f));
  Orb->Collect();Orb->Collect();
 }
 TestEqual(TEXT("Each orb awards XP exactly once"),XP->GetCurrentXP(),123);
 auto* PC=World->SpawnActor<ASurvivorPlayerController>();
 auto* Local=NewObject<ULocalPlayer>(GEngine);Local->SetControllerId(0);PC->SetPlayer(Local);
 PC->PlayerState=World->SpawnActor<APlayerState>();
 auto* Previous=World->SpawnActor<AActor>();
 auto* Interrupted=World->SpawnActor<AEliteRewardChest>();
 Interrupted->Player=PC;Interrupted->PreviousView=Previous;
 PC->SetViewTarget(Interrupted);Interrupted->RestorePlayer();
 TestEqual(TEXT("Interrupted reveal restores original view"),PC->GetViewTarget(),Previous);
 Interrupted->RestorePlayer();TestFalse(TEXT("Restore clears pause ownership"),Interrupted->bOwnsPause);
 World->GetWorldSettings()->SetTimeDilation(.7f);
 TestTrue(TEXT("Reward may pause the game"),PC->SetPause(true));
 Interrupted->InitializeReward(0,AExperiencePickup::StaticClass(),XP,nullptr);
 Interrupted->bOpening=true;Interrupted->bOwnsPause=true;Interrupted->OpeningSound=nullptr;
 Interrupted->OpeningStarted=FPlatformTime::Seconds()-1.5;
 Interrupted->LastRealTime=FPlatformTime::Seconds()-1./60.;
 PC->SetViewTarget(Interrupted);Interrupted->Tick(0);
 TestTrue(TEXT("Lid transforms advance even with zero paused delta"),
  !Interrupted->ChestMesh->GetBoneTransformByName(Interrupted->LidBone,EBoneSpaces::ComponentSpace).GetRotation().Equals(Interrupted->ClosedLid.GetRotation(),.1));
 TestTrue(TEXT("World remains paused during the reveal"),UGameplayStatics::IsGamePaused(World));
 Interrupted->OpeningStarted=FPlatformTime::Seconds()-4;Interrupted->Tick(0);
 TestFalse(TEXT("Reward completion releases its pause"),UGameplayStatics::IsGamePaused(World));
 TestEqual(TEXT("Reward does not reset existing slow motion"),World->GetWorldSettings()->TimeDilation,.7f);
 PC->SetPause(true);Interrupted->RestorePlayer();
 TestTrue(TEXT("Non-owner cannot release another menu's pause"),UGameplayStatics::IsGamePaused(World));
 PC->SetPause(false);
 const auto* Gorilla=LoadClass<AEnemyBase>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/Elites/BP_EnemyGorilla.BP_EnemyGorilla_C"));
 TestNotNull(TEXT("Gorilla blueprint"),Gorilla);
 if(Gorilla)TestEqual(TEXT("Saved Gorilla opts into shared elite reward"),Gorilla->GetDefaultObject<AEnemyBase>()->GetDropCategory(),EEnemyDropCategory::Elite);
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
 return true;
}
#endif
