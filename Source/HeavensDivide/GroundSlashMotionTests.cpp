#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SamuraiBladeWave.h"
#include "SamuraiCharacter.h"
#include "SurvivorAbilityComponent.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "UpgradeDefinition.h"
#include "Components/BoxComponent.h"
#include "NiagaraComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroundSlashMotionTest,"HeavensDivide.Combat.GroundSlashMotion",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGroundSlashMotionTest::RunTest(const FString&)
{
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());
 auto Class=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 auto* PC=World->SpawnActor<ASurvivorPlayerController>(Class);
 if(!TestNotNull(TEXT("Controller"),PC)) return false;
 auto* Abilities=PC->FindComponentByClass<USurvivorAbilityComponent>();
 Abilities->Controller=PC;Abilities->Upgrades=PC->GetPlayerUpgrades();
 auto* Definition=Abilities->TuningDefinition(4);
 TestTrue(TEXT("Saved upgrade enables vendor motion"),Definition && Definition->Presentation.bGroundSlashMotion);
 auto* Ground=World->SpawnActor<AActor>();
 auto* Box=NewObject<UBoxComponent>(Ground);Ground->SetRootComponent(Box);
 Box->SetBoxExtent(FVector(5000,5000,10));Box->SetCollisionObjectType(ECC_WorldStatic);
 Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Block);
 Box->RegisterComponent();Ground->SetActorLocation(FVector(0,0,-10));
 auto* Samurai=World->SpawnActor<ASamuraiCharacter>(FVector(0,0,100),FRotator::ZeroRotator);Samurai->SetOwner(PC);
 auto* Wave=World->SpawnActor<ASamuraiBladeWave>(FVector(100,0,100),FRotator::ZeroRotator);
 Wave->InitializeBladeWave(Samurai,PC->GetPlayerUpgrades(),FVector::ForwardVector,10,300,650,1400,true);
 TestTrue(TEXT("Ground mode selected from upgrade"),Wave->bGroundSlash);
 TestTrue(TEXT("Ground trace positions visual 10cm above floor"),FMath::IsNearlyEqual(Wave->GroundAnchor.Z,10.f,.1f));
 Wave->Tick(.2f);
 TestTrue(TEXT("No slowdown before delay"),FMath::IsNearlyEqual(Wave->Movement->Velocity.Size(),1400.f));
 for(int32 Step=0;Step<6;++Step) Wave->Tick(.05f);
 TestTrue(TEXT("Velocity eases down after vendor delay"),Wave->Movement->Velocity.Size()<1400.f && Wave->Movement->Velocity.Size()>0.f);
 auto Outbound=Wave->AssignedVisual;
 TestTrue(TEXT("Upgrade visual exists"),Outbound.IsValid());
 Wave->SetActorLocation(Wave->PhaseOrigin+Wave->PhaseDirection*650);
 Wave->Tick(.01f);
 TestTrue(TEXT("Range cap starts return"),Wave->bReturning);
 TestTrue(TEXT("Returning wave restores speed"),FMath::IsNearlyEqual(Wave->Movement->Velocity.Size(),1400.f));
 TestTrue(TEXT("Outbound debris survives return"),Outbound.IsValid()&&!Outbound->IsActorBeingDestroyed());
 TestTrue(TEXT("Return spawns a fresh oriented effect"),Wave->AssignedVisual.IsValid()&&Wave->AssignedVisual!=Outbound);
 auto Return=Wave->AssignedVisual;
 Wave->FinishWave();
 TestTrue(TEXT("Return debris survives projectile completion"),Return.IsValid()&&!Return->IsActorBeingDestroyed());
 if(Return.IsValid())
 {
  Return->Tick(.5f);
  TestFalse(TEXT("Debris is not cut off at old short wave lifetime"),Return->IsActorBeingDestroyed());
  Return->Tick(6.f);
  TestTrue(TEXT("Debris has bounded cleanup"),!Return.IsValid() || Return->IsActorBeingDestroyed());
 }
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
 return true;
}
#endif
