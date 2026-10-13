#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/SaveGame.h"
#include "PlayerUpgradeComponent.h"
#include "RunReportSubsystem.generated.h"

class AEnemyBase;
class ASurvivorPlayerController;

struct FRunMapInfo { FName Id; const TCHAR* Title; const TCHAR* ShortTitle; const TCHAR* SteamBoard; };
namespace RunReports
{
    HEAVENSDIVIDE_API const TArray<FRunMapInfo>& Maps();
    HEAVENSDIVIDE_API const FRunMapInfo* FindMap(FName Id);
    HEAVENSDIVIDE_API FString DamageName(FName Id);
    HEAVENSDIVIDE_API void ObjectiveCompleted(AActor* Objective);
}

/** A single damage event belongs to one source. Nested scopes restore their caller's attribution. */
class HEAVENSDIVIDE_API FRunDamageScope
{
public:
    explicit FRunDamageScope(FName Source, bool bPlayerDamage = false, bool bOnlyIfUnset = false,
        FName SecondarySource = NAME_None, float SecondaryFraction = 0.f);
    ~FRunDamageScope();
    FRunDamageScope(const FRunDamageScope&) = delete;
    FRunDamageScope& operator=(const FRunDamageScope&) = delete;
    static void Record(AActor* Victim, float ActualDamage, bool bKilled);
private:
    FName PreviousSource;
    bool bPreviousPlayerDamage;
    FName PreviousSecondarySource;
    float PreviousSecondaryFraction;
};

USTRUCT()
struct FRunDamageRow
{
    GENERATED_BODY()
    UPROPERTY() FName Source;
    UPROPERTY() double Damage = 0;
    UPROPERTY() int32 Hits = 0;
    UPROPERTY() int32 Kills = 0;
};

USTRUCT()
struct FRunScoreEntry
{
    GENERATED_BODY()
    UPROPERTY() FGuid RunId;
    UPROPERTY() FName MapId;
    UPROPERTY() FDateTime Date;
    UPROPERTY() int32 Score = 0;
    UPROPERTY() double Seconds = 0;
    UPROPERTY() int32 NormalKills = 0;
    UPROPERTY() int32 EliteKills = 0;
    UPROPERTY() int32 BossKills = 0;
    UPROPERTY() int32 Objectives = 0;
    UPROPERTY() int32 ModifierRanks = 0;
    UPROPERTY() bool bVictory = false;
    UPROPERTY() int32 ScoreVersion = 1;
    int32 SurvivalPoints() const;
    int32 BasePoints() const;
    int32 CalculateScore() const;
    double Multiplier() const { return 1.0 + .08 * FMath::Clamp(ModifierRanks, 0, 25); }
};

UCLASS()
class HEAVENSDIVIDE_API URunLeaderboardSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version = 1;
    UPROPERTY() TArray<FRunScoreEntry> Entries;
};

/** Local best-run storage. Steam can listen for new personal bests and use the stable board IDs. */
UCLASS()
class HEAVENSDIVIDE_API URunLeaderboardSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    TArray<FRunScoreEntry> TopTen(FName MapId) const;
    int32 PersonalBest(FName MapId) const;
    bool Submit(const FRunScoreEntry& Entry);
    bool HasPendingSave() const { return bPendingSave; }
    DECLARE_MULTICAST_DELEGATE_OneParam(FBestScoreCommitted, const FRunScoreEntry&);
    FBestScoreCommitted OnBestScoreCommitted;
private:
    friend class FRunReportsTest;
    bool Save();
    FString SaveSlot() const;
    FString SlotName = TEXT("HeavensDivide_Leaderboards_v1");
    UPROPERTY() TArray<FRunScoreEntry> Entries;
    bool bPendingSave = false;
    bool bReadOnly = false;
    FTSTicker::FDelegateHandle RetryHandle;
};

/** Lives across boss-arena travel; freezes exactly once before death/victory presentation. */
UCLASS()
class HEAVENSDIVIDE_API URunReportSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    void BeginRun(ASurvivorPlayerController* Controller);
    void AdvanceTime(float DeltaSeconds);
    void RecordDamage(AEnemyBase* Enemy, FName Source, float ActualDamage, bool bKilled);
    void RecordObjective(AActor* Objective);
    void CaptureBuild(UPlayerUpgradeComponent* Upgrades);
    void Finish(ASurvivorPlayerController* Controller, bool bVictory);
    void Abandon() { bActive = false; }
    bool IsActive() const { return bActive; }
    bool IsRanked() const { return bRanked; }
    const FString& UnrankedReason() const { return RankReason; }
    const FRunScoreEntry& Summary() const { return Run; }
    const FPlayerUpgradeRunState& Build() const { return RunBuild; }
    const TArray<FRunDamageRow>& DamageRows() const { return Damage; }
    const TMap<FName, int32>& Modifiers() const { return ModifierSnapshot; }
private:
    friend class FRunReportsTest;
    UPROPERTY() FRunScoreEntry Run;
    UPROPERTY() FPlayerUpgradeRunState RunBuild;
    UPROPERTY() TArray<FRunDamageRow> Damage;
    UPROPERTY() TMap<FName, int32> ModifierSnapshot;
    TSet<TWeakObjectPtr<AActor>> CountedObjectives;
    TSet<TWeakObjectPtr<AEnemyBase>> CountedKills;
    bool bActive = false;
    bool bRanked = false;
    bool bDifficultyEligible = false;
    FString RankReason;
};
