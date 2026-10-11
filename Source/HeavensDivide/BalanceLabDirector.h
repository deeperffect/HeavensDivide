#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BalanceLabDirector.generated.h"
class ASurvivorPlayerController;
class AEnemyBase;

/** Isolated, repeatable combat measurements. Place only in Lvl_BalanceLab. */
UCLASS()
class HEAVENSDIVIDE_API ABalanceLabDirector : public AActor
{
    GENERATED_BODY()
public:
    ABalanceLabDirector();
    virtual void Tick(float DeltaSeconds) override;
    static ABalanceLabDirector* Find(const UWorld* World);
    FString Command(ASurvivorPlayerController* PC, const FString& Action, const FString& Value, int32 Count);
    FString Report() const;
    bool IsReady() const;
private:
    void ClearTargets();
    void ResetMeter();
    bool SpawnTargets(const FString& Name, int32 Count);
    bool SetBuild(const FString& Name, int32 Ranks);
    bool WriteReport() const;
    UFUNCTION() void OnDamage(float Amount, float RemainingHealth);
    UFUNCTION() void OnTargetDied(AEnemyBase* Enemy);
    TWeakObjectPtr<ASurvivorPlayerController> Controller;
    TArray<TWeakObjectPtr<AEnemyBase>> Targets;
    FString BuildName = TEXT("Blood"), TargetName = TEXT("Dummy");
    float FirstHitTime = -1.f, LastHitTime = -1.f, TotalDamage = 0.f;
    int32 Hits = 0, Kills = 0;
    bool bInitialized = false;
    int32 SmokeSample = -1;
    float SmokeRemaining = 0.f;
    bool bSmokeFailed = false;
};
