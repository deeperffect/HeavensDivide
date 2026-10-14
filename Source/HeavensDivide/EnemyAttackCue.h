#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyAttackCue.generated.h"
class AEnemyBase;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/** Cosmetic only: damage remains owned by the attack/animation notify. */
UCLASS()
class HEAVENSDIVIDE_API AEnemyAttackCue : public AActor
{
    GENERATED_BODY()
public:
    AEnemyAttackCue();
    static AEnemyAttackCue* Spawn(AEnemyBase* Source,FVector Center,float Radius,float Windup,bool bProjectile=false);
    void Impact(TOptional<FVector> ReleaseLocation = {});
    virtual void Tick(float Delta) override;
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<UMaterialInterface> SurfaceTemplate;
    UPROPERTY() TObjectPtr<UMaterialInterface> EnergyTemplate;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
    TWeakObjectPtr<AEnemyBase> Source;
    FVector LocalOffset;
    float Age=0,Duration=1,Size=1;
    bool bImpact=false,bProjectile=false;
};
