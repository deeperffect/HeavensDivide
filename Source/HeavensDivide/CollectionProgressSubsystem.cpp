#include "CollectionProgressSubsystem.h"
#include "UpgradeDefinition.h"
#include "PlayerUpgradeRules.h"
#include "RunReportSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "OnlineSubsystem.h"
#include "HAL/PlatformProcess.h"
#if WITH_BLOODSHIFT_STEAM
#include "steam/steam_api.h"
#endif

const TArray<FCollectionAchievement>& CollectionJournal::Achievements()
{
    static const TArray<FCollectionAchievement> Entries={
        {TEXT("HD_FIRST_RUN"),TEXT("First Steps"),TEXT("Finish a run, in victory or defeat."),TEXT("Runs"),1},
        {TEXT("HD_SURVIVE_10"),TEXT("Hold the Line"),TEXT("Survive ten minutes in one run."),TEXT("LongestRun"),600},
        {TEXT("HD_FIRST_VICTORY"),TEXT("Dawn Breaks"),TEXT("Win your first run."),TEXT("Wins"),1},
        {TEXT("HD_VESHRA"),TEXT("Break the Stinger"),TEXT("Defeat Veshra, the Pale Stinger."),TEXT("Bosses"),1},
        {TEXT("HD_KILLS_1000"),TEXT("Thousand Cuts"),TEXT("Defeat 1,000 enemies across completed runs."),TEXT("Kills"),1000},
        {TEXT("HD_ELITES_25"),TEXT("Elite Hunter"),TEXT("Defeat 25 elites across completed runs."),TEXT("Elites"),25},
        {TEXT("HD_OBJECTIVES_10"),TEXT("Answer the Call"),TEXT("Complete 10 objectives across completed runs."),TEXT("Objectives"),10},
        {TEXT("HD_DISCOVER_25"),TEXT("Growing Arsenal"),TEXT("Acquire 25 different upgrades or stances in runs."),TEXT("Discovered"),25},
        {TEXT("HD_MASTER_10"),TEXT("Honed to Perfection"),TEXT("Reach maximum rank on 10 different multi-rank upgrades."),TEXT("Mastered"),10},
        {TEXT("HD_FIRST_SYNERGY"),TEXT("Two as One"),TEXT("Acquire a synergy in a run."),TEXT("Synergies"),1},
        {TEXT("HD_FOUR_MAPS"),TEXT("Across the Divide"),TEXT("Win a run on each of the four maps."),TEXT("Maps"),4},
        {TEXT("HD_CHALLENGE_5"),TEXT("Against the Odds"),TEXT("Win with at least five total challenge ranks enabled."),TEXT("ChallengeWin"),5}
    };
    return Entries;
}

struct FSteamCollectionWork
{
    TWeakObjectPtr<UCollectionProgressSubsystem> Owner;
    TSet<FName> Pending;
    FString PendingProfile;
    bool bStoring=false;
    double StoreStarted=0;
#if WITH_BLOODSHIFT_STEAM
    CCallback<FSteamCollectionWork,UserStatsStored_t> Stored;
    explicit FSteamCollectionWork(UCollectionProgressSubsystem* InOwner):Owner(InOwner),Stored(this,&FSteamCollectionWork::OnStored) {}
    void OnStored(UserStatsStored_t* Result)
    {
        if(!bStoring || Result->m_nGameID!=5283180) return;
        bStoring=false;
        if(auto* O=Owner.Get(); O && O->ProfileKey==PendingProfile && SteamUser()
            && LexToString(SteamUser()->GetSteamID().ConvertToUint64())==PendingProfile)
            O->CompleteSteamStore(Result->m_eResult==k_EResultOK,PendingProfile,Pending);
    }
#else
    explicit FSteamCollectionWork(UCollectionProgressSubsystem* InOwner):Owner(InOwner) {}
#endif
};

namespace
{
FString SteamCollectionProfile()
{
#if WITH_BLOODSHIFT_STEAM
    auto* OSS=IOnlineSubsystem::Get(FName(TEXT("STEAM")));
    if(OSS && OSS->GetAppId()==TEXT("5283180") && SteamUser() && SteamUser()->GetSteamID().IsValid())
        return LexToString(SteamUser()->GetSteamID().ConvertToUint64());
#endif
    return TEXT("Local");
}
}

void UCollectionProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const TCHAR* Cmd=FCommandLine::Get();
    // Editor tests/previews never load a real profile or award real Steam achievements.
    bMemoryOnly=IsRunningCommandlet() || (GIsEditor && !FParse::Param(Cmd,TEXT("game")))
        || FParse::Param(Cmd,TEXT("NullRHI")) || FParse::Param(Cmd,TEXT("DisableSteamAchievements"));
    LoadProfile(bMemoryOnly?TEXT("Preview"):SteamCollectionProfile());
    if(!bMemoryOnly) SteamWork=MakeShared<FSteamCollectionWork>(this);
    TickHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&UCollectionProgressSubsystem::Tick),2.f);
}
void UCollectionProgressSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    if(bDirty) Save();
    SteamWork.Reset(); Super::Deinitialize();
}
void UCollectionProgressSubsystem::LoadProfile(const FString& Key)
{
    if(bDirty && !Save()) return; // Never discard unsaved progress on an account change.
    ProfileKey=Key; SteamConfirmed.Reset(); bReadOnly=false;
    if(SteamWork){SteamWork->Pending.Reset();SteamWork->bStoring=false;}
    Data=NewObject<UCollectionSave>(this);
    const FString Slot=TEXT("HeavensDivide_Collection_v1_")+Key;
    if(!bMemoryOnly && UGameplayStatics::DoesSaveGameExist(Slot,0))
    {
        auto* Loaded=Cast<UCollectionSave>(UGameplayStatics::LoadGameFromSlot(Slot,0));
        if(Loaded && Loaded->Version==1) Data=Loaded;
        else { bReadOnly=true; SyncStatus=TEXT("Collection save could not be read; existing file preserved."); }
    }
    NextSteamAttempt=0;
}
bool UCollectionProgressSubsystem::Save()
{
    if(bMemoryOnly) { bDirty=false; return true; }
    if(bReadOnly || !Data) return false;
    bDirty=!UGameplayStatics::SaveGameToSlot(Data,TEXT("HeavensDivide_Collection_v1_")+ProfileKey,0);
    return !bDirty;
}
int32 UCollectionProgressSubsystem::BestRank(FName Id) const { return Data?Data->BestRanks.FindRef(Id):0; }
bool UCollectionProgressSubsystem::IsMastered(FName Id) const { return Data && Data->Mastered.Contains(Id); }
bool UCollectionProgressSubsystem::IsEarned(FName Id) const { return Data && Data->Earned.Contains(Id); }
int32 UCollectionProgressSubsystem::Progress(const FCollectionAchievement& A) const
{ return IsEarned(A.Id)?A.Goal:FMath::Clamp(Data?Data->Metrics.FindRef(A.Metric):0,0,A.Goal); }

