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
    const TArray<FName> Samurai = {TEXT("BladeWave"), TEXT("Iaijutsu"), TEXT("BattleStance")};
    const TSet<FName> MeleeScaling = {TEXT("SamuraiHeavyBlade"), TEXT("SamuraiTempo"), TEXT("SamuraiArea")};
    const TSet<FName> IaijutsuScaling = {TEXT("IaijutsuDamage"), TEXT("IaijutsuChargeSpeed"), TEXT("IaijutsuWidth"), TEXT("IaijutsuMarkDamage")};
    const TSet<FName> CrescentScaling = {TEXT("CrescentDamage"), TEXT("CrescentSpeed"), TEXT("CrescentRange"), TEXT("CrescentSlow")};
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
            TestFalse(TEXT("Retired trial cannot offer a route"), Upgrades->BeginDirectCategoryUpgradeSelection(Trial, Ids.Num()));
            TestTrue(TEXT("Pre-run loadout can grant any chosen route"), Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(ChosenId)));
            TestTrue(TEXT("Selected route is owned"), Upgrades->HasUpgradeId(ChosenId));
            for (FName Id : Ids)
                TestFalse(TEXT("Other routes and duplicate ranks remain excluded"), Upgrades->CanAcquireUpgrade(Upgrades->FindUpgradeDefinition(Id)));
            const auto CheckStance = [&](const UUpgradeDefinition* Choice)
            {
                if (!bSamurai) return;
                TestFalse(TEXT("Non-Blood offers exclude melee scaling"),ChosenId!=TEXT("BattleStance") && MeleeScaling.Contains(Choice->UpgradeId));
                TestFalse(TEXT("Other stances exclude Crescent scaling"),ChosenId!=TEXT("BladeWave") && CrescentScaling.Contains(Choice->UpgradeId));
                TestFalse(TEXT("Melee stance offers exclude Iaijutsu scaling"),ChosenId!=TEXT("Iaijutsu") && IaijutsuScaling.Contains(Choice->UpgradeId));
            };
            for (auto* Choice : Upgrades->GetEligibleUpgradesForCategory(Category)) CheckStance(Choice);
            Upgrades->BeginDirectUpgradeSelection(1000);
            for (auto* Choice : Upgrades->GetCurrentUpgradeChoices()) CheckStance(Choice);
            if (bSamurai)
                for (FName Id : ChosenId==TEXT("Iaijutsu") ? IaijutsuScaling : ChosenId==TEXT("BladeWave") ? CrescentScaling : MeleeScaling)
                {
                    auto* Card = Upgrades->FindUpgradeDefinition(Id);
                    TestTrue(*FString::Printf(TEXT("%s remains in its authored reward category"), *Id.ToString()),
                        Card && Upgrades->GetEligibleUpgradesForCategory(Card->Category).Contains(Card));
                }
            TestFalse(TEXT("Trials remain unavailable after selecting a stance"), Upgrades->BeginDirectCategoryUpgradeSelection(Trial, Ids.Num()));
            TestTrue(TEXT("Regular category offers an eligible upgrade"), Upgrades->BeginDirectCategoryUpgradeSelection(Category, Ids.Num()));
            for (auto* Choice : Upgrades->GetCurrentUpgradeChoices())
            {
                CheckStance(Choice);
                TestEqual(TEXT("Regular reward stays with selected character"), Choice->Category, Category);
                TestFalse(TEXT("Repeat reward cannot replace chosen route"), Ids.Contains(Choice->UpgradeId));
                TestTrue(TEXT("Repeat reward respects prerequisites"), Upgrades->CanAcquireUpgrade(Choice));
            }
            TestFalse(TEXT("Other character trial is also retired"), Upgrades->BeginDirectCategoryUpgradeSelection(
                bSamurai ? EUpgradeCategory::NinjaTrial : EUpgradeCategory::SamuraiTrial, 3));
            TestTrue(TEXT("Other character loadout remains available"), Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition((bSamurai ? Ninja : Samurai)[0])));
            PC->Destroy();
        }
    }
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
