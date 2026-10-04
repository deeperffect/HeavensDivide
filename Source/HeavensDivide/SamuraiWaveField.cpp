#include "SamuraiWaveField.h"
#include "CrescentBuild.h"
#include "Components/SceneComponent.h"
#include "EnemyBase.h"
#include "SamuraiCharacter.h"
#include "SurvivorAbilityComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"

ASamuraiWaveField::ASamuraiWaveField()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
void ASamuraiWaveField::Initialize(ASamuraiCharacter* Samurai, UPlayerUpgradeComponent* U, float WaveDamage,
    const FVector& WaveHalfExtent, AAbilityAccent* DepositedDebris)
{
    if (!Samurai || !U || !GetWorld()) { Destroy(); return; }
    Source = Samurai;
    const float Scaling = 1.f + CrescentBuild::Scaling(U, TEXT("CrescentFieldPower"), .15f);
    HitboxHalfExtent = WaveHalfExtent.GetAbs().ComponentMax(FVector(.01f));
    Remaining = FMath::Max(.01f, CrescentBuild::Value(U, TEXT("CrescentField"), TEXT("Duration"), 3.f) * Scaling);
    // Duration supplies the +15% total damage per rank. Scaling DPS as well would
    // double-dip and turn five ranks into 3.0625x total damage instead of 1.75x.
    DamagePerSecond = FMath::Max(0.f, WaveDamage * CrescentBuild::Value(U, TEXT("CrescentField"), TEXT("DamagePerSecond"), .3f));
    if (U->HasUpgradeId(TEXT("CrescentFieldPact"))) DamagePerSecond *= 1.5f;
    TotalDamage = DamagePerSecond * Remaining;
    const bool bErupt = U->HasUpgradeId(TEXT("CrescentEruptionPact"));
    const float Lifetime = bErupt ? .3f : Remaining;
    LastPulseTime = GetWorld()->GetTimeSeconds();
    // Adopt the particles deposited during outbound travel. No separate decal,
    // rectangle, or eruption flash: the rocks are the field's only presentation.
    Debris = DepositedDebris;
    if (Debris.IsValid()) Debris->ReleaseGroundSlash(Lifetime + .1f);
    if (bErupt) GetWorldTimerManager().SetTimer(DamageTimer, this, &ASamuraiWaveField::Erupt, .3f, false);
    else GetWorldTimerManager().SetTimer(DamageTimer, this, &ASamuraiWaveField::Pulse, FMath::Min(.5f, Remaining), false);
    SetLifeSpan(Lifetime + 1.f);
}
void ASamuraiWaveField::DealDamage(float Amount)
{
    if (!Source.IsValid() || Amount <= 0.f) return;
    TArray<FOverlapResult> Hits;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn); Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
    GetWorld()->OverlapMultiByObjectType(Hits, GetActorLocation(), GetActorQuat(), Objects,
        FCollisionShape::MakeBox(HitboxHalfExtent));
    TSet<AEnemyBase*> Seen;
    for (const auto& Hit : Hits)
        if (auto* Enemy = Cast<AEnemyBase>(Hit.GetActor()); Enemy && !Seen.Contains(Enemy))
        { Seen.Add(Enemy); Enemy->ApplyStatusDamage(Amount, EPlayerAttackSource::Samurai); }
}
void ASamuraiWaveField::Pulse()
{
    const double Now = GetWorld()->GetTimeSeconds();
    const float Elapsed = FMath::Clamp(static_cast<float>(Now - LastPulseTime), 0.f, Remaining);
    LastPulseTime = Now;
    DealDamage(DamagePerSecond * Elapsed);
    Remaining -= Elapsed;
    if (Remaining <= KINDA_SMALL_NUMBER || !Source.IsValid()) { Destroy(); return; }
    GetWorldTimerManager().SetTimer(DamageTimer, this, &ASamuraiWaveField::Pulse, FMath::Min(.5f, Remaining), false);
}
void ASamuraiWaveField::Erupt()
{
    DealDamage(TotalDamage);
    Destroy();
}
void ASamuraiWaveField::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(DamageTimer);
    if (Debris.IsValid()) Debris->Destroy();
    Super::EndPlay(Reason);
}
