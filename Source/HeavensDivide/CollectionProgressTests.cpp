#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CollectionProgressSubsystem.h"
#include "RunReportSubsystem.h"
#include "UpgradeDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollectionProgressTest,"HeavensDivide.Collection.Progress",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCollectionProgressTest::RunTest(const FString&)
{
    auto* Journal=NewObject<UCollectionProgressSubsystem>(NewObject<UGameInstance>());Journal->bMemoryOnly=true;Journal->LoadProfile(TEXT("TestA"));
    auto* D=NewObject<UUpgradeDefinition>();D->UpgradeId=TEXT("TestUpgrade");D->MaxLevel=4;
    Journal->RecordUpgrade(D,3);Journal->RecordUpgrade(D,1);
    TestEqual(TEXT("Best rank survives weaker later runs"),Journal->BestRank(D->UpgradeId),3);
    TestFalse(TEXT("Three of four is not mastery"),Journal->IsMastered(D->UpgradeId));
    Journal->RecordUpgrade(D,4);TestTrue(TEXT("Maximum rank is mastery"),Journal->IsMastered(D->UpgradeId));
    Journal->RecordUpgrade(D,4);TestEqual(TEXT("Repeat acquisitions do not duplicate discoveries"),Journal->Data->Metrics.FindRef(TEXT("Discovered")),1);
    D->UpgradeId=TEXT("OneRankStance");D->MaxLevel=1;Journal->RecordUpgrade(D,1);
    TestFalse(TEXT("Single-rank entries do not inflate mastery"),Journal->IsMastered(D->UpgradeId));
    FRunScoreEntry Run;Run.RunId=FGuid::NewGuid();Run.MapId=TEXT("FallenGrove");Run.Seconds=601;Run.NormalKills=1000;Run.BossKills=1;Run.Objectives=10;Run.bVictory=true;Run.ModifierRanks=5;
    Journal->RecordRun(Run);Journal->RecordRun(Run);
    TestEqual(TEXT("A run is counted once"),Journal->Data->Metrics.FindRef(TEXT("Runs")),1);
    for(FName Id:{FName(TEXT("HD_SURVIVE_10")),FName(TEXT("HD_KILLS_1000")),FName(TEXT("HD_VESHRA")),FName(TEXT("HD_OBJECTIVES_10")),FName(TEXT("HD_CHALLENGE_5"))})
        TestTrue(TEXT("Completed goal unlocks its matching achievement"),Journal->IsEarned(Id));
    TestFalse(TEXT("Offline unlock is not falsely marked Steam-confirmed"),Journal->IsSteamConfirmed(TEXT("HD_VESHRA")));
    TSet<FName> Pending{FName(TEXT("HD_VESHRA"))};
    TestFalse(TEXT("Failed Steam writes are not acknowledged"),Journal->CompleteSteamStore(false,TEXT("TestA"),Pending));
    TestTrue(TEXT("Failed writes retain pending IDs despite Steam's local cache"),Pending.Contains(TEXT("HD_VESHRA")));
    TestFalse(TEXT("A callback for another account cannot confirm this batch"),Journal->CompleteSteamStore(true,TEXT("TestB"),Pending));
    TestTrue(TEXT("Successful write acknowledges the intended account"),Journal->CompleteSteamStore(true,TEXT("TestA"),Pending));
    TestTrue(TEXT("Confirmed writes clear pending IDs"),Pending.IsEmpty()&&Journal->IsSteamConfirmed(TEXT("HD_VESHRA")));
    TSet<FName> Unique;for(const auto& A:CollectionJournal::Achievements()){TestFalse(TEXT("Steam API IDs unique"),Unique.Contains(A.Id));Unique.Add(A.Id);TestTrue(TEXT("Positive goal"),A.Goal>0);}
    TArray<uint8> Bytes;TestTrue(TEXT("Lifetime journal serializes"),UGameplayStatics::SaveGameToMemory(Journal->Data,Bytes));
    auto* Loaded=Cast<UCollectionSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
    TestTrue(TEXT("Ranks, mastery and earned achievements survive save round-trip"),Loaded&&Loaded->BestRanks.FindRef(TEXT("TestUpgrade"))==4&&Loaded->Mastered.Contains(TEXT("TestUpgrade"))&&Loaded->Earned.Contains(TEXT("HD_VESHRA")));
    Journal->LoadProfile(TEXT("TestB"));
    TestFalse(TEXT("Changing accounts does not import another account's achievements"),Journal->IsEarned(TEXT("HD_VESHRA")));
    TestEqual(TEXT("Changing accounts does not import another account's collection"),Journal->BestRank(TEXT("TestUpgrade")),0);
    return true;
}
#endif
