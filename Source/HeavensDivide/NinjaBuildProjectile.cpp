#include "NinjaBuildProjectile.h"

#include "AttackProjectileBase.h"
#include "AutoAttackComponent.h"
#include "CharacterManagerComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnemyBase.h"
#include "EnemyStatusEffectComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "HealthComponent.h"
#include "FangBuild.h"
#include "Kismet/GameplayStatics.h"
#include "NinjaBuildComponent.h"
#include "NinjaCharacter.h"
#include "SwapPresentationComponent.h"
#include "PlayerUpgradeComponent.h"
#include "ShadowClone.h"
#include "Sound/SoundBase.h"
#include "SurvivorPlayerController.h"
#include "UObject/ConstructorHelpers.h"

ANinjaBuildProjectile::ANinjaBuildProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Blade"));
    SetRootComponent(Visual);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Visual->SetStaticMesh(Mesh.Object);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetCastShadow(false);
    Visual->SetRelativeScale3D(FVector(.6f, .12f, .035f));
    auto *Cross = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrossBlade"));
    Cross->SetupAttachment(Visual);
    Cross->SetStaticMesh(Mesh.Object);
    Cross->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Cross->SetCastShadow(false);
    Cross->SetRelativeScale3D(FVector(.2f, 5.f, 1.f));
}

void ANinjaBuildProjectile::LaunchFang()
{
    auto *B = Build.Get();
    if (!B || !B->IsRunning() || !B->Attack() || (bCloneProjectile ? !Clone.IsValid()
        : (!bAssistProjectile && (!B->IsActive() || !B->Attack()->IsAutoAttackEnabled()))))
    {
        Destroy();
        return;
    }
    // Fang uses its own launch clock, but must respect the full swap entrance.
    // Otherwise its montage interrupts the entrance's single-node pose player.
    if (!bCloneProjectile && !bAssistProjectile && B->Ninja()->SwapPresentation
        && B->Ninja()->SwapPresentation->IsBlockingAttacks())
    {
        Destroy();
        return;
    }
    Target = bAssistProjectile ? B->Attack()->FindAssistTarget() : B->Nearest(GetActorLocation(), B->Attack()->GetEffectiveTargetingRange());
    if (!Target.IsValid())
    {
        Destroy();
        return;
    }
    auto *A = B->Attack();
    int32 Extra = FMath::Max(0, A->GetEffectiveProjectileCount() - 1);
    if (const auto *Grand = (bCloneProjectile || bAssistProjectile) ? nullptr : A->GetReadyGrandEntranceUpgrade())
    {
        Extra += FMath::Max(0, FMath::RoundToInt(Grand->GetBalanceValue(TEXT("NinjaBonusProjectiles"), 8)));
        A->bGrandEntranceReady = false;
    }
    Damage = A->GetEffectiveAttackDamage() * (1 + Extra * B->Tune(TEXT("ReturningFang"), TEXT("CountDamage"), .25f));
    Damage *= B->FangDamageMultiplier();
    Speed = B->FangSpeedMultiplier() * A->GetEffectiveProjectileSpeed() * A->GetBaseAttackInterval() / A->GetEffectiveAttackInterval();
    Speed *= B->Tune(TEXT("ReturningFang"), TEXT("FlightSpeedMultiplier"), .5f);
    if (bCloneProjectile)
        Speed *= Clone->GetFangSpeedMultiplier();
    if (B->KunaiThrowSound && (!bCloneProjectile || FlightAge > 0))
        UGameplayStatics::PlaySoundAtLocation(this, B->KunaiThrowSound, GetActorLocation());
    bReturning = false;
    bPursued = false;
    FangPursuitTargetsRemaining = 0;
    FlightAge = 0;
    FangOutwardDistance = 0;
    bFangHitThisTrip = false;
    LastHits.Reset();
    if (!bCloneProjectile && !bAssistProjectile)
    {
        A->PlayFangMontage((Target->GetActorLocation() - B->Ninja()->GetActorLocation()).Rotation());
        A->OnAutoAttack.Broadcast(A, EAutoAttackSource::NormalAutoAttack);
    }
    B->FangLaunched(this);
}

