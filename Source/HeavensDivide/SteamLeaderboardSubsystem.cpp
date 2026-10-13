#include "SteamLeaderboardSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "OnlineSubsystem.h"
#if WITH_BLOODSHIFT_STEAM
#include "steam/steam_api.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogBloodshiftSteam, Log, All);
namespace
{
const TCHAR* QueueSlot = TEXT("HeavensDivide_SteamQueue_v1");
enum class ESteamWork { None, Find, Upload, Top, Self };
}
struct FSteamLeaderboardWork
{
    ESteamWork Operation = ESteamWork::None;
    uint64 Call = 0;
    double Started = 0;
    double RetryAt = 0;
    double NextUpload = 0;
    double SaveAt = 0;
    FName Map;
    FString User;
    FRunScoreEntry Upload;
    TMap<FName, uint64> Handles;
    TSet<FName> Refresh;
    TMap<FName, double> Refreshed;
};
USteamLeaderboardSubsystem::USteamLeaderboardSubsystem() = default;
USteamLeaderboardSubsystem::~USteamLeaderboardSubsystem() = default;

bool USteamLeaderboardSubsystem::ValidScore(const FRunScoreEntry& R)
{
    return RunReports::FindMap(R.MapId) && R.RunId.IsValid() && R.ScoreVersion == 1
        && FMath::IsFinite(R.Seconds) && R.Seconds >= 0 && R.Seconds <= MAX_int32
        && R.NormalKills >= 0 && R.EliteKills >= 0 && R.BossKills >= 0 && R.Objectives >= 0
        && R.ModifierRanks >= 0 && R.ModifierRanks <= 25 && R.Score > 0 && R.Score == R.CalculateScore();
}
bool USteamLeaderboardSubsystem::MergePending(TArray<FPendingSteamScore>& Q, const FString& User, const FRunScoreEntry& R)
{
    uint64 Id = 0;
    if (!LexTryParseString(Id, *User) || Id == 0 || !ValidScore(R)) return false;
    for (auto& P : Q) if (P.UserId == User && P.Run.MapId == R.MapId)
    {
        if (P.Run.Score >= R.Score) return false;
        P.Run = R; return true;
    }
    FPendingSteamScore P; P.UserId = User; P.Run = R; Q.Add(P); return true;
}
TArray<int32> USteamLeaderboardSubsystem::ScoreDetails(const FRunScoreEntry& R)
{
    return {R.ScoreVersion, int32(FMath::FloorToInt(FMath::Clamp(R.Seconds, 0., double(MAX_int32)))), R.NormalKills, R.EliteKills,
        R.BossKills, R.Objectives, R.ModifierRanks, R.bVictory ? 1 : 0};
}
void USteamLeaderboardSubsystem::Initialize(FSubsystemCollectionBase& C)
{
    Super::Initialize(C);
    C.InitializeDependency<URunLeaderboardSubsystem>();
    // Automation/previews must never write to a real Steam board or load the player's upload queue.
    const TCHAR* Cmd = FCommandLine::Get();
    if (IsRunningCommandlet() || (GIsEditor && !FParse::Param(Cmd, TEXT("game")))
        || FParse::Param(Cmd, TEXT("DisableSteamLeaderboards")) || FParse::Param(Cmd, TEXT("RunReportPreview"))
        || FParse::Param(Cmd, TEXT("NullRHI")) || FCString::Strifind(Cmd, TEXT("LeaderboardSaveSlot="))) return;
    Work = MakeShared<FSteamLeaderboardWork>();
    if (UGameplayStatics::DoesSaveGameExist(QueueSlot, 0))
    {
        auto* Save = Cast<URunSteamQueueSave>(UGameplayStatics::LoadGameFromSlot(QueueSlot, 0));
        if (Save && Save->Version == 1)
            for (const auto& P : Save->Pending) MergePending(Pending, P.UserId, P.Run);
        else bQueueReadOnly = true;
    }
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USteamLeaderboardSubsystem::Tick), .2f);
    RequestRefresh(TEXT("FallenGrove"));
}
void USteamLeaderboardSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    if (bQueueDirty) SaveQueue();
    Work.Reset(); Boards.Reset(); bOnline = false;
    Super::Deinitialize();
}
void USteamLeaderboardSubsystem::RequestRefresh(FName Map)
{
    if (!Work || !RunReports::FindMap(Map)) return;
    const double* Last = Work->Refreshed.Find(Map);
    if (!Last || FPlatformTime::Seconds() - *Last > 30) Work->Refresh.Add(Map);
}
void USteamLeaderboardSubsystem::SaveQueue()
{
    if (bQueueReadOnly) return;
    auto* Save = NewObject<URunSteamQueueSave>(); Save->Pending = Pending;
    bQueueDirty = !UGameplayStatics::SaveGameToSlot(Save, QueueSlot, 0);
}
void USteamLeaderboardSubsystem::QueueBest(const FRunScoreEntry& R)
{
#if WITH_BLOODSHIFT_STEAM
    if (!Work || bQueueReadOnly) return;
    auto* OSS = IOnlineSubsystem::Get(FName(TEXT("STEAM")));
    if (!OSS || OSS->GetAppId() != TEXT("5283180") || !SteamUser()) return;
    // Bind pending uploads to the account that earned them. Never migrate another account's local best.
    const CSteamID Id = SteamUser()->GetSteamID();
    if (!Id.IsValid()) return;
    if (MergePending(Pending, LexToString(Id.ConvertToUint64()), R))
    {
        bQueueDirty = true; SaveQueue(); RequestRefresh(R.MapId);
    }
#endif
}

