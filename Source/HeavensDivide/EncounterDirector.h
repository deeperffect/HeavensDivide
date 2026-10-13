#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EncounterDirector.generated.h"
class ATacticalEnemy;
class AHealingUrn;
class AEnemySpawner;
class ASurvivalFieldObjective;
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters|Magnet") TSubclassOf<AHealingUrn> MagnetUrnClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters|Magnet", meta=(ClampMin="0")) float FirstMagnetSeconds=150;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters|Magnet", meta=(ClampMin="10")) float MagnetIntervalMin=200;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters|Magnet", meta=(ClampMin="10")) float MagnetIntervalMax=280;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters|Healing", meta=(ClampMin="0")) float FirstUrnSeconds=36;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters|Healing", meta=(ClampMin="10")) float UrnIntervalMin=76;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters|Healing", meta=(ClampMin="10")) float UrnIntervalMax=110;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters") float FirstMarchSeconds=95;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters") float MarchIntervalMin=65;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters") float MarchIntervalMax=95;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounters") int32 MaxMarchEnemies=21;
    UFUNCTION(BlueprintCallable) bool SpawnMarch();
    UFUNCTION(BlueprintCallable) bool SpawnUrn(bool bMagnet=false);
    UFUNCTION(BlueprintCallable, Category="Encounters|Testing") bool SpawnWallTrap();
    UFUNCTION(BlueprintCallable, Category="Encounters|Testing") bool SpawnEmberCircle();
    UFUNCTION(CallInEditor, Category="Encounters|Testing", meta=(DisplayName="Spawn Wall Trap (PIE)", ToolTip="During Play, surround the player with breakable seals if the whole ring fits on valid ground.")) void TestWallTrap() { SpawnWallTrap(); }
    UFUNCTION(CallInEditor, Category="Encounters|Testing", meta=(DisplayName="Spawn Ember Circle (PIE)", ToolTip="During Play, place a nearby circle. Stay inside for 20 seconds to earn XP and Embers.")) void TestEmberCircle() { SpawnEmberCircle(); }
private:
    TWeakObjectPtr<AEnemySpawner> Spawner;
    TArray<TWeakObjectPtr<ATacticalEnemy>> Marchers;
    TArray<TWeakObjectPtr<AHealingUrn>> Urns;
    TArray<TWeakObjectPtr<AHealingUrn>> MagnetUrns;
    float NextMarch=0, NextUrn=36, NextMagnet=150;
    float WallTimes[2]={150,330};
    int32 WallWindow=0;
    float NextWallAttempt=0, NextCircle=75;
    TWeakObjectPtr<ASurvivalFieldObjective> WallTrap, EmberCircle;
};
