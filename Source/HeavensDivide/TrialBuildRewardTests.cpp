#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrialBuildRewardTest, "HeavensDivide.Combat.TrialBuildRewards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTrialBuildRewardTest::RunTest(const FString&)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* Class = LoadClass<ASurvivorPlayerController>(nullptr,
        TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    const TArray<FName> Samurai = {TEXT("BloodStance"), TEXT("ExecutionStance"), TEXT("WaveStance")};
    const TArray<FName> Ninja = {TEXT("ReturningFang"), TEXT("BarrageStance"), TEXT("GreatShuriken")};
    for (bool bSamurai : {true, false})
    {
        const auto& Ids = bSamurai ? Samurai : Ninja;
        const auto Category = bSamurai ? EUpgradeCategory::Samurai : EUpgradeCategory::Ninja;
        const auto Trial = bSamurai ? EUpgradeCategory::SamuraiTrial : EUpgradeCategory::NinjaTrial;
        for (FName ChosenId : Ids)
        {
            auto* PC = World->SpawnActor<ASurvivorPlayerController>(Class);
            auto* Upgrades = PC->GetPlayerUpgrades();
            for (auto* Choice : Upgrades->GetEligibleUpgradesForCategory(Category))
                TestFalse(TEXT("Regular character offers exclude route starters"), Ids.Contains(Choice->UpgradeId));
            Upgrades->BeginDirectUpgradeSelection(1000);
            for (auto* Choice : Upgrades->GetCurrentUpgradeChoices())
                TestFalse(TEXT("Unrestricted rewards exclude both characters' starters"), Samurai.Contains(Choice->UpgradeId) || Ninja.Contains(Choice->UpgradeId));
            TestTrue(TEXT("Trial produces a route choice"), Upgrades->BeginDirectCategoryUpgradeSelection(Trial, 3));
            const auto Choices = Upgrades->GetCurrentUpgradeChoices();
            TestEqual(TEXT("Trial offers all three routes"), Choices.Num(), 3);
            for (FName Id : Ids)
                TestTrue(TEXT("Correct character route is offered"), Choices.Contains(Upgrades->FindUpgradeDefinition(Id)));
            TestTrue(TEXT("Trial selection grants route"), Upgrades->SelectUpgrade(Upgrades->FindUpgradeDefinition(ChosenId)));
            TestTrue(TEXT("Selected route is owned"), Upgrades->HasUpgradeId(ChosenId));
            for (FName Id : Ids)
                TestFalse(TEXT("Other routes and duplicate ranks remain excluded"), Upgrades->CanAcquireUpgrade(Upgrades->FindUpgradeDefinition(Id)));
            TestTrue(TEXT("Repeat trial offers an eligible upgrade"), Upgrades->BeginDirectCategoryUpgradeSelection(Trial, 3));
            for (auto* Choice : Upgrades->GetCurrentUpgradeChoices())
            {
                TestEqual(TEXT("Repeat reward stays with trial character"), Choice->Category, Category);
                TestFalse(TEXT("Repeat reward cannot replace chosen route"), Ids.Contains(Choice->UpgradeId));
                TestTrue(TEXT("Repeat reward respects prerequisites"), Upgrades->CanAcquireUpgrade(Choice));
            }
            // Choosing one character's route must not prevent the other character's trial choice.
            TestTrue(TEXT("Other character trial remains available"), Upgrades->BeginDirectCategoryUpgradeSelection(
                bSamurai ? EUpgradeCategory::NinjaTrial : EUpgradeCategory::SamuraiTrial, 3));
            TestEqual(TEXT("Other character still has three routes"), Upgrades->GetCurrentUpgradeChoices().Num(), 3);
            PC->Destroy();
        }
    }
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
