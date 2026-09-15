#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SwapPortal.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwapPortalAnimationTest,"HeavensDivide.ImpactFeedback.SwapPortalAnimation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSwapPortalAnimationTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    for (EAxis::Type Axis : {EAxis::X,EAxis::Y,EAxis::Z})
    {
        auto* Portal=World->SpawnActor<ASwapPortal>();
        Portal->FullScale=FVector(2,3,4);
        Portal->WidthAxis=Axis;
        Portal->HoldDuration=1;
        Portal->OpeningDuration=.2f;
        Portal->ClosingDuration=.2f;
        const int32 Index=Axis==EAxis::X?0:Axis==EAxis::Z?2:1;
        Portal->AdvancePresentation(0);
        TestTrue(TEXT("Portal begins squeezed"),Portal->GetActorScale3D()[Index]<.01f);
        Portal->AdvancePresentation(.2f);
        TestTrue(TEXT("Opening restores the user's complete scale"),Portal->GetActorScale3D().Equals(Portal->FullScale));
        Portal->AdvancePresentation(.8f);
        TestFalse(TEXT("Closing has its own lifetime after movement and linger"),Portal->IsActorBeingDestroyed());
        Portal->AdvancePresentation(.1f);
        FVector Expected=Portal->FullScale;Expected[Index]*=.5f;
        TestTrue(TEXT("Closing squeezes only the chosen horizontal axis"),Portal->GetActorScale3D().Equals(Expected,.001f));
        Portal->AdvancePresentation(1);
        TestTrue(TEXT("A long frame completes closing and cleans up"),Portal->IsActorBeingDestroyed());
    }
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    return true;
}
#endif