bool USteamLeaderboardSubsystem::Tick(float)
{
    if (!Work) return false;
    const double Now = FPlatformTime::Seconds();
    if (bQueueDirty && Now >= Work->SaveAt) { SaveQueue(); Work->SaveAt = Now + 5; }
#if WITH_BLOODSHIFT_STEAM
    auto* OSS = IOnlineSubsystem::Get(FName(TEXT("STEAM")));
    const bool Connected = OSS && OSS->GetAppId() == TEXT("5283180") && SteamUser()
        && SteamUserStats() && SteamUtils() && SteamFriends() && SteamUser()->BLoggedOn();
    if (!Connected)
    {
        if (bOnline) { bOnline = false; OnBoardsChanged.Broadcast(); }
        Work->Call = 0; Work->Operation = ESteamWork::None;
        return true;
    }
    const FString User = LexToString(SteamUser()->GetSteamID().ConvertToUint64());
    if (User != Work->User)
    {
        Work->User = User; Work->Handles.Reset(); Work->Refreshed.Reset(); Boards.Reset();
        Work->Call = 0; Work->Operation = ESteamWork::None;
        Work->RetryAt = 0; Work->Refresh.Add(TEXT("FallenGrove"));
        OnBoardsChanged.Broadcast();
    }
    if (!bOnline) { bOnline = true; OnBoardsChanged.Broadcast(); }
    auto Failed = [&]
    {
        UE_LOG(LogBloodshiftSteam, Warning, TEXT("Leaderboard request failed (%d, %s); retrying in 30 seconds"), int32(Work->Operation), *Work->Map.ToString());
        Work->Refresh.Add(Work->Map); Work->Operation = ESteamWork::None; Work->Call = 0;
        Work->RetryAt = Now + 30; OnBoardsChanged.Broadcast();
    };
    if (Work->Operation != ESteamWork::None)
    {
        bool IOFailure = false;
        if (!Work->Call) { Failed(); return true; }
        if (!SteamUtils()->IsAPICallCompleted(Work->Call, &IOFailure))
        {
            if (Now - Work->Started > 20) Failed();
            return true;
        }
        if (IOFailure) { Failed(); return true; }
        bool OK = false;
        switch (Work->Operation)
        {
        case ESteamWork::Find:
        {
            LeaderboardFindResult_t R{};
            OK = SteamUtils()->GetAPICallResult(Work->Call, &R, sizeof(R), R.k_iCallback, &IOFailure) && !IOFailure && R.m_bLeaderboardFound;
            if (OK)
            {
                // Refuse mismatched boards rather than submitting with the wrong score ordering.
                OK = SteamUserStats()->GetLeaderboardSortMethod(R.m_hSteamLeaderboard) == k_ELeaderboardSortMethodDescending
                    && SteamUserStats()->GetLeaderboardDisplayType(R.m_hSteamLeaderboard) == k_ELeaderboardDisplayTypeNumeric;
                if (OK) Work->Handles.Add(Work->Map, R.m_hSteamLeaderboard);
            }
            break;
        }
        case ESteamWork::Upload:
        {
            LeaderboardScoreUploaded_t R{};
            OK = SteamUtils()->GetAPICallResult(Work->Call, &R, sizeof(R), R.k_iCallback, &IOFailure) && !IOFailure && R.m_bSuccess;
            if (OK)
            {
                Pending.RemoveAll([&](const auto& P) { return P.UserId == User && P.Run.MapId == Work->Map && P.Run.Score <= Work->Upload.Score; });
                bQueueDirty = true; SaveQueue(); Work->Refresh.Add(Work->Map); Work->Refreshed.Remove(Work->Map);
                UE_LOG(LogBloodshiftSteam, Log, TEXT("Score uploaded: %s = %d"), *Work->Map.ToString(), Work->Upload.Score);
            }
            break;
        }
        case ESteamWork::Top:
        case ESteamWork::Self:
        {
            LeaderboardScoresDownloaded_t R{};
            OK = SteamUtils()->GetAPICallResult(Work->Call, &R, sizeof(R), R.k_iCallback, &IOFailure) && !IOFailure;
            if (OK)
            {
                auto& B = Boards.FindOrAdd(Work->Map);
                if (Work->Operation == ESteamWork::Top) B.Rows.Reset();
                else { B.PersonalBest = 0; B.PersonalRank = 0; }
                for (int32 I = 0; I < R.m_cEntryCount; ++I)
                {
                    LeaderboardEntry_t E{};
                    if (!SteamUserStats()->GetDownloadedLeaderboardEntry(R.m_hSteamLeaderboardEntries, I, &E, nullptr, 0)) { OK = false; break; }
                    if (Work->Operation == ESteamWork::Self)
                    {
                        if (E.m_steamIDUser == SteamUser()->GetSteamID()) { B.PersonalBest = E.m_nScore; B.PersonalRank = E.m_nGlobalRank; }
                    }
                    else
                    {
                        FSteamScoreRow Row; Row.UserId = LexToString(E.m_steamIDUser.ConvertToUint64());
                        Row.Name = UTF8_TO_TCHAR(SteamFriends()->GetFriendPersonaName(E.m_steamIDUser));
                        Row.Name.ReplaceInline(TEXT("\n"), TEXT(" ")); Row.Name.ReplaceInline(TEXT("\r"), TEXT(" "));
                        if (Row.Name.IsEmpty()) Row.Name = TEXT("Player");
                        Row.Rank = E.m_nGlobalRank; Row.Score = E.m_nScore; Row.bSelf = Row.UserId == User; B.Rows.Add(Row);
                    }
                }
                if (OK && Work->Operation == ESteamWork::Top)
                {
                    CSteamID Id = SteamUser()->GetSteamID();
                    Work->Operation = ESteamWork::Self; Work->Started = Now;
                    Work->Call = SteamUserStats()->DownloadLeaderboardEntriesForUsers(Work->Handles.FindChecked(Work->Map), &Id, 1);
                    return true;
                }
                if (OK) { B.bReady = true; Work->Refresh.Remove(Work->Map); Work->Refreshed.Add(Work->Map, Now); OnBoardsChanged.Broadcast(); }
            }
            break;
        }
        default: break;
        }
        if (!OK) { Failed(); return true; }
        Work->Operation = ESteamWork::None; Work->Call = 0;
    }
    if (Now < Work->RetryAt) return true;
    // Serialize uploads and space them well below Steam's ten-per-ten-minutes limit.
    const FPendingSteamScore* Upload = Pending.FindByPredicate([&](const auto& P) { return P.UserId == User && ValidScore(P.Run); });
    if (Upload && Now >= Work->NextUpload) { Work->Map = Upload->Run.MapId; Work->Upload = Upload->Run; }
    else
    {
        Upload = nullptr;
        if (Work->Refresh.IsEmpty()) return true;
        Work->Map = *Work->Refresh.CreateConstIterator();
    }
    const uint64* Handle = Work->Handles.Find(Work->Map);
    if (!Handle)
    {
        Work->Operation = ESteamWork::Find;
        Work->Call = SteamUserStats()->FindLeaderboard(TCHAR_TO_UTF8(RunReports::FindMap(Work->Map)->SteamBoard));
    }
    else if (Upload)
    {
        Work->Operation = ESteamWork::Upload;
        const auto Details = ScoreDetails(Work->Upload);
        Work->Call = SteamUserStats()->UploadLeaderboardScore(*Handle, k_ELeaderboardUploadScoreMethodKeepBest,
            Work->Upload.Score, Details.GetData(), Details.Num());
        Work->NextUpload = Now + 65;
    }
    else
    {
        Work->Operation = ESteamWork::Top;
        Work->Call = SteamUserStats()->DownloadLeaderboardEntries(*Handle, k_ELeaderboardDataRequestGlobal, 1, 10);
    }
    Work->Started = Now;
#endif
    return true;
}
