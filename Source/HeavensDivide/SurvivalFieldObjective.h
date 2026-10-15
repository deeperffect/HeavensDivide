#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "SurvivalFieldObjective.generated.h"
class AEncounterWall;
class AEnemySpawner;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMinimapMarkerComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UObjectiveInteractionComponent;

UCLASS()
class HEAVENSDIVIDE_API ASurvivalFieldObjective : public AActor, public IInteractable
{
    GENERATED_BODY()
public:
    ASurvivalFieldObjective();
    static constexpr float HoldZoneRadius = 1200.f;
    virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
    virtual void Interact_Implementation(APawn* InteractingPawn) override;
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(BlueprintCallable) bool InitializeWallTrap(AEnemySpawner* Spawner);
    UFUNCTION(BlueprintCallable) void InitializeHoldZone();
    UPROPERTY(EditAnywhere, Category="Encounter") float Radius=1600;
    UPROPERTY(EditAnywhere, Category="Encounter", meta=(ClampMin="1",Units="s")) float WallLifetime=20;
    UPROPERTY(EditAnywhere, Category="Encounter|Wall", meta=(ClampMin="0.1",Units="s")) float WallWaveSeconds=.75f;
    UPROPERTY(EditAnywhere, Category="Encounter|Wall", meta=(ClampMin="0.05",Units="s")) float WallRiseSeconds=.22f;
    UPROPERTY(EditAnywhere, Category="Encounter") float HoldSeconds=20;
    UPROPERTY(EditAnywhere, Category="Encounter") int32 EmberReward=20;
    float GetProgress() const { return Progress; }
    bool IsComplete() const { return bComplete; }
    bool IsWallTrap() const { return bWallTrap; }
    bool IsHoldActive() const { return bHoldActive; }
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Components") TObjectPtr<UStaticMeshComponent> ActivationMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Components") TObjectPtr<UObjectiveInteractionComponent> ObjectiveInteraction;
    bool ContainsPlayer(const FVector& Point) const;
    void AdvanceHold(float Delta, bool bInside);
private:
    void UpdateHoldPresentation();
    void CompleteHold();
    void BeginHold();
    void BeginWall();
    friend class FObjectiveIntroTest;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Ring;
    UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY() TObjectPtr<UMinimapMarkerComponent> Marker;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RingMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> HoldRingMaterial;
    UPROPERTY() TArray<TObjectPtr<AEncounterWall>> Walls;
    float Age=0, Progress=0;
    float WallWaveStartAge=-1.f;
    bool bWallTrap=false, bComplete=false, bSealed=false;
    bool bWallIntroShown=false;
    bool bHoldInitialized=false, bHoldActive=false, bIntroPending=false;
};
