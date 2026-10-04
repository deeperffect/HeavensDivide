#include "SamuraiIaijutsu.h"
#include "SamuraiCharacter.h"
#include "AutoAttackComponent.h"
#include "IaijutsuBuild.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "SwapAfterimage.h"
#include "SwapPresentationComponent.h"
#include "PlayerUpgradeComponent.h"
#include "EnemyBase.h"
#include "EnemyLightweightMovementComponent.h"
#include "EnemyStatusEffectComponent.h"
#include "HealthComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/StrongObjectPtr.h"

ASamuraiIaijutsu::ASamuraiIaijutsu()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    SetActorEnableCollision(false);
    ChargeIndicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChargeIndicator"));
    ChargeIndicator->SetupAttachment(RootComponent);
    ChargeIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ChargeIndicator->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/HeavensDivide/Materials/M_AttackIndicatorRectangle"));
    ChargeIndicator->SetStaticMesh(Plane.Object);
    DefaultIndicatorMaterial = Material.Object;
}

void ASamuraiIaijutsu::Initialize(ASamuraiCharacter* Source, UPlayerUpgradeComponent* Upgrades,
    FVector Direction, float Damage, float Distance, float Radius, float Duration,
    UAnimMontage* Montage, const FImpactFeedbackData& Feedback, UNiagaraSystem* HitVFX,
    UMaterialInterface* IndicatorMaterial, FLinearColor IndicatorColor)
{
    SourceUpgrades = Upgrades;
    SourceCharacter = Source;
    Origin = GetActorLocation();
    End = Origin + Direction.GetSafeNormal2D() * FMath::Max(1.f, Distance);
    HitDamage = FMath::Max(0.f, Damage);
    HitRadius = FMath::Max(1.f, Radius);
    TravelDuration = FMath::Max(0.f, Duration);
    Impact = Feedback;
    DamageVFX = HitVFX;
    VisualDirection = (End - Origin).GetSafeNormal2D();
    const float Length = FVector::Distance(Origin, End);
    UpdateLaneTransform();
    ChargeMaterial = UMaterialInstanceDynamic::Create(IndicatorMaterial ? IndicatorMaterial : DefaultIndicatorMaterial.Get(), this);
    if (ChargeMaterial)
    {
        ChargeIndicator->SetMaterial(0, ChargeMaterial);
        ChargeMaterial->SetScalarParameterValue(TEXT("FillAmount"), 0.f);
        ChargeMaterial->SetScalarParameterValue(TEXT("LaneAspect"), HitRadius * 2.f / Length);
        ChargeMaterial->SetVectorParameterValue(TEXT("FillColor"), IndicatorColor);
        ChargeMaterial->SetVectorParameterValue(TEXT("BorderColor"), IndicatorColor);
    }
    if (Source && Source->GetMesh() && Source->GetMesh()->GetSkeletalMeshAsset()
        && Source->SwapPresentation && Source->SwapPresentation->GhostMaterial)
    {
        FActorSpawnParameters Params;
        Params.Owner = Source;
        auto* Ghost = GetWorld()->SpawnActor<ASwapAfterimage>(Origin, Source->GetActorRotation(), Params);
        if (Ghost)
        {
            Ghost->InitializeSwordDash(Source, Source->SwapPresentation->GhostMaterial,
                Montage, Direction, TravelDuration);
            Ghost->AddTickPrerequisiteActor(this);
            Visual = Ghost;
        }
    }
}

void ASamuraiIaijutsu::ConfigurePathSlashes(UNiagaraSystem* System, int32 Count, float Scale, FRotator Rotation,
    float Delay, FRotator RotationRandomness)
{
    PathSlashVFX = System;
    PathSlashCount = FMath::Clamp(Count, 0, 12);
    PathSlashScale = FMath::Max(0.f, Scale);
    PathSlashRotation = Rotation;
    PathSlashDelay = FMath::Clamp(Delay, 0.f, 1.f);
    PathSlashRotationRandomness = FRotator(
        FMath::Clamp(FMath::Abs(RotationRandomness.Pitch), 0., 180.),
        FMath::Clamp(FMath::Abs(RotationRandomness.Yaw), 0., 180.),
        FMath::Clamp(FMath::Abs(RotationRandomness.Roll), 0., 180.));
}

