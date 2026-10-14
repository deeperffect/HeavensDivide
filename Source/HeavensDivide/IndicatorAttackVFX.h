#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EncounterHazard.h"
#include "IndicatorAttackVFX.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

UENUM()
enum class EIndicatorVFX : uint8 { Impact, Slash, Fire, Frost, Poison, Flash };

/** Cosmetic execution effects. Never owns a warning, collision or damage timer. */
UCLASS()
class HEAVENSDIVIDE_API AIndicatorAttackVFX : public AActor
{
    GENERATED_BODY()
public:
    AIndicatorAttackVFX();
    virtual void Tick(float DeltaSeconds) override;
    static AIndicatorAttackVFX* Spawn(AActor* Source, FVector Center, EEncounterShape Shape,
        float Size, float Width = 0, float Yaw = 0, EIndicatorVFX Style = EIndicatorVFX::Impact);
    static AIndicatorAttackVFX* BeginCharge(AActor* Source, float HitWidth, float Seconds);
    static AIndicatorAttackVFX* Release(AActor* Source, FVector Location);
    static EIndicatorVFX StyleFor(const AActor* Source);
    void StopCharge();

    struct FSample { FVector Offset; FVector Scale; float Radius; };
    // Deterministic placement, also used to verify safe centres and rotated lanes.
    static TArray<FSample> Samples(EEncounterShape Shape, float Size, float Width, FVector ViewOffset);
private:
    friend class FIndicatorAttackVFXTest;
    static AIndicatorAttackVFX* Create(AActor* Source, FVector Center, float Yaw);
    void AddBurst(FVector Location, FVector Scale, EIndicatorVFX Style, bool bGround = true);
    UPROPERTY() TArray<TObjectPtr<UNiagaraSystem>> Systems;
    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraComponent>> Bursts;
    TWeakObjectPtr<AActor> ChargeSource;
    float ChargeRadius = 0, ChargeTime = 0, SinceBurst = 0;
};
