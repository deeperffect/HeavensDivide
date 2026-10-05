#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "BuildFamilyCatalog.h"
#include "PlayerUpgradeComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRetiredUpgradeStateTest, "HeavensDivide.Upgrades.RetiredRunState",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRetiredUpgradeStateTest::RunTest(const FString&)
{
	auto* Upgrades = NewObject<UPlayerUpgradeComponent>();
	FPlayerUpgradeRunState Legacy;
	Legacy.SamuraiMastery = 12;
	Legacy.NinjaMastery = 9;
	TArray<FName> RetiredIds = {TEXT("DeepCuts"), TEXT("PotentVenom"), TEXT("Crescendo")};
	for (const auto& Family : BuildFamilies)
	{
		if (Family.Available)
			continue;
		RetiredIds.Add(FName(Family.Id));
		for (const auto* Branch : Family.Branches)
			RetiredIds.Add(FName(Branch));
		for (const auto* Suffix : {TEXT("Power"), TEXT("Area"), TEXT("Haste")})
			RetiredIds.Add(FName(FString(Family.Id) + Suffix));
	}
	for (FName Id : RetiredIds)
	{
		auto* Card = NewObject<UUpgradeDefinition>(Upgrades);
		Card->UpgradeId = Id;
		Card->MaxLevel = 5;
		Legacy.Levels.Add(Id, 2);
		Legacy.AccumulatedMagnitudes.Add(Id, .5f);
		Legacy.Definitions.Add(Id, Card);
		TestFalse(TEXT("Stale retired card is ineligible"), Upgrades->CanAcquireUpgrade(Card));
		TestFalse(TEXT("Debug grants cannot reactivate retired cards"), Upgrades->DebugForceAcquireUpgrade(Card));
	}
	Upgrades->RestoreRunState(Legacy);
	FPlayerUpgradeRunState Restored;
	Upgrades->CaptureRunState(Restored);
	for (FName Id : RetiredIds)
	{
		TestEqual(TEXT("Retired rank is inactive"), Upgrades->GetUpgradeLevelById(Id), 0);
		TestEqual(TEXT("Retired magnitude is inactive"), Upgrades->GetAccumulatedUpgradeMagnitude(Id), 0.f);
		TestFalse(TEXT("Retired rank removed from next snapshot"), Restored.Levels.Contains(Id));
		TestFalse(TEXT("Retired magnitude removed from next snapshot"), Restored.AccumulatedMagnitudes.Contains(Id));
		TestFalse(TEXT("Retired definition removed from next snapshot"), Restored.Definitions.Contains(Id));
	}
	TestEqual(TEXT("Retirement preserves Samurai mastery"), Restored.SamuraiMastery, Legacy.SamuraiMastery);
	TestEqual(TEXT("Retirement preserves Ninja mastery"), Restored.NinjaMastery, Legacy.NinjaMastery);

	// A partially saved snapshot can contain a magnitude without a rank entry.
	Legacy.Levels.Reset();
	Legacy.Definitions.Reset();
	Upgrades->RestoreRunState(Legacy);
	for (FName Id : RetiredIds)
		TestEqual(TEXT("Orphan retired magnitudes cannot affect combat"), Upgrades->GetAccumulatedUpgradeMagnitude(Id),
		          0.f);

	auto* FamilyCard = NewObject<UUpgradeDefinition>(Upgrades);
	FamilyCard->UpgradeId = TEXT("UnknownOldFamilyBranch");
	FamilyCard->BuildFamilyId = TEXT("SteelTempest");
	TestFalse(TEXT("Retired family membership rejects unknown old branches"), Upgrades->CanAcquireUpgrade(FamilyCard));
	TestFalse(TEXT("Debug cannot bypass retired family membership"), Upgrades->DebugForceAcquireUpgrade(FamilyCard));

	auto* ActiveCard = NewObject<UUpgradeDefinition>(Upgrades);
	ActiveCard->UpgradeId = TEXT("CleanupTestActiveUpgrade");
	ActiveCard->MaxLevel = 3;
	ActiveCard->InvestmentOwner = EUpgradeInvestmentOwner::Ninja;
	ActiveCard->bUsesRolledRarity = true;
	FUpgradeRarityMagnitude Common;
	Common.Rarity = EUpgradeRarity::Common;
	Common.Magnitude = .2f;
	ActiveCard->RarityMagnitudes.Add(Common);
	TestTrue(TEXT("Ordinary acquisition still succeeds"), Upgrades->AcquireUpgrade(ActiveCard));
	TestEqual(TEXT("Ordinary acquisition records its rank"), Upgrades->GetUpgradeLevel(ActiveCard), 1);
	TestEqual(TEXT("Ordinary acquisition records magnitude"),
	          Upgrades->GetAccumulatedUpgradeMagnitude(ActiveCard->UpgradeId), .2f);
	TestEqual(TEXT("Ordinary acquisition grants mastery once"), Upgrades->GetNinjaMasteryPoints(), 10);
	TestTrue(TEXT("Debug acquisition still succeeds"), Upgrades->DebugForceAcquireUpgrade(ActiveCard, 3));
	TestEqual(TEXT("Debug acquisition replaces rank"), Upgrades->GetUpgradeLevel(ActiveCard), 3);
	TestTrue(TEXT("Debug acquisition replaces magnitude"),
	         FMath::IsNearlyEqual(Upgrades->GetAccumulatedUpgradeMagnitude(ActiveCard->UpgradeId), .6f));
	TestEqual(TEXT("Debug acquisition grants mastery once"), Upgrades->GetNinjaMasteryPoints(), 11);
	return true;
}
#endif
