#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaBuildProjectile.generated.h"
class UNinjaBuildComponent;
class AEnemyBase;
class AShadowClone;
class AAttackProjectileBase;
class UStaticMeshComponent;

enum class ENinjaProjectileKind : uint8
{
    ReturningFang,
    GreatShuriken,
    Fragment
};

UCLASS()
class HEAVENSDIVIDE_API ANinjaBuildProjectile : public AActor
{
    GENERATED_BODY()
  public:
    ANinjaBuildProjectile();
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void SetupKunaiPresentation();
    void UpdateShurikenVisualScale();
    TWeakObjectPtr<AAttackProjectileBase> KunaiPresentation;
    TWeakObjectPtr<AShadowClone> Clone;
    bool bCloneProjectile = false;
    bool bAssistProjectile = false;
    FVector AssistReturnOrigin = FVector::ZeroVector;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
    TWeakObjectPtr<UNinjaBuildComponent> Build;
    TWeakObjectPtr<AEnemyBase> Target;
    TWeakObjectPtr<AEnemyBase> LastVictim;
    TMap<TWeakObjectPtr<AEnemyBase>, float> LastHits;
    TMap<TWeakObjectPtr<AEnemyBase>, int32> HitCounts;
    // 0 = returning fang, 1 = great shuriken, 2 = scattered fragment.
    ENinjaProjectileKind Kind = ENinjaProjectileKind::ReturningFang;
    int32 Streak = 0;
    float Damage = 0, Speed = 1000, Radius = 14, Age = 0, FlightAge = 0, SlowRemaining = 0;
    float InitialRadius = 0, TravelDistance = 0;
    FVector Direction = FVector::ForwardVector;
    bool bReturning = false, bPursued = false;
    void LaunchFang();
    void Finish();
};
