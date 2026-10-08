#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EncounterDirector.generated.h"
class ATacticalEnemy;
class AHealingUrn;
class AEnemySpawner;
UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AEncounterDirector : public AActor
{
    GENERATED_BODY()
public:
    AEncounterDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters") TSubclassOf<ATacticalEnemy> MarchingEnemyClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters") TSubclassOf<AHealingUrn> HealingUrnClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters") float FirstMarchSeconds=95;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters") float MarchIntervalMin=65;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters") float MarchIntervalMax=95;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters") int32 MaxMarchEnemies=21;
    UFUNCTION(BlueprintCallable) bool SpawnMarch();
    UFUNCTION(BlueprintCallable) bool SpawnUrn();
private:
    TWeakObjectPtr<AEnemySpawner> Spawner;
    TArray<TWeakObjectPtr<ATacticalEnemy>> Marchers;
    TArray<TWeakObjectPtr<AHealingUrn>> Urns;
    float NextMarch=0, NextUrn=18;
};