void ASamuraiIaijutsu::SpawnPathSlashes()
{
    if (!PathSlashVFX || PathSlashCount == 0 || PathSlashScale <= 0.f) return;
    const float Scale = PathSlashScale * HitRadius / 100.f;
    const FQuat BaseRotation = (End - Origin).Rotation().Quaternion() * PathSlashRotation.Quaternion();
    const FNiagaraVariable ScaleParameter(FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.Scale"));
    const auto& Defaults = PathSlashVFX->GetExposedParameters();
    const bool bHasScaleParameter = Defaults.IndexOf(ScaleParameter) != INDEX_NONE;
    const float AuthoredScale = bHasScaleParameter ? Defaults.GetParameterValue<float>(ScaleParameter) : 1.f;
    // Own the effect asset until the last timer fires, without retaining the attack
    // actor or its source. The actor still resolves damage once and dies immediately.
    const auto SpawnBurst = [World = TWeakObjectPtr<UWorld>(GetWorld()),
        System = TStrongObjectPtr<UNiagaraSystem>(PathSlashVFX.Get()), Scale, bHasScaleParameter, AuthoredScale]
        (FVector Location, FRotator Rotation)
    {
        if (!World.IsValid() || World->bIsTearingDown) return;
        auto* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World.Get(), System.Get(),
            Location, Rotation, bHasScaleParameter ? FVector::OneVector : FVector(Scale), true, false);
        if (!Effect) return;
        if (bHasScaleParameter) Effect->SetVariableFloat(TEXT("User.Scale"), AuthoredScale * Scale);
        Effect->Activate();
    };
    // A private visual stream leaves FMath's combat/proc RNG unchanged.
    FRandomStream VisualRandom(static_cast<int32>(FPlatformTime::Cycles()));
    for (int32 Index = 0; Index < PathSlashCount; ++Index)
    {
        const FVector Location = FMath::Lerp(Origin, End, (Index + .5f) / PathSlashCount);
        const FRotator Variation(
            VisualRandom.FRandRange(-PathSlashRotationRandomness.Pitch, PathSlashRotationRandomness.Pitch),
            VisualRandom.FRandRange(-PathSlashRotationRandomness.Yaw, PathSlashRotationRandomness.Yaw),
            VisualRandom.FRandRange(-PathSlashRotationRandomness.Roll, PathSlashRotationRandomness.Roll));
        const FRotator Rotation = (BaseRotation * Variation.Quaternion()).Rotator();
        const float Delay = Index * PathSlashDelay;
        if (Delay <= 0.f) SpawnBurst(Location, Rotation);
        else
        {
            // Capture final transforms now: later aim/movement cannot bend the trail.
            FTimerHandle Timer;
            GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda(
                [SpawnBurst, Location, Rotation] { SpawnBurst(Location, Rotation); }), Delay, false);
        }
    }
}

void ASamuraiIaijutsu::UpdateLaneTransform()
{
    const FVector Center = (Origin + End) * .5f;
    const FRotator Rotation = (End - Origin).Rotation();
    const auto* Source = SourceCharacter.Get();
    SetActorLocationAndRotation(Origin, Rotation);
    ChargeIndicator->SetWorldLocation(Center - FVector(0, 0, Source ? Source->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 4.f : 0.f));
    ChargeIndicator->SetWorldRotation(Rotation);
    ChargeIndicator->SetWorldScale3D(FVector(FVector::Distance(Origin, End) / 100.f, HitRadius * 2.f / 100.f, 1.f));
}

