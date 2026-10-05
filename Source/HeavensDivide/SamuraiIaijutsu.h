#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImpactFeedback.h"
#include "SamuraiIaijutsu.generated.h"

class ASamuraiCharacter;
class UPlayerUpgradeComponent;
class ASwapAfterimage;
class UAnimMontage;
class UNiagaraSystem;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

DECLARE_DELEGATE_TwoParams(FIaijutsuAimUpdate, FVector&, FVector&);

/** Charged lane: normal attacks follow the player/aim; dash and cascade lanes stay fixed. */
UCLASS(Transient, NotBlueprintable)
class ASamuraiIaijutsu : public AActor
{
    GENERATED_BODY()
public:
    ASamuraiIaijutsu();
    void Initialize(ASamuraiCharacter* Source, UPlayerUpgradeComponent* Upgrades, FVector Direction,
        float Damage, float Distance, float Radius, float Duration, UAnimMontage* Montage,
        const FImpactFeedbackData& Feedback, UNiagaraSystem* HitVFX = nullptr,
        UMaterialInterface* IndicatorMaterial = nullptr, FLinearColor IndicatorColor = FLinearColor(0.f, .8f, 1.f));
    virtual void Tick(float DeltaSeconds) override;
    void ConfigurePathSlashes(UNiagaraSystem* System, int32 Count, float Scale, FRotator Rotation,
        float Delay, FRotator RotationRandomness);
    FSimpleDelegate OnResolved;
    FIaijutsuAimUpdate UpdateAim;
    TSharedPtr<int32> ChainBudget;
    TSharedPtr<bool> ChainTriggered;
    bool bEndpointBurst = false;
    UPROPERTY() TObjectPtr<UAnimMontage> EndpointMontage;
private:
    friend class FIaijutsuTest;
    TWeakObjectPtr<UPlayerUpgradeComponent> SourceUpgrades;
    TWeakObjectPtr<ASwapAfterimage> Visual;
    TSet<TWeakObjectPtr<AActor>> HitEnemies;
    UPROPERTY() FImpactFeedbackData Impact;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ChargeIndicator;
    UPROPERTY() TObjectPtr<UMaterialInterface> DefaultIndicatorMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ChargeMaterial;
    UPROPERTY() TObjectPtr<UNiagaraSystem> DamageVFX;
    UPROPERTY() TObjectPtr<UNiagaraSystem> PathSlashVFX;
    int32 PathSlashCount = 0;
    float PathSlashScale = 1.f;
    FRotator PathSlashRotation = FRotator::ZeroRotator;
    float PathSlashDelay = 0.f;
    FRotator PathSlashRotationRandomness = FRotator::ZeroRotator;
    void SpawnPathSlashes();
    void Vacuum(float Step);
    void UpdateLaneTransform();
    bool HitEnemy(class AEnemyBase* Enemy);
    void SpawnKillFollowUp(FVector Location);
    TWeakObjectPtr<ASamuraiCharacter> SourceCharacter;
    FVector FirstKillLocation = FVector::ZeroVector;
    bool bKilledEnemy = false;
    void ReleaseMovementHolds();
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    TSet<TWeakObjectPtr<class UEnemyLightweightMovementComponent>> HeldMovements;
    float PostHitRemaining = .3f;
    bool bResolved = false;
    FVector Origin, End;
    FVector VisualDirection = FVector::ForwardVector;
    float HitDamage = 0, HitRadius = 1, Age = 0, TravelDuration = .5f;
};
