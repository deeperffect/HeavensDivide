#pragma once
#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "EncounterWall.generated.h"
class UBoxComponent;
class UStaticMeshComponent;

/** Enemy damage contract makes the seal breakable by every stance. No kill/XP rewards. */
UCLASS()
class HEAVENSDIVIDE_API AEncounterWall : public AEnemyBase
{
    GENERATED_BODY()
public:
    AEncounterWall(const FObjectInitializer& Init=FObjectInitializer::Get());
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    UFUNCTION(BlueprintCallable) void SetSealed(bool bSealed);
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Stone;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Barrier;
protected:
    virtual void UpdateEnemyBehavior(float Delta) override {}
    virtual bool ShouldSkipMovement() const override { return true; }
    virtual void HandleDeath() override;
private:
    float BrokenAge=0;
};
