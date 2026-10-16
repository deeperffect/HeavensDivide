#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ObjectiveBeaconComponent.generated.h"

class UStaticMeshComponent;
class UMaterialBillboardComponent;
class UMaterialInterface;

/** Two cheap, shader-animated surfaces. No lights, particles or per-frame component work. */
UCLASS()
class HEAVENSDIVIDE_API UObjectiveBeaconComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UObjectiveBeaconComponent();
    void Configure(UStaticMeshComponent* Visual, FLinearColor Color);
    void SetBeaconState(bool bVisible, bool bAvailable);
    bool IsArrowVisible() const;
    bool IsAuraVisible() const;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> SourceVisual;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Aura;
    UPROPERTY() TObjectPtr<UMaterialBillboardComponent> Arrow;
    UPROPERTY() TObjectPtr<UMaterialInterface> AuraMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> ArrowMaterial;
    UPROPERTY() FLinearColor Tint = FLinearColor(1.f,.65f,.08f);
    bool bBeaconVisible = false;
    bool bBeaconAvailable = true;
};
