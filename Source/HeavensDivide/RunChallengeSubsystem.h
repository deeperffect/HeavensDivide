#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RunChallengeSubsystem.generated.h"

class AEnemyBase;
class AEnemySpawner;
class AActor;
class UWorld;

struct FRunChallengeModifier
{
    FName Id;
    FString Name;
    FString Description;
    float PerRank;
};

namespace RunChallenges
{
    HEAVENSDIVIDE_API const TArray<FRunChallengeModifier>& Modifiers();
    /** Source-level guard: training, despawns and menu worlds never pay permanent currency. */
    HEAVENSDIVIDE_API bool CanEarnCurrencies(const UWorld* World);
    HEAVENSDIVIDE_API void AwardCurrencies(AActor* Source, int32 Embers, int32 Sigils);
}

/** Optional, fixed run difficulty. Ranks survive map travel and retries; never grow with time. */
UCLASS()
class HEAVENSDIVIDE_API URunChallengeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="Run|Challenges") int32 GetModifierRank(FName Id) const;
    UFUNCTION(BlueprintCallable, Category="Run|Challenges") bool SetModifierRank(FName Id, int32 Rank);
    UFUNCTION(BlueprintPure, Category="Run|Challenges") float GetMultiplier(FName Id) const;
    UFUNCTION(BlueprintPure, Category="Run|Challenges") float GetRewardMultiplier() const;
    UFUNCTION(BlueprintPure, Category="Run|Challenges") bool CanEditModifiers() const;
    int32 GetUnlockedRank(FName Id) const;
    bool SetMap(FName Id);
    FName GetMapId() const { return MapId; }
    void BeginRun();
    bool ApplyEnemy(AEnemyBase* Enemy);
    bool ApplySpawner(AEnemySpawner* Spawner);
    void NotifyEnemyKilled(AEnemyBase* Enemy);
private:
    friend class FRunChallengeTest;
    UPROPERTY(Transient) TMap<FName, int32> Ranks;
    FName MapId=TEXT("FallenGrove");
    int32 CommonKillsTowardEmber = 0;
};
