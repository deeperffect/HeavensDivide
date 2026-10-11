#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MagnetPickup.generated.h"
class UStaticMeshComponent;
class UMaterialInterface;
class ASurvivorPlayerController;
class UMinimapMarkerComponent;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AMagnetPickup : public AActor
{
    GENERATED_BODY()
public:
    AMagnetPickup();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Magnet", meta=(ClampMin="20")) float CollectRadius=100;
    UFUNCTION(BlueprintCallable, Category="Magnet") int32 Collect(ASurvivorPlayerController* Player);
private:
    UPROPERTY() TObjectPtr<UMinimapMarkerComponent> MinimapMarker;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Pieces;
    UPROPERTY() TObjectPtr<USceneComponent> Symbol;
    UPROPERTY() TObjectPtr<UMaterialInterface> EnergyMaterial;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Ring;
    bool bConsumed=false;
    float Age=0;
};
