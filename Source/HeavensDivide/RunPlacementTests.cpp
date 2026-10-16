#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "EnemySpawner.h"
#include "EnemySpawnArea.h"
#include "EnemyBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunPlacementTest,"HeavensDivide.RunFixes.Placement",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRunPlacementTest::RunTest(const FString&)
{
    auto* W=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W); W->InitializeActorsForPlay(FURL());
    auto Box=[&](FVector Center,FVector Extent,bool Ground)
    {
        auto* A=W->SpawnActor<AActor>();auto* B=NewObject<UBoxComponent>(A);
        A->SetRootComponent(B);B->SetBoxExtent(Extent);B->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        B->SetCollisionObjectType(ECC_WorldStatic);B->SetCollisionResponseToAllChannels(ECR_Block);
        B->RegisterComponent();A->SetActorLocation(Center);if(Ground)A->Tags.Add(TEXT("SurvivalMapGround"));return B;
    };
    Box(FVector(0,0,-10),FVector(1000,1000,10),true);
    auto* Bounds=W->SpawnActor<AEnemySpawnArea>();Bounds->FindComponentByClass<UBoxComponent>()->SetBoxExtent(FVector(1800,1800,500));
    auto* Spawner=W->SpawnActor<AEnemySpawner>();FVector Found;
    TestTrue(TEXT("Open authored floor accepts objective"),Spawner->ValidateObjectiveLocation(FVector(0,400,100),220,400,Found));
    TestTrue(TEXT("Objective lands on floor, not marker elevation"),FMath::IsNearlyZero(Found.Z,1.f));
    W->BeginPlay(); // Include the enemy's real BeginPlay floor seating.
    for (const TCHAR* ClassPath : {TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyGrunt.BP_EnemyGrunt_C"),TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyGoblinCrawler.BP_EnemyGoblinCrawler_C")})
    {
        auto* EnemyClass=LoadClass<AEnemyBase>(nullptr,ClassPath);
        if(TestNotNull(TEXT("Scaled enemy Blueprint"),EnemyClass))
        {
            TestTrue(TEXT("Scaled enemy has a valid landing"),Spawner->ValidateEncounterLocation(FVector(-400,400,100),EnemyClass,Found));
            const auto* Capsule=EnemyClass->GetDefaultObject<AEnemyBase>()->GetCapsuleComponent();
            const float ExpectedHeight=Capsule->GetUnscaledCapsuleHalfHeight()*Capsule->GetRelativeScale3D().GetAbsMin();
            TestTrue(TEXT("Spawn height includes authored Blueprint root scale"),FMath::IsNearlyEqual(float(Found.Z),ExpectedHeight,.1f));
            auto* Enemy=Spawner->SpawnEnemyAtValidatedLocation(EnemyClass,Found+FVector(0,0,2));
            TestNotNull(TEXT("Collision spawn accepts scaled enemy on clear floor"),Enemy);
            if(Enemy)Enemy->Destroy();
            auto* LateObstacle=Box(FVector(-400,400,25),FVector(60,60,25),false);
            TestNull(TEXT("A constructed enemy cannot relocate onto a new obstacle"),Spawner->SpawnEnemyAtValidatedLocation(EnemyClass,Found+FVector(0,0,2)));
            LateObstacle->GetOwner()->Destroy();
        }
    }
    auto* Roof=Box(FVector(400,0,150),FVector(100,100,150),false);
    TestFalse(TEXT("Flat roof is not spawn ground"),Spawner->ValidateEncounterLocation(FVector(400,0,100),AEnemyBase::StaticClass(),Found));
    TestFalse(TEXT("Objective cannot sit on top of roof"),Spawner->ValidateObjectiveLocation(FVector(400,0,400),100,250,Found));
    TestFalse(TEXT("Objective footprint cannot intersect an adjacent solid prop"),Spawner->ValidateObjectiveLocation(FVector(150,0,100),220,400,Found));
    TestFalse(TEXT("Enemy capsule cannot hang over floor edge"),Spawner->ValidateEncounterLocation(FVector(995,400,100),AEnemyBase::StaticClass(),Found));
    TestFalse(TEXT("Objective footprint must be supported"),Spawner->ValidateObjectiveLocation(FVector(920,400,100),220,400,Found));
    Box(FVector(2500,0,-10),FVector(400,400,10),true);
    TestFalse(TEXT("Even authored floor outside arena is rejected"),Spawner->ValidateEncounterLocation(FVector(2500,0,100),AEnemyBase::StaticClass(),Found));
    Roof->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Ignore);
    Roof->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
    TestFalse(TEXT("Enemy-only barrier blocks spawns too"),Spawner->ValidateEncounterLocation(FVector(400,0,100),AEnemyBase::StaticClass(),Found));
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif
