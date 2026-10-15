#include "TacticalEnemy.h"
#include "IndicatorAttackVFX.h"
#include "EnemyAttackCue.h"
#include "EnemySpawner.h"
#include "BloodshiftBalanceData.h"
#include "EncounterHazard.h"
#include "HealthComponent.h"
#include "EnemyLightweightMovementComponent.h"
#include "SurvivorPlayerController.h"
#include "CharacterBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ATacticalEnemy::ATacticalEnemy(const FObjectInitializer& Init) : Super(Init)
{
    RoleRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoleRing"));
    RoleRing->SetupAttachment(RootComponent);
    RoleRing->SetCollisionEnabled(ECollisionEnabled::NoCollision); RoleRing->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    RoleRing->SetStaticMesh(Plane.Object);
}

void ATacticalEnemy::BeginPlay()
{
    Super::BeginPlay();
    Cooldown = FMath::FRandRange(InitialCooldownMin,FMath::Max(InitialCooldownMin,InitialCooldownMax));
    RoleRing->SetWorldLocation(GetActorLocation()+FVector(0,0,-GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+5));
    RoleRing->SetWorldScale3D(FVector(1.4f,1.4f,1));
    if (auto* Base = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/HeavensDivide/Materials/M_EnemyRoleRing")))
    {
        auto* MID = UMaterialInstanceDynamic::Create(Base,this);
        MID->SetVectorParameterValue(TEXT("Tint"),RoleColor); RoleRing->SetMaterial(0,MID);
    }
    if (auto* Base = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/HeavensDivide/Materials/M_EnemyRim")))
    {
        auto* MID = UMaterialInstanceDynamic::Create(Base,this);
        MID->SetVectorParameterValue(TEXT("Tint"),RoleColor); GetMesh()->SetOverlayMaterial(MID);
    }
}

void ATacticalEnemy::ApplySpawnDifficultyScaling(float Health, float Damage)
{ Super::ApplySpawnDifficultyScaling(Health,Damage); AbilityDamage *= FMath::Max(0.f,Damage); }
void ATacticalEnemy::ApplySpawnInstanceModifiers(float Health, float Damage, float Speed)
{ Super::ApplySpawnInstanceModifiers(Health,Damage,Speed); AbilityDamage *= FMath::Max(0.f,Damage); }
void ATacticalEnemy::GrantHaste(float Duration) { HasteUntil = GetWorld()->GetTimeSeconds()+Duration; }
bool ATacticalEnemy::ShouldSkipMovement() const { return Super::ShouldSkipMovement() || CastRemaining>0 || ChargeRemaining>0 || Recovery>0 || bMarching; }
FVector ATacticalEnemy::GetVelocity() const
{
    if (bIsDead || bGameplaySuspended) return FVector::ZeroVector;
    if (bMarching && MarchDelay<=0) return LockedDirection*MarchSpeed;
    if (ChargeRemaining>0) return LockedDirection*ChargeSpeed;
    return AEnemyBase::GetEnemyMovementVelocity();
}
bool ATacticalEnemy::ShouldForceHighAnimationBudgetSignificance() const { return ShouldSkipMovement(); }

void ATacticalEnemy::InitializeMarch(FVector Direction,float Speed,float Distance,float Delay)
{
    bMarching = true; LockedDirection = Direction.GetSafeNormal2D(); MarchSpeed = Speed * GetRunChallengeSpeedMultiplier();
    MarchRemaining = Distance; MarchDelay = Delay; bChargeHit = false;
    StopEnemyMovement(); StopSeparationUpdates(); bUseEnemySeparation = false; bUseCrowdSpread = false;
    RoleRing->SetVisibility(false);
    SetLifeSpan(Delay+Distance/FMath::Max(1.f,Speed)+3);
}

void ATacticalEnemy::HitAlongMovement(FVector Before,FVector After)
{
    if (bChargeHit || !CachedSurvivorController || CachedSurvivorController->IsPlayerDead()) return;
    auto* Player = Cast<ACharacterBase>(CachedSurvivorController->GetPawn());
    if (!Player) return;
    FVector P = Player->GetActorLocation();
    if (FMath::Abs(P.Z-After.Z)>180) return;
    P.Z=Before.Z; After.Z=Before.Z;
    if (FMath::PointDistToSegment(P,Before,After) <= GetCapsuleComponent()->GetScaledCapsuleRadius()+Player->GetCapsuleComponent()->GetScaledCapsuleRadius())
    { CachedSurvivorController->ApplyDamageToPlayer(AbilityDamage); bChargeHit=true; }
}

void ATacticalEnemy::Tick(float Delta)
{
    Super::Tick(Delta);
    if (bIsDead || bGameplaySuspended || IsPlayerTargetDead()) return;
    Cooldown = FMath::Max(0.f,Cooldown-Delta); Recovery = FMath::Max(0.f,Recovery-Delta);
    if (bMarching || ChargeRemaining>0)
    {
        if (bMarching && MarchDelay>0) { MarchDelay-=Delta; return; }
        const float Speed = bMarching ? MarchSpeed : FMath::Max(1.f,ChargeSpeed);
        const float Distance = FMath::Min(Speed*Delta,bMarching ? MarchRemaining : ChargeRemaining);
        const FVector Before = GetActorLocation(); FHitResult Hit;
        if (bMarching)
            if (auto* Spawner=Cast<AEnemySpawner>(GetOwner()))
            {
                if (!Spawner->IsEncounterInsideArena(Before+LockedDirection*Distance)) { Destroy(); return; }
            }
        LightweightMovementComponent->MoveOwnerToNoSlide(Before+LockedDirection*Distance,Hit);
        HitAlongMovement(Before,GetActorLocation());
        // Rotate the actor; never overwrite a species' authored mesh rotation.
        SetActorRotation(LockedDirection.Rotation());
        if (bMarching) { MarchRemaining-=Distance; if (Hit.bBlockingHit || MarchRemaining<=0) Destroy(); }
        else if ((ChargeRemaining-=Distance)<=0 || Hit.bBlockingHit)
        { ChargeRemaining=0; if (ChargeVFX.IsValid()) ChargeVFX->StopCharge(); ChargeVFX.Reset(); Recovery=ChargeRecoverySeconds; if (FollowupCharges>0) { --FollowupCharges; Cooldown=FollowupCooldown; } }
        return;
    }
    if (CastRemaining>0 && (CastRemaining-=Delta)<=0) FinishAbility();
}

float ATacticalEnemy::GetEngagementRange() const
{
    // Self-centered attacks must be able to reach the position where we stop.
    // Targeted circles and charges use their authored cast range.
    if (TacticalRole==ETacticalEnemyRole::OgreWarden) return FMath::Max(1.f,FMath::Min(CastRange,WardenOuterRadius));
    if (TacticalRole==ETacticalEnemyRole::StormGorilla) return FMath::Max(1.f,FMath::Min(CastRange,StormLength*.5f));
    return FMath::Max(1.f,CastRange);
}

void ATacticalEnemy::UpdateEnemyBehavior(float Delta)
{
    if (bIsDead || bGameplaySuspended || IsPlayerTargetDead() || IsStressTestCombatDisabled()) { StopEnemyMovement(); return; }
    if (!EnsureTargetFromCharacterManager() || ShouldSkipMovement()) { StopEnemyMovement(); return; }
    LightweightMovementComponent->SetMoveSpeed(MoveSpeed*(GetWorld()->GetTimeSeconds()<HasteUntil ? HasteMultiplier : 1.f));
    const float Distance = FVector::Dist2D(CurrentTarget->GetActorLocation(),GetActorLocation());
    if (Distance>GetEngagementRange() || !HasClearChaseRoute()) { MoveTowardCurrentTarget(); return; }
    StopEnemyMovement(); FaceTarget();
    if (Cooldown<=0) BeginAbility();
    // Hold attack range during cooldown. Resume chasing only when the target
    // leaves it; approaching the enemy must never make it retreat or orbit.
}

void ATacticalEnemy::BeginAbility()
{
    if (!CurrentTarget) return;
    LockedTarget=CurrentTarget->GetActorLocation();
    LockedDirection=(LockedTarget-GetActorLocation()).GetSafeNormal2D();
    CastRemaining=FMath::Max3(MinimumWindup,Windup,UBloodshiftBalanceData::Get()->MinimumWarning); Cooldown=AbilityCooldown+CastRemaining; StopEnemyMovement();
    UpdateAnimationBudgetSignificance();
    if(CastCue.IsValid())CastCue->Destroy();
    CastCue=AEnemyAttackCue::Spawn(this,GetActorLocation()+FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*.35f),28,CastRemaining,true);
    if (CastMontage && GetMesh()->GetAnimInstance()) GetMesh()->GetAnimInstance()->Montage_Play(CastMontage);
    auto Circle = [&](FVector Position,float Radius,float Active= -1.f,float Delay=0.f)
    { AEncounterHazard::Spawn(this,Position,EEncounterShape::Circle,Radius,0,CastRemaining,AbilityDamage,Active<0?BurstActiveTime:Active,0,Delay); };
    switch (TacticalRole)
    {
    case ETacticalEnemyRole::AshSeer: Circle(LockedTarget,SeerRadius); break;
    case ETacticalEnemyRole::MireWeaver: Circle(LockedTarget,MireRadius,MireDuration); break;
    case ETacticalEnemyRole::FrostOracle:
        for (int32 I=0;I<FMath::Clamp(FrostCount,1,32);++I) Circle(LockedTarget+LockedDirection*I*FrostSpacing,FrostRadius,BurstActiveTime,I*FrostStagger); break;
    case ETacticalEnemyRole::HexSniper:
        AEncounterHazard::Spawn(this,GetActorLocation()+LockedDirection*(SniperLength*.5f),EEncounterShape::Lane,SniperLength,SniperWidth,CastRemaining,AbilityDamage,SniperActiveTime,LockedDirection.Rotation().Yaw); break;
    case ETacticalEnemyRole::HornLancer:
    case ETacticalEnemyRole::FangStalker:
        ChargeWarning=AEncounterHazard::Spawn(this,GetActorLocation()+LockedDirection*(ChargeDistance*.5f),EEncounterShape::Lane,ChargeDistance,FMath::Max(ChargeWidth,GetCapsuleComponent()->GetScaledCapsuleRadius()*2),CastRemaining,0,ChargeDistance/FMath::Max(1.f,ChargeSpeed)+.2f,LockedDirection.Rotation().Yaw);
        if (!ChargeWarning.IsValid()) { CastRemaining=0; Recovery=CastRecoverySeconds; }
        break;
    case ETacticalEnemyRole::OgreWarden:
        AEncounterHazard::Spawn(this,GetActorLocation(),EEncounterShape::Ring,WardenOuterRadius,FMath::Min(WardenInnerRadius,WardenOuterRadius),CastRemaining,AbilityDamage,BurstActiveTime); break;
    case ETacticalEnemyRole::StormGorilla:
        for (float Angle : {0.f,StormCrossAngle}) AEncounterHazard::Spawn(this,GetActorLocation(),EEncounterShape::Lane,StormLength,StormWidth,CastRemaining,AbilityDamage,BurstActiveTime,LockedDirection.Rotation().Yaw+Angle);
        break;
    default: RoleRing->SetWorldScale3D(FVector(SupportRadius/50,SupportRadius/50,1)); break;
    }
}

void ATacticalEnemy::FinishAbility()
{
    CastRemaining=0; Recovery=CastRecoverySeconds;
    if ((TacticalRole == ETacticalEnemyRole::HornLancer || TacticalRole == ETacticalEnemyRole::FangStalker)
        && (!ChargeWarning.IsValid() || !ChargeWarning->HasVisibleWarning())) return;
    if(CastCue.IsValid())CastCue->Impact();
    UpdateAnimationBudgetSignificance();
    if (TacticalRole==ETacticalEnemyRole::HornLancer || TacticalRole==ETacticalEnemyRole::FangStalker)
    {
        if (!ChargeWarning.IsValid() || !ChargeWarning->HasVisibleWarning()) return;
        ChargeRemaining=FMath::Max(1.f,ChargeDistance); bChargeHit=false;
        ChargeVFX = AIndicatorAttackVFX::BeginCharge(this, GetCapsuleComponent()->GetScaledCapsuleRadius() * 2,
            ChargeRemaining / FMath::Max(1.f, ChargeSpeed));
        if (TacticalRole==ETacticalEnemyRole::FangStalker)
        { if (!bSecondDash) FollowupCharges=1; bSecondDash=!bSecondDash; }
        // The locked lane remains visible throughout the dash.
    }
    if (TacticalRole==ETacticalEnemyRole::GraveCantor || TacticalRole==ETacticalEnemyRole::WarDrummer)
    {
        AIndicatorAttackVFX::Spawn(this, GetActorLocation(), EEncounterShape::Circle, SupportRadius, 0, 0,
            TacticalRole == ETacticalEnemyRole::GraveCantor ? EIndicatorVFX::Poison : EIndicatorVFX::Impact);
        int32 Count=0;
        for (TActorIterator<AEnemyBase> It(GetWorld()); It && Count<FMath::Clamp(SupportTargets,1,64); ++It)
            if (*It!=this && !It->IsDead() && FVector::DistSquared2D(It->GetActorLocation(),GetActorLocation())<FMath::Square(SupportRadius))
            {
                if (TacticalRole==ETacticalEnemyRole::GraveCantor) It->GetHealthComponent()->Heal(It->GetHealthComponent()->GetMaxHealth()*FMath::Clamp(HealFraction,0.f,1.f));
                else if (auto* Ally=Cast<ATacticalEnemy>(*It)) Ally->GrantHaste(HasteDuration);
                else continue;
                ++Count;
            }
        RoleRing->SetWorldScale3D(FVector(1.4f,1.4f,1));
    }
}

void ATacticalEnemy::StopEnemyBehavior()
{
    CastRemaining=0; ChargeRemaining=0; Recovery=0; StopEnemyMovement();
    if (ChargeVFX.IsValid()) ChargeVFX->StopCharge(); ChargeVFX.Reset();
    if(CastCue.IsValid())CastCue->Destroy();
    if (RoleRing && !bMarching) RoleRing->SetWorldScale3D(FVector(1.4f,1.4f,1));
    if (GetMesh() && GetMesh()->GetAnimInstance()) GetMesh()->GetAnimInstance()->Montage_Stop(.1f);
    if (ChargeWarning.IsValid()) ChargeWarning->Destroy();
    for (TActorIterator<AEncounterHazard> It(GetWorld()); It; ++It)
        if (It->GetOwner()==this) It->Destroy();
}
void ATacticalEnemy::HandleDeath() { if (RoleRing) RoleRing->SetVisibility(false); Super::HandleDeath(); }
