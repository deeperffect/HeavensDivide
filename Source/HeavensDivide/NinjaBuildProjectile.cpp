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
#include "Kismet/GameplayStatics.h"
#include "NinjaBuildComponent.h"
#include "NinjaCharacter.h"
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
    if (!B || !B->IsRunning() || !B->Attack() || (bCloneProjectile ? !Clone.IsValid() : (!bAssistProjectile && !B->IsActive())))
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
    Speed = A->GetEffectiveProjectileSpeed() * A->GetBaseAttackInterval() / A->GetEffectiveAttackInterval();
    if (bCloneProjectile)
        Speed *= Clone->GetFangSpeedMultiplier();
    if (B->KunaiThrowSound && (!bCloneProjectile || FlightAge > 0))
        UGameplayStatics::PlaySoundAtLocation(this, B->KunaiThrowSound, GetActorLocation());
    bReturning = false;
    bPursued = false;
    FlightAge = 0;
    LastHits.Reset();
    if (!bCloneProjectile && !bAssistProjectile)
        A->OnAutoAttack.Broadcast(A, EAutoAttackSource::NormalAutoAttack);
}

void ANinjaBuildProjectile::Finish()
{
    auto *B = Build.Get();
    if (Kind == ENinjaProjectileKind::GreatShuriken && B && B->IsRunning() && B->Has(TEXT("BreakingWheel")))
        B->Scatter(GetActorLocation(), 6, Damage * .3f, 700);
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
        if ((!bCloneProjectile && !bAssistProjectile && !B->IsActive()) || FlightAge > 4)
            bReturning = true;
        if (!bReturning && (!Target.IsValid() || Target->IsDead()))
        {
            Target = B->Nearest(Start, B->Attack()->GetEffectiveTargetingRange());
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
            if(bAssistProjectile) Destroy();
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
        for (auto *E : B->Sweep(Start, End, Radius))
        {
            if (bReturning)
            {
                if (!B->Has(TEXT("CuttingReturn")) || LastHits.Contains(E))
                    continue;
                LastHits.Add(E, Age);
                B->Hit(E, Damage,true,bAssistProjectile);
                continue;
            }
            Streak = LastVictim == E ? FMath::Min(Streak + 1, 5) : 0;
            LastVictim = E;
            B->Hit(E, Damage * (B->Has(TEXT("RelentlessFang")) ? 1 + Streak * .15f : 1),true,bAssistProjectile);
            LastHits.Reset();
            if (E->IsDead() && B->Has(TEXT("FinalPursuit")) && !bPursued)
            {
                Target = B->Nearest(E->GetActorLocation(), 350, E);
                bPursued = true;
                if (Target.IsValid())
                    break;
            }
            bReturning = true;
            break;
        }
    }
    else
    {
        if (Kind == ENinjaProjectileKind::GreatShuriken &&
            Age >= B->GetShurikenLifetime())
        {
            Finish();
            return;
        }
        End = Start + Direction * Speed * Delta * (SlowRemaining > 0 ? .15f : 1.f);
        if (Kind == ENinjaProjectileKind::GreatShuriken && B->Has(TEXT("WideOrbit")))
        {
            if (InitialRadius <= 0)
                InitialRadius = Radius;
            TravelDistance += FVector::Distance(Start, End);
            const float Growth = FMath::Clamp(
                TravelDistance / FMath::Max(1.f, B->Tune(TEXT("WideOrbit"), TEXT("GrowthDistance"), 1000.f)), 0.f, 1.f);
            Radius =
                InitialRadius *
                FMath::Lerp(1.f, FMath::Clamp(B->Tune(TEXT("WideOrbit"), TEXT("MaxSizeMultiplier"), 2.f), 1.f, 4.f),
                            Growth);
            UpdateShurikenVisualScale();
        }
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
    if (!B || !B->Attack() || !B->Attack()->ProjectileClass)
        return;
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto *P = GetWorld()->SpawnActor<AAttackProjectileBase>(B->Attack()->ProjectileClass, GetActorLocation(),
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