void UCollectionProgressSubsystem::RecordUpgrade(const UUpgradeDefinition* D,int32 Rank)
{
    if(!Data || bReadOnly || !D || D->UpgradeId.IsNone() || Rank<=0) return;
    if(!bMemoryOnly && SteamCollectionProfile()!=ProfileKey) return;
    if(PlayerUpgradeRules::IsRetiredUpgrade(D->UpgradeId)) return;
    const int32 Limit=FMath::Max(1,PlayerUpgradeRules::UpgradeRankLimit(D));
    const int32 Before=BestRank(D->UpgradeId);
    if(Rank<=Before) return;
    Data->BestRanks.Add(D->UpgradeId,FMath::Min(Rank,Limit));
    if(Limit>1 && Rank>=Limit) Data->Mastered.Add(D->UpgradeId);
    if(Before==0 && D->Category==EUpgradeCategory::Synergy) ++Data->Metrics.FindOrAdd(TEXT("Synergies"));
    Data->Metrics.Add(TEXT("Discovered"),Data->BestRanks.Num());
    Data->Metrics.Add(TEXT("Mastered"),Data->Mastered.Num());
    bDirty=true; Evaluate();
}
void UCollectionProgressSubsystem::RecordRun(const FRunScoreEntry& R)
{
    if(!Data || bReadOnly || !R.RunId.IsValid() || !RunReports::FindMap(R.MapId) || !FMath::IsFinite(R.Seconds) || R.Seconds<0
        || R.NormalKills<0 || R.EliteKills<0 || R.BossKills<0 || R.Objectives<0 || Data->RecentRuns.Contains(R.RunId)) return;
    if(!bMemoryOnly && SteamCollectionProfile()!=ProfileKey) return;
    Data->RecentRuns.Add(R.RunId);
    if(Data->RecentRuns.Num()>128) Data->RecentRuns.RemoveAt(0);
    auto Add=[&](FName Id,int32 Amount){auto& V=Data->Metrics.FindOrAdd(Id); V=int32(FMath::Min<int64>(int64(V)+Amount,MAX_int32));};
    Add(TEXT("Runs"),1); Add(TEXT("Kills"),R.NormalKills); Add(TEXT("Kills"),R.EliteKills); Add(TEXT("Kills"),R.BossKills);
    Add(TEXT("Elites"),R.EliteKills); Add(TEXT("Bosses"),R.BossKills); Add(TEXT("Objectives"),R.Objectives);
    auto& Longest=Data->Metrics.FindOrAdd(TEXT("LongestRun")); Longest=FMath::Max(Longest,int32(FMath::Min(R.Seconds,double(MAX_int32))));
    if(R.bVictory)
    {
        Add(TEXT("Wins"),1); Data->WonMaps.Add(R.MapId); Data->Metrics.Add(TEXT("Maps"),Data->WonMaps.Num());
        auto& Challenge=Data->Metrics.FindOrAdd(TEXT("ChallengeWin")); Challenge=FMath::Max(Challenge,R.ModifierRanks);
    }
    bDirty=true; Evaluate(); Save();
}
void UCollectionProgressSubsystem::Evaluate()
{
    for(const auto& A:CollectionJournal::Achievements())
        if(!IsEarned(A.Id) && Data->Metrics.FindRef(A.Metric)>=A.Goal)
        { Data->Earned.Add(A.Id,FDateTime::UtcNow()); bDirty=true; NextSteamAttempt=0; }
}
bool UCollectionProgressSubsystem::Tick(float)
{
    if(bDirty) Save();
    if(bMemoryOnly) return true;
    const FString Key=SteamCollectionProfile();
    // An account without an active Steam session uses its own local journal, never another user's progress.
    if(Key!=ProfileKey) LoadProfile(Key);
    if(FPlatformTime::Seconds()>=NextSteamAttempt) { NextSteamAttempt=FPlatformTime::Seconds()+30; SyncSteam(); }
    return true;
}
void UCollectionProgressSubsystem::SyncSteam()
{
#if WITH_BLOODSHIFT_STEAM
    if(!SteamWork || bReadOnly || ProfileKey==TEXT("Local") || SteamCollectionProfile()!=ProfileKey
        || !SteamUserStats() || !SteamUser() || !SteamUser()->BLoggedOn())
    { SyncStatus=TEXT("Steam unavailable - progress is saved locally."); return; }
    auto& Work=*SteamWork;
    if(Work.bStoring)
    {
        if(FPlatformTime::Seconds()-Work.StoreStarted<60) return;
        Work.bStoring=false; // Retry unconfirmed writes after a callback timeout.
    }
    bool Missing=false;
    for(const auto& A:CollectionJournal::Achievements())
    {
        bool Achieved=false;
        if(!SteamUserStats()->GetAchievement(TCHAR_TO_UTF8(*A.Id.ToString()),&Achieved)) { Missing=true; continue; }
        if(Achieved && !Work.Pending.Contains(A.Id))
        {
            SteamConfirmed.Add(A.Id);
            if(!IsEarned(A.Id)) { Data->Earned.Add(A.Id,FDateTime::UtcNow()); bDirty=true; }
        }
        else if(IsEarned(A.Id) && !SteamConfirmed.Contains(A.Id)
            && SteamUserStats()->SetAchievement(TCHAR_TO_UTF8(*A.Id.ToString()))) Work.Pending.Add(A.Id);
    }
    if(!Work.Pending.IsEmpty())
    {
        Work.PendingProfile=ProfileKey; Work.StoreStarted=FPlatformTime::Seconds();
        Work.bStoring=SteamUserStats()->StoreStats();
        SyncStatus=TEXT("Steam sync pending - progress is saved locally.");
    }
    else SyncStatus=Missing?TEXT("Steam achievement definitions are not available yet."):TEXT("Steam achievements synchronized.");
#endif
}
bool UCollectionProgressSubsystem::CompleteSteamStore(bool bSuccess,const FString& BatchProfile,TSet<FName>& Pending)
{
    if(BatchProfile!=ProfileKey) return false;
    if(!bSuccess)
    {
        // SetAchievement changes Steam's local cache before StoreStats is acknowledged.
        // Keep these IDs pending so a failed write cannot be mistaken for a cloud unlock.
        SyncStatus=TEXT("Steam sync pending - progress is saved locally."); return false;
    }
    SteamConfirmed.Append(Pending); Pending.Reset();
    SyncStatus=TEXT("Steam achievements synchronized."); return true;
}
bool UCollectionProgressSubsystem::OpenSteamAchievements() const
{
#if WITH_BLOODSHIFT_STEAM
    if(!bMemoryOnly && ProfileKey!=TEXT("Local") && SteamFriends() && SteamUtils() && SteamUtils()->IsOverlayEnabled())
    { SteamFriends()->ActivateGameOverlay("achievements"); return true; }
#endif
    if(!bMemoryOnly)
    { FPlatformProcess::LaunchURL(TEXT("https://steamcommunity.com/stats/5283180/achievements"),nullptr,nullptr); return true; }
    return false;
}