void ASamuraiIaijutsu::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bResolved) return;
    if (UpdateAim.IsBound())
    {
        UpdateAim.Execute(Origin, End);
        UpdateLaneTransform();
    }
    Age += DeltaSeconds;
    VacuumElapsed += DeltaSeconds;
    if (VacuumElapsed >= .05f || Age >= TravelDuration)
    {
        Vacuum(TravelDuration <= 0.f ? .5f : VacuumElapsed);
        VacuumElapsed = 0.f;
    }
    const float Progress = TravelDuration <= 0.f ? 1.f : FMath::Clamp(Age / TravelDuration, 0.f, 1.f);
    if (Visual.IsValid())
    {
        const FVector Direction = (End - Origin).GetSafeNormal2D();
        Visual->AddActorWorldRotation(FRotator(0, FMath::FindDeltaAngleDegrees(
            VisualDirection.Rotation().Yaw, Direction.Rotation().Yaw), 0));
        VisualDirection = Direction;
        Visual->SetActorLocation(FMath::Lerp(Origin, End, Progress));
    }
    if (ChargeMaterial) ChargeMaterial->SetScalarParameterValue(TEXT("FillAmount"), Progress);
    if (Age < TravelDuration) return;
    bResolved = true;
    ChargeIndicator->SetVisibility(false);
    SpawnPathSlashes();
    if (DamageVFX)
    {
        auto* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DamageVFX,
            (Origin + End) * .5f, (End - Origin).Rotation(), FVector::OneVector, true, false);
        if (Effect)
        {
            Effect->SetVariableVec3(TEXT("User.StartPosition"), Origin);
            Effect->SetVariableVec3(TEXT("User.EndPosition"), End);
            Effect->SetVariableFloat(TEXT("User.Length"), FVector::Distance(Origin, End));
            Effect->SetVariableFloat(TEXT("User.Width"), HitRadius * 2.f);
            Effect->Activate();
        }
    }
    TArray<FHitResult> Hits;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(Iaijutsu), false, GetOwner());
    // Resolve the full indicated lane in one query, at charge completion.
    const FVector Center = (Origin + End) * .5f;
    GetWorld()->SweepMultiByObjectType(Hits, Center, Center, (End - Origin).Rotation().Quaternion(), Objects,
        FCollisionShape::MakeBox(FVector(FVector::Distance(Origin, End) * .5f, HitRadius, HitRadius)), Query);
    bool bHit = false;
    for (const auto& Hit : Hits)
    {
        auto* Enemy = Cast<AEnemyBase>(Hit.GetActor());
        if (!Enemy || Enemy->IsDead() || HitEnemies.Contains(Enemy)
            || !Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Samurai)) continue;
        HitEnemies.Add(Enemy);
        if (!HitEnemy(Enemy)) continue;
        bHit = true;
        FVector Location, Normal;
        Enemy->GetImpactContact(Origin, Location, Normal);
        UImpactFeedbackLibrary::PlayImpactFeedback(this, Impact, Location, Normal, false);
    }
    if (bEndpointBurst)
    {
        const float Radius = FMath::Max(1.f, IaijutsuBuild::Value(SourceUpgrades.Get(), TEXT("IaijutsuAOE"), TEXT("Radius"), 250.f));
        TArray<FOverlapResult> Overlaps;
        GetWorld()->OverlapMultiByObjectType(Overlaps, End, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius), Query);
        TSet<AEnemyBase*> Seen;
        for (const auto& Overlap : Overlaps)
            if (auto* Enemy = Cast<AEnemyBase>(Overlap.GetActor()); Enemy && !Seen.Contains(Enemy))
            { Seen.Add(Enemy); bHit |= HitEnemy(Enemy); }
        if (auto* Source = SourceCharacter.Get(); Source && EndpointMontage && Source->SwapPresentation)
        {
            FActorSpawnParameters Params; Params.Owner = Source;
            if (auto* Ghost = GetWorld()->SpawnActor<ASwapAfterimage>(End, (End-Origin).Rotation(), Params))
                Ghost->InitializeSwordDash(Source, Source->SwapPresentation->GhostMaterial, EndpointMontage,
                    (End-Origin).GetSafeNormal2D(), FMath::Max(.1f, EndpointMontage->GetPlayLength()));
        }
    }
    if (bKilledEnemy) SpawnKillFollowUp(FirstKillLocation);
    if (bHit && Impact.bEnableCameraShake)
        UImpactFeedbackLibrary::PlayGameplayCameraShake(this, Impact.CameraShakeClass, Impact.CameraShakeScale);
    OnResolved.ExecuteIfBound();
    Destroy();
}

