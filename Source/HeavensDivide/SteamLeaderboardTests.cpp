#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SteamLeaderboardSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "Kismet/GameplayStatics.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamScoreQueueTest, "HeavensDivide.Progression.SteamScoreQueue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSteamScoreQueueTest::RunTest(const FString&)
{
    FRunScoreEntry R; R.RunId=FGuid::NewGuid();R.MapId=TEXT("FallenGrove");R.Seconds=125.7;
    R.NormalKills=100;R.EliteKills=2;R.BossKills=1;R.Objectives=3;R.ModifierRanks=3;R.bVictory=true;R.Score=R.CalculateScore();
    TestEqual(TEXT("Steam uploads the exact modifier-inclusive score"), R.Score,17670);
    TArray<FPendingSteamScore> Q;
    TestTrue(TEXT("Valid score queues for its earning account"),USteamLeaderboardSubsystem::MergePending(Q,TEXT("76561198000000001"),R));
    TestFalse(TEXT("Duplicate best not requeued"),USteamLeaderboardSubsystem::MergePending(Q,TEXT("76561198000000001"),R));
    auto Lower=R;Lower.NormalKills=1;Lower.Score=Lower.CalculateScore();
    TestFalse(TEXT("Worse run cannot replace pending best"),USteamLeaderboardSubsystem::MergePending(Q,TEXT("76561198000000001"),Lower));
    auto Higher=R;Higher.NormalKills=200;Higher.Score=Higher.CalculateScore();
    TestTrue(TEXT("New best coalesces"),USteamLeaderboardSubsystem::MergePending(Q,TEXT("76561198000000001"),Higher));
    TestEqual(TEXT("Only one pending score per account and map"),Q.Num(),1);
    TestTrue(TEXT("Different account remains separate"),USteamLeaderboardSubsystem::MergePending(Q,TEXT("76561198000000002"),R));
    auto Other=R;Other.MapId=TEXT("CinderCitadel");
    TestTrue(TEXT("Different map remains separate"),USteamLeaderboardSubsystem::MergePending(Q,TEXT("76561198000000001"),Other));
    TestEqual(TEXT("Account and map isolation"),Q.Num(),3);
    auto Invalid=R;Invalid.Score++;
    TestFalse(TEXT("Inconsistent score rejected"),USteamLeaderboardSubsystem::ValidScore(Invalid));
    Invalid=R;Invalid.Seconds=NAN;TestFalse(TEXT("Invalid duration rejected"),USteamLeaderboardSubsystem::ValidScore(Invalid));
    Invalid=R;Invalid.ScoreVersion=2;TestFalse(TEXT("Future formula rejected"),USteamLeaderboardSubsystem::ValidScore(Invalid));
    TestFalse(TEXT("Unknown user rejected"),USteamLeaderboardSubsystem::MergePending(Q,TEXT(""),R));
    const auto Details=USteamLeaderboardSubsystem::ScoreDetails(R);
    TestEqual(TEXT("Upload includes score version"),Details[0],1);
    TestEqual(TEXT("Upload includes completed seconds"),Details[1],125);
    TestEqual(TEXT("Upload includes modifiers"),Details[6],3);
    auto* Save=NewObject<URunSteamQueueSave>();Save->Pending=Q;
    TArray<uint8> Bytes;
    TestTrue(TEXT("Pending queue serializes"),UGameplayStatics::SaveGameToMemory(Save,Bytes));
    auto* Reload=Cast<URunSteamQueueSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if(TestNotNull(TEXT("Pending queue restores"),Reload))
    {
        TestEqual(TEXT("Restored entries"),Reload->Pending.Num(),3);
        TestEqual(TEXT("Restored account"),Reload->Pending[0].UserId,Q[0].UserId);
        TestEqual(TEXT("Restored best"),Reload->Pending[0].Run.Score,Higher.Score);
    }
    const FString Old=FCommandLine::Get();FCommandLine::Set(*(Old+TEXT(" -DisableSteamLeaderboards")));
    auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();FCommandLine::Set(*Old);
    auto* Steam=GI->GetSubsystem<USteamLeaderboardSubsystem>();
    TestNotNull(TEXT("Offline subsystem exists"),Steam);
    Steam->RequestRefresh(TEXT("FallenGrove"));
    TestFalse(TEXT("Automation remains offline"),Steam->IsOnline());
    TestTrue(TEXT("No fabricated global scores"),Steam->Board(TEXT("FallenGrove"))==nullptr);
    GI->Shutdown();return true;
}
#endif
