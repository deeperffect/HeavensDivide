#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "RunLoadoutSubsystem.h"
#include "PlayerUpgradeComponent.h"
#include "ComboAbilityComponent.h"
#include "SurvivorPlayerController.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunLoadoutSelectionTest, "HeavensDivide.Progression.LoadoutSelections",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRunLoadoutSelectionTest::RunTest(const FString&)
{
    const FString Slot = TEXT("Automation_LoadoutSelections_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString PreviousCommandLine = FCommandLine::Get();
    FCommandLine::Set(*(TEXT("-MetaSaveSlot=") + Slot + TEXT(" ") + PreviousCommandLine));
    auto* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone();
    FCommandLine::Set(*PreviousCommandLine);
    auto* World = GI->GetWorld();
    World->InitializeActorsForPlay(FURL());
    auto* Meta = GI->GetSubsystem<USynergyMetaProgressionSubsystem>();
    auto* Loadout = GI->GetSubsystem<URunLoadoutSubsystem>();
    const auto Catalog = Meta->GetSynergyUpgradeDefinitions();
    auto Cleanup = [&]() {
        GI->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
        UGameplayStatics::DeleteGameInSlot(Slot, 0);
    };
    TestEqual(TEXT("Fresh profile equips only Tag Team"), Loadout->GetSelectedSynergies().Num(), 1);
    TestTrue(TEXT("Default uses the actual upgrade ID"), Loadout->GetSelectedSynergies().Contains(TEXT("TagTeam")));
    auto* TagTeam = Loadout->FindSynergyDefinition(TEXT("TagTeam"));
    if (!TestNotNull(TEXT("Saved Tag Team definition exists"), TagTeam)) { Cleanup(); return false; }
    TestEqual(TEXT("Tag Team resolves its permanent unlock ID"), TagTeam->MetaUnlockId, FName(TEXT("Synergy.TagTeam")));
    for (auto* Definition : Catalog)
        TestEqual(TEXT("Shipped synergy grants are single-rank mechanics"), Definition->MaxLevel, 1);
    TestFalse(TEXT("Locked synergy cannot be equipped"), Loadout->SetSynergySelected(TEXT("Handoff"), true));
    TestFalse(TEXT("Unknown synergy cannot be equipped"), Loadout->SetSynergySelected(TEXT("MissingSynergy"), true));
    TestFalse(TEXT("Duplicate selections are rejected"), Loadout->SetSelectedSynergies({TEXT("TagTeam"), TEXT("TagTeam")}));
    TestTrue(TEXT("An intentionally empty loadout is supported"), Loadout->SetSelectedSynergies({}));
    Loadout->SanitizeSelections();
    TestTrue(TEXT("Sanitizing preserves empty equipment"), Loadout->GetSelectedSynergies().IsEmpty());
    TestFalse(TEXT("Locked combo cannot be equipped"), Loadout->SetSelectedComboAbility(TEXT("BloodMoon")));
    Meta->AwardCurrencies(50000, 50000); Meta->RetrySkillReward();
    for (auto* Definition : Catalog)
        if (!Meta->IsUpgradeMetaEligible(Definition))
            TestTrue(TEXT("Purchase real synergy unlock for selection coverage"), Meta->PurchaseSynergy(Definition->MetaUnlockId.IsNone() ? Definition->UpgradeId : Definition->MetaUnlockId));
    TestTrue(TEXT("Purchase current combo"), Meta->PurchaseSkill(TEXT("Ability.ComboUnlock")));
    TestEqual(TEXT("Existing owners default to the current combo"), Loadout->GetSelectedComboAbility(), FName(TEXT("BloodMoon")));
    TestTrue(TEXT("Combo may be deliberately unequipped"), Loadout->SetSelectedComboAbility(NAME_None));
    Loadout->SanitizeSelections();
    TestTrue(TEXT("Explicit None survives reopening and sanitizing"), Loadout->GetSelectedComboAbility().IsNone());
    TestFalse(TEXT("Future placeholder cannot be equipped"), Loadout->SetSelectedComboAbility(TEXT("FutureCombo")));
    TestTrue(TEXT("Current combo can be re-equipped"), Loadout->SetSelectedComboAbility(TEXT("BloodMoon")));

    // Six owned synthetic definitions cover the hard slot bound before the shipped catalog grows.
    // They reuse the owned Tag Team entitlement but keep distinct runtime IDs and no gameplay effects.
    TArray<UUpgradeDefinition*> Future;
    TArray<FName> Selected;
    for (int32 I = 0; I < 6; ++I)
    {
        auto* Definition = NewObject<UUpgradeDefinition>(Loadout);
        Definition->UpgradeId = FName(*FString::Printf(TEXT("AutomationSynergy%d"), I));
        Definition->Category = EUpgradeCategory::Synergy;
        Definition->MetaUnlockId = TEXT("Synergy.TagTeam");
        Definition->MaxLevel = 1;
        Loadout->CachedSynergies.Add(Definition); Future.Add(Definition);
        if (I < 5) Selected.Add(Definition->UpgradeId);
    }
    Future[0]->PrerequisiteUpgradeIds.Add(TEXT("AutomationBuildDependency"));
    TestTrue(TEXT("Five valid distinct synergies may be selected"), Loadout->SetSelectedSynergies(Selected));
    auto Six = Selected; Six.Add(Future[5]->UpgradeId);
    TestFalse(TEXT("Sixth selection is rejected"), Loadout->SetSelectedSynergies(Six));
    TestEqual(TEXT("Rejected selection preserves all five previous choices"), Loadout->GetSelectedSynergies().Num(), 5);
    TestFalse(TEXT("Single-button setter also enforces five slots"), Loadout->SetSynergySelected(Future[5]->UpgradeId, true));

    auto* ControllerClass = LoadClass<ASurvivorPlayerController>(nullptr,
        TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    if (!TestNotNull(TEXT("Shipped controller supplies the real draft pool"), ControllerClass)) { Cleanup(); return false; }
    auto* PC = World->SpawnActor<ASurvivorPlayerController>(ControllerClass);
    auto* Upgrades = PC->GetPlayerUpgrades();
    for (auto* Definition : Future) Upgrades->UpgradePool.Add(Definition);
    TestFalse(TEXT("Build-dependent synergy still needs prerequisites for direct acquisition"), Upgrades->CanAcquireUpgrade(Future[0]));
    TestTrue(TEXT("Fresh run grants selected equipment without draft prerequisites"), Loadout->ApplyToFreshRun(Upgrades));
    TestTrue(TEXT("Blood is granted before starting synergy effects"), Upgrades->HasUpgradeId(TEXT("BattleStance")));
    TestTrue(TEXT("Barrage is granted before starting synergy effects"), Upgrades->HasUpgradeId(TEXT("BarrageStance")));
    for (FName Id : Selected) TestEqual(TEXT("Every selected synergy is active at rank one"), Upgrades->GetUpgradeLevelById(Id), 1);
    TestFalse(TEXT("Unselected Tag Team is not forced onto an explicit loadout"), Upgrades->HasUpgradeId(TEXT("TagTeam")));
    TestTrue(TEXT("Applying the loadout twice is idempotent"), Loadout->ApplyToFreshRun(Upgrades));
    for (FName Id : Selected) TestEqual(TEXT("Reapply never adds a rank"), Upgrades->GetUpgradeLevelById(Id), 1);
    TestFalse(TEXT("Acquisition also enforces the active five-synergy limit"), Upgrades->AcquireUpgrade(Future[5]));
    TestFalse(TEXT("Rank limit still applies to starting grants"), Upgrades->AcquireUpgrade(Future[1]));
    TestFalse(TEXT("Run locks equipment editing"), Loadout->SetSelectedSynergies({}));
    TestFalse(TEXT("Run locks combo editing"), Loadout->SetSelectedComboAbility(NAME_None));
    auto* Combo = PC->FindComponentByClass<UComboAbilityComponent>();
    TestTrue(TEXT("Owned and selected combo is equipped"), Combo->IsComboEquipped());
    Combo->AddCombo(1000.f);
    TestTrue(TEXT("Equipped combo meter can fill"), Combo->IsFull());

    FPlayerUpgradeRunState Snapshot; Upgrades->CaptureRunState(Snapshot);
    auto* Next = World->SpawnActor<ASurvivorPlayerController>(ControllerClass);
    auto* Restored = Next->GetPlayerUpgrades();
    for (auto* Definition : Future) Restored->UpgradePool.Add(Definition);
    Restored->RestoreRunState(Snapshot);
    for (FName Id : Selected) TestEqual(TEXT("Boss-travel snapshot retains selected mechanics"), Restored->GetUpgradeLevelById(Id), 1);
    TestEqual(TEXT("Travel retains combo equipment in the game instance"), Loadout->GetSelectedComboAbility(), FName(TEXT("BloodMoon")));
    // Incomplete older snapshots resolve their definitions before enforcing the hard limit.
    Snapshot.Levels.Add(Future[5]->UpgradeId, 1); Snapshot.Definitions.Reset();
    Restored->RestoreRunState(Snapshot);
    for (FName Id : Selected) TestTrue(TEXT("Overfull legacy snapshot prefers selected equipment"), Restored->HasUpgradeId(Id));
    TestFalse(TEXT("Missing snapshot definitions cannot bypass the five-slot limit"), Restored->HasUpgradeId(Future[5]->UpgradeId));

    auto* DraftPC = World->SpawnActor<ASurvivorPlayerController>(ControllerClass);
    auto* Draft = DraftPC->GetPlayerUpgrades();
    TestTrue(TEXT("Direct acquisition remains valid for loadout and tools"), Draft->AcquireUpgrade(TagTeam));
    TestFalse(TEXT("Synergy category is absent from normal choices"), Draft->GetEligibleCategories().Contains(EUpgradeCategory::Synergy));
    TestTrue(TEXT("Synergy category has no draft candidates"), Draft->GetEligibleUpgradesForCategory(EUpgradeCategory::Synergy).IsEmpty());
    TestTrue(TEXT("Direct candidate filtering excludes synergy cards"), Draft->GetEligibleUpgrades(Catalog).IsEmpty());
    TestTrue(TEXT("Directed rolls cannot return synergy cards"), Draft->RollUpgradeChoices(EUpgradeCategory::Synergy).IsEmpty());
    auto CheckCards = [&]() {
        for (auto* Card : Draft->GetCurrentUpgradeChoices())
            TestTrue(TEXT("Reward draft contains no synergy"), Card && Card->Category != EUpgradeCategory::Synergy);
    };
    Draft->BeginNormalLevelUpSelection(100); CheckCards();
    Draft->BeginDirectUpgradeSelection(100); CheckCards();
    Draft->BeginBloodShrineSelection(100); CheckCards();
    TestFalse(TEXT("Directed synergy draft is rejected"), Draft->BeginDirectCategoryUpgradeSelection(EUpgradeCategory::Synergy));
    TestTrue(TEXT("Rejected directed draft clears previous rewards"), Draft->GetCurrentUpgradeChoices().IsEmpty());
    TestFalse(TEXT("Draft selection cannot directly grant a synergy"), Draft->SelectUpgrade(Future[5]));
    PC->Destroy(); Next->Destroy(); DraftPC->Destroy();

    TestTrue(TEXT("Menu equipment survives retry with the same selection"), Loadout->GetSelectedSynergies() == Selected);
    TestTrue(TEXT("Can unequip combo between runs"), Loadout->SetSelectedComboAbility(NAME_None));
    auto* EmptyPC = World->SpawnActor<ASurvivorPlayerController>(ControllerClass);
    auto* EmptyCombo = EmptyPC->FindComponentByClass<UComboAbilityComponent>();
    TestTrue(TEXT("Unequipping does not remove permanent ownership"), EmptyCombo->IsComboUnlocked());
    TestFalse(TEXT("Explicit None disables the runtime ability"), EmptyCombo->IsComboEquipped());
    EmptyCombo->AddCombo(1000.f); EmptyCombo->NotifySynergyAttack(); EmptyCombo->RestoreCombo(1000.f);
    TestEqual(TEXT("Unequipped combo never gains or restores charge"), EmptyCombo->GetCombo(), 0.f);
    TestFalse(TEXT("Unequipped combo cannot activate"), EmptyCombo->TryActivateAbility());
    EmptyPC->GetPlayerUpgrades()->BalancePreviewMetaProfile = 1;
    TestTrue(TEXT("Full-meta lab bypasses menu equipment"), EmptyCombo->IsComboEquipped());
    EmptyCombo->AddCombo(1000.f);
    TestTrue(TEXT("Full-meta lab can exercise the meter"), EmptyCombo->IsFull());
    auto* LabUpgrades = EmptyPC->GetPlayerUpgrades();
    for (int32 I = 1; I < 6; ++I) TestTrue(TEXT("Lab can directly grant mechanics"), LabUpgrades->AcquireUpgrade(Future[I]));
    TestTrue(TEXT("Lab deliberately bypasses normal equipment cap"), LabUpgrades->AcquireUpgrade(TagTeam));
    EmptyPC->Destroy();

    TestTrue(TEXT("Equip paid synergy before reset"), Loadout->SetSelectedSynergies({TEXT("TagTeam"), TEXT("Handoff")}));
    TestTrue(TEXT("Equip current combo before reset"), Loadout->SetSelectedComboAbility(TEXT("BloodMoon")));
    TestTrue(TEXT("Reset uses only the isolated test profile"), Meta->ResetMetaProgression());
    Loadout->SanitizeSelections();
    TestEqual(TEXT("Reset removes a stale paid synergy choice"), Loadout->GetSelectedSynergies().Num(), 1);
    TestTrue(TEXT("Reset preserves default-owned Tag Team"), Loadout->GetSelectedSynergies().Contains(TEXT("TagTeam")));
    TestTrue(TEXT("Reset removes a stale paid combo choice"), Loadout->GetSelectedComboAbility().IsNone());
    Cleanup();
    return true;
}
#endif
