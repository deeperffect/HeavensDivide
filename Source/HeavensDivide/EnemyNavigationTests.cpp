#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "EnemyNavigationSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyNavigationBudgetTest, "HeavensDivide.Enemies.NavigationBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnemyNavigationBudgetTest::RunTest(const FString& Parameters)
{
    FEnemyPathRequestBudget Budget;
    TestTrue(TEXT("Initial request"), Budget.TryAcquire(1, 0.0));
    TestTrue(TEXT("Second request"), Budget.TryAcquire(1, 0.0));
    TestFalse(TEXT("Crowd cannot exceed two requests per frame"), Budget.TryAcquire(1, 1.0));
    TestTrue(TEXT("Next frame admits work"), Budget.TryAcquire(2, 1.0));
    Budget.RecordCost(0.6);
    TestFalse(TEXT("Expensive query prevents another this frame"), Budget.TryAcquire(2, 1.0));
    FEnemyPathRequestBudget Rate;
    int32 Requests = 0;
    for (uint64 Frame = 0; Frame < 600; ++Frame)
        for (int32 Enemy = 0; Enemy < 200; ++Enemy)
            Requests += Rate.TryAcquire(Frame, Frame / 600.0) ? 1 : 0;
    TestTrue(TEXT("200 enemies at 600 FPS still obey 30/s plus two burst tokens"), Requests <= 32 && Requests >= 30);
    FEnemyPathRequestBudget OtherWorld;
    TestTrue(TEXT("Separate worlds have independent budgets"), OtherWorld.TryAcquire(1, 0.0));
    return true;
}
#endif
