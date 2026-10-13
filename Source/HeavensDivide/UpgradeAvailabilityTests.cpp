#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerUpgradeComponent.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "HeavensDivideMetaSaveGame.h"
#include "RunStanceCatalog.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpgradeAvailabilityTest,"HeavensDivide.Progression.UnlockShop",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUpgradeAvailabilityTest::RunTest(const FString&)
{
 auto* Meta=NewObject<USynergyMetaProgressionSubsystem>(NewObject<UGameInstance>(GEngine));
 Meta->TestSaveSlot=TEXT("Automation_UnlockShop_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
 Meta->CreateFreshSave();
 TestEqual(TEXT("Two default stances"),Meta->CurrentSave->UnlockedStanceIds.Num(),2);
 TestTrue(TEXT("Blood default"),Meta->IsStanceUnlocked(TEXT("BattleStance")));
 TestTrue(TEXT("Barrage default"),Meta->IsStanceUnlocked(TEXT("BarrageStance")));
 TestFalse(TEXT("Unknown stance locked"),Meta->IsStanceUnlocked(TEXT("Missing")));
 TestTrue(TEXT("Only Tag Team default"),Meta->GetUnlockedSynergyUpgradeIds()==TArray<FName>{FName(TEXT("Synergy.TagTeam"))});
 TestFalse(TEXT("Handoff needs purchase"),Meta->IsSynergyUpgradeUnlocked(TEXT("Synergy.Handoff")));
 TestEqual(TEXT("Stance unlock price tripled"),Meta->GetStanceCost(TEXT("Iaijutsu")),18);
 TestEqual(TEXT("Synergy unlock price tripled"),Meta->GetSynergyCost(TEXT("Synergy.Handoff")),12);
 TestEqual(TEXT("Default stance remains free"),Meta->GetStanceCost(TEXT("BattleStance")),0);
 TestEqual(TEXT("Default synergy remains free"),Meta->GetSynergyCost(TEXT("Synergy.TagTeam")),0);
 TestFalse(TEXT("Empty wallet blocks stance"),Meta->PurchaseStance(TEXT("Iaijutsu")));
 Meta->CurrentSave->SoulEmbers=73;
 constexpr int32 StartingSigils=1000;
 Meta->CurrentSave->SoulSigils=StartingSigils;
 Meta->CurrentSave->SkillRanks.Add(TEXT("Root.Vitality"),1);
 Meta->CurrentSave->SkillEmbersSpent=5;
 Meta->RefreshSkillBonuses();
 TestFalse(TEXT("Locked stance skill cannot be bought"),Meta->PurchaseSkill(TEXT("Iaijutsu.Edge")));
 Meta->bSimulateSaveFailure=true;
 TestFalse(TEXT("Failed stance save rolls back"),Meta->PurchaseStance(TEXT("Iaijutsu")));
 TestEqual(TEXT("Failed stance save keeps Sigils"),Meta->GetSoulSigils(),StartingSigils);
 TestFalse(TEXT("Failed save leaves stance locked"),Meta->IsStanceUnlocked(TEXT("Iaijutsu")));
 Meta->bSimulateSaveFailure=false;
 int32 Spent=0;
 for(const auto& Stance:RunStances::All)
 {
  const bool bDefault=RunStances::IsDefaultUnlocked(Stance.Id);
  const int32 Price=Meta->GetStanceCost(Stance.Id);
  TestEqual(TEXT("Only locked stances can be bought"),Meta->PurchaseStance(Stance.Id),!bDefault);
  Spent+=Price;
  TestFalse(TEXT("No duplicate stance purchase"),Meta->PurchaseStance(Stance.Id));
  TestTrue(TEXT("Purchased stance unlocked"),Meta->IsStanceUnlocked(Stance.Id));
 }
 const auto Synergies=Meta->GetSynergyUpgradeDefinitions();
 TestFalse(TEXT("Authored synergies present"),Synergies.IsEmpty());
 for(auto* Definition:Synergies)
 {
  const FName Id=Definition->MetaUnlockId.IsNone()?Definition->UpgradeId:Definition->MetaUnlockId;
  const bool bDefault=Id==TEXT("Synergy.TagTeam");
  TestEqual(TEXT("Only Tag Team eligible initially"),Meta->IsUpgradeMetaEligible(Definition),bDefault);
  const int32 Price=Meta->GetSynergyCost(Id);
  if(!bDefault)
  {
   const int32 Before=Meta->GetSoulSigils();
   Meta->bSimulateSaveFailure=true;
   TestFalse(TEXT("Failed synergy save rolls back"),Meta->PurchaseSynergy(Id));
   TestEqual(TEXT("Failed synergy save keeps wallet"),Meta->GetSoulSigils(),Before);
   TestFalse(TEXT("Failed synergy save keeps lock"),Meta->IsUpgradeMetaEligible(Definition));
   Meta->bSimulateSaveFailure=false;
  }
  TestEqual(TEXT("Buy only locked synergy"),Meta->PurchaseSynergy(Id),!bDefault);
  Spent+=Price;
  TestTrue(TEXT("Purchased synergy eligible"),Meta->IsUpgradeMetaEligible(Definition));
  TestFalse(TEXT("No duplicate synergy purchase"),Meta->PurchaseSynergy(Id));
 }
 TestFalse(TEXT("Unknown synergy cannot spend currency"),Meta->PurchaseSynergy(TEXT("Synergy.Missing")));
 TestEqual(TEXT("Unlocks use Sigils only"),Meta->GetSoulSigils(),StartingSigils-Spent);
 TestEqual(TEXT("Unlocks preserve Embers"),Meta->GetSoulEmbers(),73);
 TestTrue(TEXT("Unlock skill for purchased stance"),Meta->PurchaseSkill(TEXT("Iaijutsu.Edge")));
 TestTrue(TEXT("Refund only ranks"),Meta->RefundSkills());
 TestTrue(TEXT("Refund keeps purchased stance"),Meta->IsStanceUnlocked(TEXT("Iaijutsu")));
 TestEqual(TEXT("Refund never creates Sigils"),Meta->GetSoulSigils(),StartingSigils-Spent);
 Meta->LoadMetaProgression();
 for(const auto& Stance:RunStances::All)TestTrue(TEXT("Unlock persists"),Meta->IsStanceUnlocked(Stance.Id));
 TestEqual(TEXT("Both wallets persist"),Meta->GetSoulSigils(),StartingSigils-Spent);
 auto* U=NewObject<UPlayerUpgradeComponent>();
 TestTrue(TEXT("No retired discovery candidates"),U->GetLockedSynergyDiscoveryCandidates().IsEmpty());
 TestFalse(TEXT("Trials stay retired"),U->BeginSynergyDiscoverySelection(3));
 TestFalse(TEXT("No trial discoveries"),Meta->RecordTwinSoulCompletion());
 // Version 5 migration keeps prior investment, but replaces obsolete blanket availability.
 Meta->CurrentSave->SaveVersion=5;
 Meta->CurrentSave->SkillRanks.Add(TEXT("Iaijutsu.Edge"),2);
 Meta->CurrentSave->SkillEmbersSpent=45;
 Meta->SaveMetaProgression(); Meta->LoadMetaProgression();
 TestEqual(TEXT("Version six migration"),Meta->CurrentSave->SaveVersion,6);
 TestEqual(TEXT("Old stance investment retained"),Meta->GetSkillRank(TEXT("Iaijutsu.Edge")),2);
 TestFalse(TEXT("Old stance must be purchased"),Meta->IsStanceUnlocked(TEXT("Iaijutsu")));
 TestEqual(TEXT("Old locked stance bonus inactive"),Meta->GetStanceSkillBonus(TEXT("SteelDamage"),TEXT("Iaijutsu")),0.f);
 TestTrue(TEXT("Reset succeeds"),Meta->ResetMetaProgression());
 TestEqual(TEXT("Reset clears both wallets"),Meta->GetSoulEmbers()+Meta->GetSoulSigils(),0);
 TestFalse(TEXT("Reset restores stance lock"),Meta->IsStanceUnlocked(TEXT("Iaijutsu")));
 TestFalse(TEXT("Reset restores synergy lock"),Meta->IsSynergyUpgradeUnlocked(TEXT("Synergy.Handoff")));
 TestFalse(TEXT("Null upgrade rejected"),Meta->IsUpgradeMetaEligible(nullptr));
 UGameplayStatics::DeleteGameInSlot(Meta->TestSaveSlot,0);
 return true;
}
#endif