void ASamuraiIaijutsu::Vacuum(float Step)
{
    const float Reach = IaijutsuBuild::VacuumReach(SourceUpgrades.Get());
    const float Speed = FMath::Max(0.f, IaijutsuBuild::Value(SourceUpgrades.Get(), TEXT("Iaijutsu"), TEXT("VacuumSpeed"), 240.f));
    if (Reach <= 0.f || Speed <= 0.f || Step <= 0.f) return;
    const FVector Forward = (End - Origin).GetSafeNormal2D();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
    const FVector Center = (Origin + End) * .5f;
    const float HalfLength = FVector::Dist2D(Origin, End) * .5f;
    const float InnerHalfLength = FMath::Max(0.f, HalfLength - 1.f);
    TArray<FOverlapResult> Overlaps;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn); Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(IaijutsuVacuum), false, GetOwner());
    GetWorld()->OverlapMultiByObjectType(Overlaps, Center, Forward.Rotation().Quaternion(), Objects,
        FCollisionShape::MakeBox(FVector(HalfLength + Reach, HitRadius + Reach, HitRadius)), Query);
    TSet<AEnemyBase*> Seen;
    for (const auto& Overlap : Overlaps)
    {
        auto* Enemy = Cast<AEnemyBase>(Overlap.GetActor());
        if (!Enemy || Seen.Contains(Enemy) || !Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Samurai)) continue;
        Seen.Add(Enemy);
        const FVector Position = Enemy->GetActorLocation();
        const FVector Offset = Position - Center;
        const float X = FVector::DotProduct(Offset, Forward), Y = FVector::DotProduct(Offset, Right);
        const FVector Closest = Center + Forward * FMath::Clamp(X, -HalfLength, HalfLength)
            + Right * FMath::Clamp(Y, -HitRadius, HitRadius);
        // Movement steps can put an enemy microscopically beyond the boundary
        // (e.g. two 30 FPS steps to exactly 60 cm); keep that boundary inclusive.
        if (FVector::DistSquared2D(Position, Closest) > FMath::Square(Reach + KINDA_SMALL_NUMBER)) continue;
        FVector Destination = Center + Forward * FMath::Clamp(X, -InnerHalfLength, InnerHalfLength)
            + Right * FMath::Clamp(Y, -HitRadius * .65f, HitRadius * .65f);
        Destination.Z = Position.Z;
        if (auto* Movement = Enemy->FindComponentByClass<UEnemyLightweightMovementComponent>())
        {
            // Chase/separation movement otherwise overcomes the 240 cm/s pull.
            // Cancel only velocity opposing the pull; movement along the lane stays free.
            const FVector Pull = Destination - Position;
            const float OpposingSpeed = FMath::Max(0.f,
                -FVector::DotProduct(Movement->GetCurrentVelocity(), Pull.GetSafeNormal2D()));
            Destination = Position + Pull.GetClampedToMaxSize((Speed + OpposingSpeed) * Step);
            FHitResult Hit;
            // Use enemy movement's world sweep: grounded capsules must not get stuck
            // on the floor, and nearby characters must not block the grouping effect.
            Movement->MoveOwnerToNoSlide(Destination, Hit);
        }
    }
}

