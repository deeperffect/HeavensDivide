#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/SaveGame.h"
#include "Containers/Ticker.h"
#include "CollectionProgressSubsystem.generated.h"

class UUpgradeDefinition;
struct FRunScoreEntry;
struct FSteamCollectionWork;
struct FCollectionAchievement
{
    FName Id;
    const TCHAR* Title;
    const TCHAR* Description;
    FName Metric;
    int32 Goal;
};
namespace CollectionJournal
{
    HEAVENSDIVIDE_API const TArray<FCollectionAchievement>& Achievements();
}

UCLASS()
class HEAVENSDIVIDE_API UCollectionSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version=1;
    UPROPERTY() TMap<FName,int32> BestRanks;
    UPROPERTY() TSet<FName> Mastered;
    UPROPERTY() TMap<FName,int32> Metrics;
    UPROPERTY() TSet<FName> WonMaps;
    UPROPERTY() TMap<FName,FDateTime> Earned;
    UPROPERTY() TArray<FGuid> RecentRuns;
};

/** Lifetime collection and achievements; separate from refundable/resettable skill purchases. */
UCLASS()
class HEAVENSDIVIDE_API UCollectionProgressSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void RecordUpgrade(const UUpgradeDefinition* Definition,int32 Rank);
    void RecordRun(const FRunScoreEntry& Run);
    int32 BestRank(FName Id) const;
    bool IsMastered(FName Id) const;
    bool IsEarned(FName Id) const;
    bool IsSteamConfirmed(FName Id) const { return SteamConfirmed.Contains(Id); }
    int32 Progress(const FCollectionAchievement& Achievement) const;
    FString SteamStatus() const { return SyncStatus; }
    bool OpenSteamAchievements() const;
    void RefreshSteam() { NextSteamAttempt=0; }
private:
    friend class FCollectionProgressTest;
    friend struct FSteamCollectionWork;
    void Evaluate();
    void LoadProfile(const FString& Key);
    bool Save();
    bool Tick(float Delta);
    void SyncSteam();
    bool CompleteSteamStore(bool bSuccess,const FString& BatchProfile,TSet<FName>& Pending);
    UPROPERTY() TObjectPtr<UCollectionSave> Data;
    FString ProfileKey;
    FString SyncStatus=TEXT("Steam unavailable - progress is saved locally.");
    TSet<FName> SteamConfirmed;
    TSharedPtr<FSteamCollectionWork> SteamWork;
    FTSTicker::FDelegateHandle TickHandle;
    double NextSteamAttempt=0;
    bool bMemoryOnly=false, bDirty=false, bReadOnly=false;
};
