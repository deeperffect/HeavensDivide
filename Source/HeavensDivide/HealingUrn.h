#pragma once
#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "HealingUrn.generated.h"
class AHealingPickup;
class UStaticMeshComponent;

/** Uses the combat hit interface so every existing player weapon can break it. */
UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AHealingUrn : public AEnemyBase
{
    GENERATED_BODY()
public:
    AHealingUrn(const FObjectInitializer& Init = FObjectInitializer::Get());
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Pot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<AHealingPickup> HealingClass;
protected:
    virtual void UpdateEnemyBehavior(float Delta) override {}
    virtual bool ShouldSkipMovement() const override { return true; }
    virtual bool ShouldUseWorldHealthBar() const override { return false; }
    virtual void HandleDeath() override;
private:
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Shards;
    TArray<FVector> ShardVelocities;
    float BrokenAge = 0;
};
