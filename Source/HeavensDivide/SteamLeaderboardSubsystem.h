#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RunReportSubsystem.h"
#include "SteamLeaderboardSubsystem.generated.h"

struct FSteamScoreRow
{
    FString UserId;
    FString Name;
    int32 Rank = 0;
    int32 Score = 0;
    bool bSelf = false;
};
struct FSteamScoreBoard
{
    TArray<FSteamScoreRow> Rows;
    int32 PersonalBest = 0;
    int32 PersonalRank = 0;
    bool bReady = false;
};
USTRUCT()
struct FPendingSteamScore
{
    GENERATED_BODY()
    UPROPERTY() FString UserId;
    UPROPERTY() FRunScoreEntry Run;
};
UCLASS()
class URunSteamQueueSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version = 1;
    UPROPERTY() TArray<FPendingSteamScore> Pending;
};

struct FSteamLeaderboardWork;
/** Steam owns initialization/callback pumping through OSS; this polls call results on the game thread. */
UCLASS()
class HEAVENSDIVIDE_API USteamLeaderboardSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    USteamLeaderboardSubsystem();
    virtual ~USteamLeaderboardSubsystem() override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void RequestRefresh(FName MapId);
    void SubmitRun(const FRunScoreEntry& Run) { QueueBest(Run); }
    const FSteamScoreBoard* Board(FName MapId) const { return Boards.Find(MapId); }
    bool IsOnline() const { return bOnline; }
    DECLARE_MULTICAST_DELEGATE(FBoardsChanged);
    FBoardsChanged OnBoardsChanged;
    static bool ValidScore(const FRunScoreEntry& Run);
    static bool MergePending(TArray<FPendingSteamScore>& Queue, const FString& UserId, const FRunScoreEntry& Run);
    static TArray<int32> ScoreDetails(const FRunScoreEntry& Run);
private:
    bool Tick(float Delta);
    void QueueBest(const FRunScoreEntry& Run);
    void SaveQueue();
    TSharedPtr<FSteamLeaderboardWork> Work;
    TMap<FName, FSteamScoreBoard> Boards;
    UPROPERTY() TArray<FPendingSteamScore> Pending;
    FTSTicker::FDelegateHandle TickHandle;
    FDelegateHandle BestHandle;
    bool bOnline = false;
    bool bQueueReadOnly = false;
    bool bQueueDirty = false;
};
