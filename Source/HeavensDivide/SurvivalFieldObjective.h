#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvivalFieldObjective.generated.h"
class AEncounterWall;
class AEnemySpawner;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMinimapMarkerComponent;
class UMaterialInstanceDynamic;

UCLASS()
class HEAVENSDIVIDE_API ASurvivalFieldObjective : public AActor
{
    GENERATED_BODY()
public:
    ASurvivalFieldObjective();
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(BlueprintCallable) bool InitializeWallTrap(AEnemySpawner* Spawner);
    UFUNCTION(BlueprintCallable) void InitializeHoldZone();
    UPROPERTY(EditAnywhere, Category="Encounter") float Radius=1600;
    UPROPERTY(EditAnywhere, Category="Encounter", meta=(ClampMin="1",Units="s")) float WallLifetime=10;
    UPROPERTY(EditAnywhere, Category="Encounter") float HoldSeconds=20;
    UPROPERTY(EditAnywhere, Category="Encounter") int32 EmberReward=20;
    float GetProgress() const { return Progress; }
    bool IsComplete() const { return bComplete; }
    bool IsWallTrap() const { return bWallTrap; }
    bool ContainsPlayer(const FVector& Point) const;
    void AdvanceHold(float Delta, bool bInside);
private:
    void CompleteHold();
    void UpdateLabel(const FString& Text);
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Ring;
    UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY() TObjectPtr<UMinimapMarkerComponent> Marker;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RingMaterial;
    UPROPERTY() TArray<TObjectPtr<AEncounterWall>> Walls;
    float Age=0, Progress=0;
    bool bWallTrap=false, bComplete=false, bSealed=false;
};
