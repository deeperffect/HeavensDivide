#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SamuraiWaveField.generated.h"
class ASamuraiCharacter;
class UPlayerUpgradeComponent;
class AAbilityAccent;

/** Stationary strip covering a Crescent wave's completed path, with a finite damage budget. */
UCLASS(Transient, NotBlueprintable)
class HEAVENSDIVIDE_API ASamuraiWaveField : public AActor
{
    GENERATED_BODY()
    friend class FCrescentBuildsTest;
    friend class FGroundSlashMotionTest;
public:
    ASamuraiWaveField();
    void Initialize(ASamuraiCharacter* Samurai, UPlayerUpgradeComponent* Upgrades, float WaveDamage,
        const FVector& WaveHalfExtent, AAbilityAccent* DepositedDebris = nullptr);
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void Pulse();
    void Erupt();
    void DealDamage(float Amount);
    TWeakObjectPtr<ASamuraiCharacter> Source;
    TWeakObjectPtr<AAbilityAccent> Debris;
    FTimerHandle DamageTimer;
    FVector HitboxHalfExtent = FVector::ZeroVector;
    float DamagePerSecond = 0.f, Remaining = 0.f, TotalDamage = 0.f;
    double LastPulseTime = 0;
};
