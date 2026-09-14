#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SwapAfterimage.generated.h"
class ACharacterBase;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UAnimMontage;
class USkeletalMeshComponent;
UCLASS(NotBlueprintable, Transient)
class ASwapAfterimage : public AActor
{
    GENERATED_BODY()
public:
    ASwapAfterimage();
    void Initialize(ACharacterBase* Source, UMaterialInterface* Material, FLinearColor Color, float Duration);
    bool InitializeDeparture(ACharacterBase* Source, UMaterialInterface* Material, FLinearColor Color,
        UAnimMontage* Montage, float PlayRate, float FadeDuration = .2f);
    virtual void Tick(float DeltaSeconds) override;
private:
    friend class FSwapDepartureTest;
    void AdvanceVisual(float DeltaSeconds);
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FadeMaterial;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> AnimatedMesh;
    float DepartureRate = 1;
    float DepartureDuration = 0;
    bool bDepartureFinished = false;
    float Age = 0, Lifetime = .2f;
};
