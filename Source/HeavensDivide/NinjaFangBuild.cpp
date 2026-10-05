#include "NinjaBuildComponent.h"
#include "AutoAttackComponent.h"
#include "AttackProjectileBase.h"
#include "Components/StaticMeshComponent.h"
#include "EnemyBase.h"
#include "FangBuild.h"
#include "HealthComponent.h"
#include "InactiveCharacterAssistComponent.h"
#include "NinjaCharacter.h"
#include "NiagaraFunctionLibrary.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorAbilityComponent.h"
#include "SwapPresentationComponent.h"
#include "Engine/World.h"

float UNinjaBuildComponent::FangDamageMultiplier() const
{
    return (Has(TEXT("FangRedline")) ? Tune(TEXT("FangRedline"), TEXT("DamageMultiplier"), .7f) : 1.f);
}

float UNinjaBuildComponent::FangSpeedMultiplier() const
{
    return (1.f + FangBuild::Scaling(Upgrades(), TEXT("FangSpeed"), .15f))
        * (Has(TEXT("FangFarstrider")) ? Tune(TEXT("FangFarstrider"), TEXT("SpeedMultiplier"), .75f) : 1.f)
        * (Has(TEXT("FangRedline")) ? Tune(TEXT("FangRedline"), TEXT("SpeedMultiplier"), 1.6f) : 1.f);
}

void UNinjaBuildComponent::FangLaunched(ANinjaBuildProjectile* P)
{
    if (!P || P->bSpectralFang || P->bCloneProjectile || P->bAssistProjectile) return;
    ++FangLaunchCount;
    const int32 Interval = FMath::Max(1, 4 - Upgrades()->GetUpgradeLevelById(TEXT("FangTwinFrequency")));
    if (!Has(TEXT("FangTwin")) || FangLaunchCount % Interval != 0) return;
    auto Candidates = Targets(P->GetActorLocation(), Attack()->GetEffectiveTargetingRange());
    Candidates.Remove(P->Target.Get());
    for (int32 Index = 0; Index < 2; ++Index)
    {
        auto* TargetEnemy = Candidates.IsValidIndex(Index) ? Candidates[Index] : P->Target.Get();
        if (!TargetEnemy) continue;
        auto* Extra = SpawnBlade(ENinjaProjectileKind::ReturningFang, P->GetActorLocation());
        if (!Extra) continue;
        Extra->bSpectralFang = true;
        Extra->Target = TargetEnemy;
        Extra->Damage = P->Damage;
        Extra->Speed = P->Speed;
        auto* Material = SpectralFangMaterial.Get();
        if (!Material && Ninja()->SwapPresentation) Material = Ninja()->SwapPresentation->GhostMaterial;
        if (Material)
            if (auto* Presentation = Extra->KunaiPresentation.Get())
            {
                TInlineComponentArray<UMeshComponent*> Meshes(Presentation);
                for (auto* Mesh : Meshes)
                    for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
                        Mesh->SetMaterial(Slot, Material);
            }
    }
}

float UNinjaBuildComponent::FangHit(ANinjaBuildProjectile* P, AEnemyBase* Enemy)
{
    if (!P || !Enemy || Enemy->IsDead() || !Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Ninja)
        || !Enemy->GetHealthComponent() || !Enemy->GetHealthComponent()->IsDamageEnabled()) return 0;
    P->Streak = P->LastVictim == Enemy ? FMath::Min(P->Streak + 1, 5) : 0;
    P->LastVictim = Enemy;
    const float Pressure = 1.f + FangBuild::Scaling(Upgrades(), TEXT("FangPressure"), .2f);
    float HitDamage = P->Damage;
    if (Has(TEXT("RelentlessFang")))
        HitDamage *= 1.f + FMath::Min(P->Streak * Tune(TEXT("RelentlessFang"), TEXT("DamagePerHit"), .15f),
            Tune(TEXT("RelentlessFang"), TEXT("MaxBonus"), .75f)) * Pressure;
    if (FVector::DistSquared2D(Ninja()->GetActorLocation(), Enemy->GetActorLocation())
        <= FMath::Square(Tune(TEXT("FangCloseQuarters"), TEXT("Radius"), 300.f)))
        HitDamage *= 1.f + FangBuild::Scaling(Upgrades(), TEXT("FangCloseQuarters"), .1f);
    if (Enemy->GetHealthComponent()->GetHealthPercent() <= Tune(TEXT("FangKillingEdge"), TEXT("HealthThreshold"), .3f))
        HitDamage *= 1.f + FangBuild::Scaling(Upgrades(), TEXT("FangKillingEdge"), .15f);
    if (Has(TEXT("FangFarstrider")))
        HitDamage *= 1.f + FMath::Clamp(P->FangOutwardDistance / FMath::Max(1.f,
            Tune(TEXT("FangFarstrider"), TEXT("DistanceForMaximum"), 1000.f)), 0.f, 1.f)
            * Tune(TEXT("FangFarstrider"), TEXT("MaxDamageBonus"), 1.5f);
    const bool Critical = Has(TEXT("FangDeadeye")) && FMath::FRand() < FMath::Clamp(
        Tune(TEXT("FangDeadeye"), TEXT("Chance"), .15f)
        + FangBuild::Scaling(Upgrades(), TEXT("FangCriticalChance"), .1f), 0.f, 1.f);
    if (Critical) HitDamage *= Tune(TEXT("FangDeadeye"), TEXT("DamageMultiplier"), 2.f)
        * (Has(TEXT("FangPredator")) ? Tune(TEXT("FangPredator"), TEXT("CriticalMultiplier"), 1.5f) : 1.f);
    else if (Has(TEXT("FangPredator"))) HitDamage *= Tune(TEXT("FangPredator"), TEXT("NoncriticalMultiplier"), .75f);
    const int32 Count = ++FangVictimHits.FindOrAdd(Enemy);
    const FVector Position = Enemy->GetActorLocation() + FVector(0, 0, 45);
    const float Dealt = Hit(Enemy, HitDamage, false, P->bAssistProjectile);
    P->bFangHitThisTrip |= Dealt > 0;
    if (Enemy->IsDead())
    {
        FangVictimHits.Remove(Enemy);
        if (Dealt > 0 && Has(TEXT("FangSplinter")))
            ScatterFangKunai(Position, Count, P->Damage * Tune(TEXT("FangSplinter"), TEXT("DamageFraction"), .4f)
                * (1.f + FangBuild::Scaling(Upgrades(), TEXT("FangSplinterPower"), .2f)));
    }
    return Dealt;
}

