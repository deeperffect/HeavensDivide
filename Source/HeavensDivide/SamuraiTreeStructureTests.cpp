#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSamuraiTreeStructureTest, "HeavensDivide.Combat.SamuraiTreeStructure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSamuraiTreeStructureTest::RunTest(const FString&)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const auto Cleanup = [&] { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Class = LoadClass<ASurvivorPlayerController>(nullptr,
        TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    auto* PC = World->SpawnActor<ASurvivorPlayerController>(Class);
    if (!TestNotNull(TEXT("Saved controller"), PC)) { Cleanup(); return false; }
    auto* U = PC->GetPlayerUpgrades();
    auto* Property = FindFProperty<FArrayProperty>(UPlayerUpgradeComponent::StaticClass(), TEXT("UpgradePool"));
    const auto& Pool = *Property->ContainerPtrToValuePtr<TArray<TObjectPtr<UUpgradeDefinition>>>(U);
    TSet<FName> PoolIds;
    for (const auto& Card : Pool) if (Card) PoolIds.Add(Card->UpgradeId);
    TestEqual(TEXT("Saved pool has unique card IDs"), PoolIds.Num(), Pool.Num());
    const TSet<FName> Disabled = {TEXT("OverkillBurst"), TEXT("BurstRadius"), TEXT("WaveMultishot"),
        TEXT("CrossingBlades"), TEXT("SplinterWave"), TEXT("BladeWavePower"), TEXT("WideArc"), TEXT("BladeWaveHaste")};
    const TSet<FName> BloodStats = {TEXT("SamuraiHeavyBlade"), TEXT("SamuraiTempo"), TEXT("SamuraiArea")};
    const TSet<FName> NewIds = {TEXT("BloodRush"), TEXT("IaijutsuVacuumReach"), TEXT("IaijutsuCascadePower"),
        TEXT("IaijutsuDashPower"), TEXT("ReturningBlade"), TEXT("CrescentSlowDuration"), TEXT("CrescentArcChance"), TEXT("CrescentFieldChance")};
    FPlayerUpgradeRunState Empty;
    for (const FName Stance : {FName(TEXT("BattleStance")), FName(TEXT("Iaijutsu")), FName(TEXT("BladeWave"))})
    {
        U->RestoreRunState(Empty);
        TestTrue(TEXT("Acquire exclusive trial stance"), U->AcquireUpgrade(U->FindUpgradeDefinition(Stance)));
        int32 Mechanics = 0, Normal = 0, Rare = 0, Shrine = 0;
        TArray<UUpgradeDefinition*> Group;
        for (const auto& Entry : Pool)
        {
            auto* Card = Entry.Get();
            if (!Card || Disabled.Contains(Card->UpgradeId) || Card->UpgradeId == Stance) continue;
            if (Card->PrerequisiteUpgradeIds.Contains(Stance) || (Stance == TEXT("BattleStance") && BloodStats.Contains(Card->UpgradeId)))
            {
                Group.Add(Card);
                if (Card->Category == EUpgradeCategory::Cursed) ++Shrine;
                else if (Card->MaxLevel == 1) ++Mechanics;
                else if (Card->Rarity == EUpgradeRarity::Rare) ++Rare;
                else ++Normal;
            }
        }
        TestEqual(TEXT("Every stance has 20 upgrades plus its stance"), Group.Num(), 20);
        TestEqual(TEXT("Six one-time mechanics"), Mechanics, 6);
        TestEqual(TEXT("Five normal scalable cards"), Normal, 5);
        TestEqual(TEXT("Six rare scalable cards"), Rare, 6);
        TestEqual(TEXT("Three Shrine tradeoffs"), Shrine, 3);
        for (FName Id : NewIds)
        {
            auto* Card = U->FindUpgradeDefinition(Id);
            if (!TestNotNull(*Id.ToString(), Card)) continue;
            if (!Group.Contains(Card))
            {
                TestFalse(TEXT("Other stances cannot acquire new mechanics or scaling"), U->AcquireUpgrade(Card));
                auto* Stale = DuplicateObject<UUpgradeDefinition>(Card, U);
                Stale->PrerequisiteUpgradeIds.Empty();
                TestFalse(TEXT("Stale references cannot bypass stance ownership"), U->AcquireUpgrade(Stale));
            }
            else if (Card->Rarity == EUpgradeRarity::Rare)
                TestFalse(TEXT("Rare branches require their unlock"), U->CanAcquireUpgrade(Card));
        }
        for (auto* Card : Group)
            if (Card->Category != EUpgradeCategory::Cursed && Card->MaxLevel == 1)
                TestTrue(TEXT("All six mechanics can coexist"), U->AcquireUpgrade(Card));
        U->BeginDirectUpgradeSelection(1000);
        for (FName Id : NewIds)
        {
            auto* Card = U->FindUpgradeDefinition(Id);
            if (!Card || !Group.Contains(Card) || Card->MaxLevel == 1) continue;
            TestTrue(TEXT("Eligible new branches enter ordinary rewards"), U->GetCurrentUpgradeChoices().ContainsByPredicate(
                [Id](const UUpgradeDefinition* Choice) { return Choice && Choice->UpgradeId == Id; }));
            for (int32 Rank = 0; Rank < 5; ++Rank) TestTrue(TEXT("Five useful ranks are obtainable"), U->AcquireUpgrade(Card));
            TestFalse(TEXT("A sixth rank is rejected"), U->AcquireUpgrade(Card));
        }
        FPlayerUpgradeRunState Saved;
        U->CaptureRunState(Saved); U->RestoreRunState(Saved);
        for (FName Id : NewIds)
            if (auto* Card = U->FindUpgradeDefinition(Id); Card && Group.Contains(Card))
                TestEqual(TEXT("New upgrades survive save restoration"), U->GetUpgradeLevelById(Id), Card->MaxLevel);
    }
    // Both slow investments convert once, preserving strength even at the damage cap.
    U->RestoreRunState(Empty);
    for (FName Id : {FName(TEXT("BladeWave")), FName(TEXT("CrescentField")), FName(TEXT("CrescentSlow")), FName(TEXT("CrescentSlowDuration"))})
        U->AcquireUpgrade(U->FindUpgradeDefinition(Id));
    U->AcquireUpgrade(U->FindUpgradeDefinition(TEXT("CrescentEruptionPact")));
    TestEqual(TEXT("Both slow purchases convert into damage"), U->GetUpgradeLevelById(TEXT("CrescentDamage")), 2);
    TestFalse(TEXT("Eruption blocks future slow duration"), U->CanAcquireUpgrade(U->FindUpgradeDefinition(TEXT("CrescentSlowDuration"))));
    FPlayerUpgradeRunState Saved; U->CaptureRunState(Saved); U->RestoreRunState(Saved);
    TestEqual(TEXT("Conversion is not repeated on reload"), U->GetUpgradeLevelById(TEXT("CrescentDamage")), 2);
    Cleanup(); return true;
}
#endif