bool ASamuraiIaijutsu::HitEnemy(AEnemyBase* Enemy)
{
    auto* U = SourceUpgrades.Get();
    if (!Enemy || !Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Samurai)) return false;
    const float HealthBefore = Enemy->GetHealthComponent()->GetCurrentHealth();
    const float ActualDamage = HitDamage * Enemy->GetIaijutsuDamageMultiplier();
    float Bonus = IaijutsuBuild::Value(U, TEXT("Iaijutsu"), TEXT("MarkBonus"), .5f)
        + IaijutsuBuild::Scaling(U, TEXT("IaijutsuMarkDamage"), .1f);
    if (U && U->HasUpgradeId(TEXT("IaijutsuMarkPact"))) Bonus *= 2.f;
    // Install before damage so even a lethal first hit qualifies for the dash refund.
    // Its own newly applied mark does not amplify the applying hit.
    Enemy->ApplyIaijutsuMark(U, Bonus, IaijutsuBuild::Value(U, TEXT("Iaijutsu"), TEXT("MarkDuration"), 3.f),
        U && U->HasUpgradeId(TEXT("IaijutsuDashPact")));
    if (!Enemy->ApplyPlayerDamage(ActualDamage / Enemy->GetIaijutsuDamageMultiplier(), EPlayerAttackSource::Samurai)) return false;
    if (U)
    {
        U->HandleSamuraiDirectHit(Enemy, ActualDamage, HealthBefore);
        if (!Enemy->IsDead() && U->HasUpgradeId(TEXT("MarkedBlade"))) Enemy->ApplyMark();
    }
    if (Enemy->IsDead() && !bKilledEnemy) { bKilledEnemy = true; FirstKillLocation = Enemy->GetActorLocation(); }
    return true;
}

void ASamuraiIaijutsu::SpawnKillFollowUp(FVector Location)
{
    auto* U = SourceUpgrades.Get();
    if (!U || !U->HasUpgradeId(TEXT("IaijutsuChain")) || !ChainBudget || *ChainBudget <= 0
        || (ChainTriggered && *ChainTriggered)) return;
    if (ChainTriggered) *ChainTriggered = true;
    --*ChainBudget;
    const auto Budget = ChainBudget;
    const auto Source = SourceCharacter;
    // Defer to avoid recursive damage/spawning when an instant cast kills.
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(U, [Source, Location, Budget, U]
    {
        auto* Samurai = Source.Get();
        auto* Attack = Samurai ? Samurai->FindComponentByClass<UAutoAttackComponent>() : nullptr;
        if (!Attack || !U->HasUpgradeId(TEXT("Iaijutsu"))) return;
        const float Range = IaijutsuBuild::Value(U, TEXT("Iaijutsu"), TEXT("DashDistance"), 800.f);
        TArray<FOverlapResult> Overlaps;
        FCollisionObjectQueryParams Objects;
        Objects.AddObjectTypesToQuery(ECC_Pawn); Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
        Samurai->GetWorld()->OverlapMultiByObjectType(Overlaps, Location, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Range));
        AEnemyBase* Target = nullptr; float Best = FMath::Square(Range);
        for (const auto& Overlap : Overlaps)
            if (auto* Enemy = Cast<AEnemyBase>(Overlap.GetActor()); Enemy && Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Samurai))
            {
                const float Distance = FVector::DistSquared2D(Location, Enemy->GetActorLocation());
                if (Distance <= Best) { Best = Distance; Target = Enemy; }
            }
        if (!Target) return;
        FVector Direction = (Target->GetActorLocation() - Location).GetSafeNormal2D();
        if (Direction.IsNearlyZero()) Direction = Samurai->GetVisualForwardVector();
        const float Charge = FMath::FRand() < IaijutsuBuild::Chance(U, TEXT("IaijutsuInstant"), TEXT("IaijutsuInstantChance"), .15f, .1f)
            ? 0.f : IaijutsuBuild::Charge(U);
        Attack->SpawnIaijutsuSlashes(Location, Direction, Range, Charge, false, Budget,
            1.f + IaijutsuBuild::Scaling(U, TEXT("IaijutsuCascadePower"), .2f));
    }));
}
