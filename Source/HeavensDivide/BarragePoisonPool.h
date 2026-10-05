#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BarragePoisonPool.generated.h"
class UPlayerUpgradeComponent;
class UStaticMeshComponent;
UCLASS()
class HEAVENSDIVIDE_API ABarragePoisonPool : public AActor
{
 GENERATED_BODY()
public:
 ABarragePoisonPool();
 void Initialize(UPlayerUpgradeComponent* U, float AverageHitDamage);
 void Pulse();
protected:
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
 TWeakObjectPtr<UPlayerUpgradeComponent> Source;
 FTimerHandle PulseTimer;
 float HitDamage=0.f, Radius=220.f;
 int32 PulsesRemaining=6;
 bool bBloom=false;
};
