#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TesterBalanceSettings.generated.h"

class AEnemyBase;
class AEnemySpawner;
USTRUCT()
struct FTesterEnemyBalance
{
    GENERATED_BODY()
    UPROPERTY() float Health = 1.f;
    UPROPERTY() float Speed = 1.f;
};

/** Local playtest overrides. Never modifies authored enemy or map assets. */
UCLASS(Config=TesterBalance)
class HEAVENSDIVIDE_API UTesterBalanceSettings : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(Config) bool bEnabled = false;
    UPROPERTY(Config) int32 MaxAlive = 0;
    UPROPERTY(Config) float Population = 1.f;
    UPROPERTY(Config) float SpawnInterval = 1.f;
    UPROPERTY(Config) float HealthGrowth = 1.f;
    UPROPERTY(Config) bool bDisableEvents = false;
    UPROPERTY(Config) TMap<FString, FTesterEnemyBalance> Enemies;
    void Reset();
    void CopyFrom(const UTesterBalanceSettings& Other);
    void Sanitize();
    void ApplyEnemy(AEnemyBase* Enemy) const;
    float GetHealthMultiplier(const AEnemyBase* Enemy) const;
    void ApplySpawner(AEnemySpawner* Spawner) const;
    FString Report() const;
    static TArray<TPair<FString, FString>> Roster(); // Display name, generated class path.
};