void UNinjaBuildComponent::FangReturned(ANinjaBuildProjectile* P)
{
    // Recalls caused by swaps, canceled trips, clones and assists grant no player return procs.
    if (!P || !P->bFangHitThisTrip || P->bCloneProjectile || P->bAssistProjectile || !IsActive()) return;
    P->bFangHitThisTrip = false;
    if (Has(TEXT("FangResonance")))
    {
        const FVector Origin = Ninja()->GetActorLocation();
        const float Radius = Tune(TEXT("FangResonance"), TEXT("Radius"), 250.f)
            * (1.f + FangBuild::Scaling(Upgrades(), TEXT("FangResonantReach"), .15f));
        const float Damage = P->Damage * Tune(TEXT("FangResonance"), TEXT("DamageFraction"), .5f);
        for (auto* Enemy : Targets(Origin, Radius)) Hit(Enemy, Damage, false);
        if (FangReturnBurstVFX)
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FangReturnBurstVFX, Origin, FRotator::ZeroRotator,
                FVector(Radius / 250.f));
        else if (auto* FX = Upgrades()->GetOwner()->FindComponentByClass<USurvivorAbilityComponent>())
            FX->UpgradeAccent(TEXT("FangResonance"), Origin, Radius, FLinearColor(.65f,.25f,1.f));
    }
    if (!P->bSpectralFang && Has(TEXT("FangAssist")) && GetWorld()->GetTimeSeconds() >= NextFangAssistTime
        && FMath::FRand() < FMath::Clamp(Tune(TEXT("FangAssist"), TEXT("Chance"), .05f)
            + FangBuild::Scaling(Upgrades(), TEXT("FangAssistChance"), .05f), 0.f, 1.f))
        if (auto* Assist = Upgrades()->GetOwner()->FindComponentByClass<UInactiveCharacterAssistComponent>())
            if (Assist->TryFangAssist())
                NextFangAssistTime = GetWorld()->GetTimeSeconds() + Tune(TEXT("FangAssist"), TEXT("Cooldown"), 1.f);
}

void UNinjaBuildComponent::ScatterFangKunai(FVector Position, int32 Count, float Damage)
{
    PendingFangScatter.Add({Position, Count, Damage});
    ProcessFangScatter();
}

void UNinjaBuildComponent::ProcessFangScatter()
{
    // Reuse ordinary straight-flying fragment presentation. Queue large kill counts instead of truncating them.
    int32 Budget = 16;
    while (Budget-- > 0 && !PendingFangScatter.IsEmpty())
    {
        auto& Event = PendingFangScatter[0];
        auto* P = SpawnBlade(ENinjaProjectileKind::Fragment, Event.Position);
        if (!P) return;
        const auto Enemies = Targets(Event.Position, Tune(TEXT("FangSplinter"), TEXT("Range"), 700.f));
        P->Damage = Event.Damage;
        P->Speed = 1400.f;
        P->Direction = Enemies.IsEmpty() ? FVector::ForwardVector.RotateAngleAxis(Event.Emitted * 137.5f, FVector::UpVector)
            : (Enemies[Event.Emitted % Enemies.Num()]->GetActorLocation() + FVector(0,0,45) - Event.Position).GetSafeNormal();
        ++Event.Emitted;
        if (--Event.Remaining <= 0) PendingFangScatter.RemoveAt(0);
    }
}