void ANinjaBuildProjectile::Finish()
{
    if (bFinished) return;
    bFinished = true;
    auto *B = Build.Get();
    if (Kind == ENinjaProjectileKind::GreatShuriken && B && B->IsRunning() && B->Has(TEXT("BreakingWheel"))) ShurikenBurst();
    Destroy();
}

void ANinjaBuildProjectile::Tick(float Delta)
{
    Super::Tick(Delta);
    auto *B = Build.Get();
    if (!B || !B->Ninja() || !B->IsRunning())
    {
        Destroy();
        return;
    }
    if (Kind == ENinjaProjectileKind::GreatShuriken) { TickShuriken(Delta); return; }
    Delta = FMath::Min(Delta, .1f);
    Age += Delta;
    FlightAge += Delta;
    SlowRemaining = FMath::Max(0.f, SlowRemaining - Delta);
    const FVector Start = GetActorLocation();
    FVector End = Start;
    if (Kind == ENinjaProjectileKind::ReturningFang)
    {
        if (!B->Has(TEXT("ReturningFang")))
        {
            Destroy();
            return;
        }
        if (bCloneProjectile && !Clone.IsValid())
        {
            Destroy();
            return;
        }
        if (!bCloneProjectile && !bAssistProjectile && (!B->IsActive() || !B->Attack()->IsAutoAttackEnabled()))
        {
            bFangHitThisTrip = false;
            bReturning = true;
        }
        if (FlightAge > 4)
            bReturning = true;
        if (!bReturning && (!Target.IsValid() || Target->IsDead()))
        {
            Target = nullptr;
            for (auto* Candidate : B->Targets(Start, B->Attack()->GetEffectiveTargetingRange()))
                if (!LastHits.Contains(Candidate)) { Target = Candidate; break; }
            if (!Target.IsValid())
                bReturning = true;
        }
        const FVector ReturnOrigin = bAssistProjectile ? AssistReturnOrigin : bCloneProjectile ? Clone->GetActorLocation() + FVector(0, 0, 60)
                                                      : B->Ninja()->GetActorLocation() + FVector(0, 0, 50);
        const FVector Goal = bReturning ? ReturnOrigin : Target->GetActorLocation() + FVector(0, 0, 45);
        const float Distance = FVector::Distance(Start, Goal);
        End = FMath::VInterpConstantTo(Start, Goal, Delta, FMath::Max(100.f, Speed));
        if (bReturning && Distance <= FMath::Max(25.f, Speed * Delta))
        {
            SetActorLocation(Goal);
            B->FangReturned(this);
            // Close range still shortens the trip, but cannot create an unlimited
            // attack rate. Wait at the owner; return procs are consumed only once.
            const bool bCanRelaunch = bCloneProjectile || (B->IsActive() && !B->Ninja()->IsDashing()
                && B->Attack()->IsAutoAttackEnabled());
            const float MinInterval = 1.f / FMath::Max(.1f, B->Tune(TEXT("ReturningFang"), TEXT("MaxLaunchesPerSecond"), 4.f));
            if (!bAssistProjectile && !bSpectralFang && bCanRelaunch && FlightAge < MinInterval)
                return;
            if(bAssistProjectile || bSpectralFang) Destroy();
            else if (bCloneProjectile)
            {
                if (Clone->CompleteFangCycle())
                    LaunchFang();
                else
                    Destroy();
            }
            else if (B->IsActive() && !B->Ninja()->IsDashing())
                LaunchFang();
            else
                Destroy();
            return;
        }
        if (!bReturning) FangOutwardDistance += FVector::Distance(Start, End);
        for (auto *E : B->Sweep(Start, End, Radius))
        {
            if (bReturning) continue; // Returning is a reset, never a damaging pass.
            // A homing Fang commits to its selected target, so spectral Fangs can reach separate enemies.
            if (E != Target.Get()) continue;
            const float Dealt = B->FangHit(this, E);
            LastHits.Add(E, Age);
            // Roll once per trip. Follow-up kills never extend the two-target chain.
            if (!bPursued && Dealt > 0 && E->IsDead() && B->Has(TEXT("FangKillingEdge")))
            {
                bPursued = true;
                if (FMath::FRand() < FMath::Clamp(B->Tune(TEXT("FangKillingEdge"), TEXT("Chance"), .15f)
                    + FangBuild::Scaling(B->Upgrades(), TEXT("FangPursuitChance"), .1f), 0.f, 1.f))
                    FangPursuitTargetsRemaining = 2;
            }
            bReturning = true;
            if (FangPursuitTargetsRemaining > 0)
                for (auto* Candidate : B->Targets(E->GetActorLocation(), B->Attack()->GetEffectiveTargetingRange()))
                    if (!LastHits.Contains(Candidate))
                    {
                        Target = Candidate;
                        --FangPursuitTargetsRemaining;
                        bReturning = false;
                        break;
                    }
            break;
        }
    }
    else
    {
        End = Start + Direction * Speed * Delta;
        if (Kind == ENinjaProjectileKind::Fragment && Age > 2)
        {
            Destroy();
            return;
        }
        for (auto *E : B->Sweep(Start, End, Radius))
        {
            if (auto *Last = LastHits.Find(E);
                Last && Age - *Last < B->Tune(TEXT("GreatShuriken"), TEXT("HitInterval"), .25f))
                continue;
            LastHits.Add(E, Age);
            int32 &Count = HitCounts.FindOrAdd(E);
            B->Hit(E,
                   Damage * (Kind == ENinjaProjectileKind::GreatShuriken && B->Has(TEXT("SerratedEdge"))
                                 ? 1 + FMath::Min(Count, 5) * .15f
                                 : 1),
                   Kind != ENinjaProjectileKind::Fragment,bAssistProjectile,Kind == ENinjaProjectileKind::GreatShuriken);
            if (Kind == ENinjaProjectileKind::Fragment)
            {
                Destroy();
                return;
            }
            if (Count == 0 && B->Has(TEXT("GrindingHalt")) && E->GetDropCategory() != EEnemyDropCategory::Normal)
                SlowRemaining = .6f;
            ++Count;
        }
    }
    SetActorLocation(End);
    if (Kind == ENinjaProjectileKind::GreatShuriken)
        AddActorLocalRotation(FRotator(0, Delta * 900, 0));
    else if (!End.Equals(Start))
        SetActorRotation((End - Start).Rotation());
}

