#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerDashMovement.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerDashGroundTest, "HeavensDivide.Movement.DashGround",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayerDashGroundTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto Box = [&](FVector Position, FVector Extent)
    {
        auto* Actor = World->SpawnActor<AActor>();
        auto* Component = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Component);
        Component->SetBoxExtent(Extent);
        Component->SetCollisionProfileName(TEXT("BlockAll"));
        Component->RegisterComponent();
        Actor->SetActorLocation(Position);
        return Actor;
    };
    Box(FVector(0,0,-10), FVector(3000,3000,10));
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Character = World->SpawnActor<ACharacter>(FVector(0,0,100), FRotator::ZeroRotator, Params);
    auto* Movement = Character->GetCharacterMovement();
    Movement->MaxStepHeight = 45;
    const float Height = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.4f;
    auto Reset = [&]()
    {
        Character->SetActorLocation(FVector(0,0,Height), false);
        Movement->SetMovementMode(MOVE_Walking);
        Movement->FindFloor(Character->GetActorLocation(), Movement->CurrentFloor, false);
    };
    for (float Bump : {5.f, 20.f, 40.f})
    {
        auto* Step = Box(FVector(400,0,Bump/2), FVector(280,300,Bump/2));
        for (int32 FPS : {30,60,120})
        {
            Reset();
            bool Blocked = false;
            for (int32 Frame=0;Frame<FPS/5 && !Blocked;++Frame)
                Blocked = PlayerDashMovement::Move(Character,FVector(600.f/(FPS/5),0,0));
            TestFalse(FString::Printf(TEXT("%g cm step does not stop dash at %d FPS"),Bump,FPS),Blocked);
            TestTrue(TEXT("Step retains horizontal dash distance"),Character->GetActorLocation().X>590);
            TestTrue(TEXT("Capsule stays above raised surface"),Character->GetActorLocation().Z>=Height+Bump-3);
        }
        Step->Destroy();
    }
    auto* Wall = Box(FVector(300,0,150),FVector(20,300,150));
    Reset();
    TestTrue(TEXT("Solid wall stops dash"),PlayerDashMovement::Move(Character,FVector(600,0,0)));
    TestTrue(TEXT("Dash cannot pass through wall"),Character->GetActorLocation().X<280);
    Wall->Destroy();
    auto* Step = Box(FVector(400,0,15),FVector(280,300,15));
    auto* Ceiling = Box(FVector(350,0,Height*2+10),FVector(300,300,10));
    Reset();
    TestTrue(TEXT("Low ceiling prevents stepping through geometry"),PlayerDashMovement::Move(Character,FVector(600,0,0)));
    Ceiling->Destroy(); Step->Destroy();
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}
#endif