void ANinjaBuildProjectile::UpdateShurikenVisualScale()
{
    const auto* B = Build.Get();
    if (B && B->ShurikenMesh)
    {
        // A flat XY mesh spins around Z. Uniform scaling preserves its authored proportions.
        const FVector Extent = B->ShurikenMesh->GetBounds().BoxExtent;
        const float MeshRadius = FMath::Max(1.f, FMath::Max(Extent.X, Extent.Y));
        Visual->SetWorldScale3D(FVector(Radius / MeshRadius * FMath::Max(.01f, B->ShurikenMeshScale)));
    }
    else
        Visual->SetWorldScale3D(FVector(Radius / 50.f, Radius / 250.f, .08f));
}

void ANinjaBuildProjectile::SetupKunaiPresentation()
{
    auto *B = Build.Get();
    if (!B || !B->Attack())
        return;
    const auto PresentationClass = Kind == ENinjaProjectileKind::ReturningFang && B->FangProjectileClass
        ? B->FangProjectileClass : B->Attack()->ProjectileClass;
    if (!PresentationClass) return;
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto *P = GetWorld()->SpawnActor<AAttackProjectileBase>(PresentationClass, GetActorLocation(),
                                                            GetActorRotation(), Params);
    if (!P)
        return;
    P->SetActorEnableCollision(false);
    P->SetLifeSpan(0);
    P->ProjectileMovement->StopMovementImmediately();
    P->ProjectileMovement->Deactivate();
    P->ProjectileMovement->SetComponentTickEnabled(false);
    Visual->SetVisibility(false, true);
    P->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    KunaiPresentation = P;
}

void ANinjaBuildProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto *P = KunaiPresentation.Get())
    {
        P->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        if (Reason == EEndPlayReason::Destroyed)
            P->BeginImpactTrailFade();
        else
            P->Destroy();
    }
    Super::EndPlay(Reason);
}
