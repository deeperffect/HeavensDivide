#include "TacticalEnemy.h"
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
    Cooldown = FMath::FRandRange(1.2f,2.8f);
    RoleRing->SetRelativeLocation(FVector(0,0,-GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+5));
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
    if (ChargeRemaining>0) return LockedDirection*950;
    return AEnemyBase::GetEnemyMovementVelocity();
}
bool ATacticalEnemy::ShouldForceHighAnimationBudgetSignificance() const { return ShouldSkipMovement(); }

void ATacticalEnemy::InitializeMarch(FVector Direction,float Speed,float Distance,float Delay)
{
    bMarching = true; LockedDirection = Direction.GetSafeNormal2D(); MarchSpeed = Speed;
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
        const float Speed = bMarching ? MarchSpeed : 950.f;
        const float Distance = FMath::Min(Speed*Delta,bMarching ? MarchRemaining : ChargeRemaining);
        const FVector Before = GetActorLocation(); FHitResult Hit;
        LightweightMovementComponent->MoveOwnerToNoSlide(Before+LockedDirection*Distance,Hit);
        HitAlongMovement(Before,GetActorLocation());
        if (GetMesh()) GetMesh()->SetWorldRotation(LockedDirection.Rotation()+FRotator(0,-90,0));
        if (bMarching) { MarchRemaining-=Distance; if (Hit.bBlockingHit || MarchRemaining<=0) Destroy(); }
        else if ((ChargeRemaining-=Distance)<=0 || Hit.bBlockingHit)
        { ChargeRemaining=0; Recovery=.75f; if (FollowupCharges>0) { --FollowupCharges; Cooldown=.9f; } }
        return;
    }
    if (CastRemaining>0 && (CastRemaining-=Delta)<=0) FinishAbility();
}

void ATacticalEnemy::UpdateEnemyBehavior(float Delta)
{
    if (bIsDead || bGameplaySuspended || IsPlayerTargetDead() || IsStressTestCombatDisabled()) { StopEnemyMovement(); return; }
    if (!EnsureTargetFromCharacterManager() || ShouldSkipMovement()) { StopEnemyMovement(); return; }
    LightweightMovementComponent->SetMoveSpeed(MoveSpeed*(GetWorld()->GetTimeSeconds()<HasteUntil ? 1.2f : 1.f));
    const float Distance = FVector::Dist2D(CurrentTarget->GetActorLocation(),GetActorLocation());
    if (Distance>CastRange) { MoveTowardCurrentTarget(); return; }
    StopEnemyMovement(); FaceTarget();
    if (Cooldown<=0) BeginAbility();
    else if (TacticalRole==ETacticalEnemyRole::HornLancer || TacticalRole==ETacticalEnemyRole::FangStalker || TacticalRole==ETacticalEnemyRole::OgreWarden || TacticalRole==ETacticalEnemyRole::StormGorilla)
        MoveTowardCurrentTarget();
    else if (Distance<280) RequestEnemyMovement((GetActorLocation()-CurrentTarget->GetActorLocation()).GetSafeNormal2D());
}

void ATacticalEnemy::BeginAbility()
{
    if (!CurrentTarget) return;
    LockedTarget=CurrentTarget->GetActorLocation();
    LockedDirection=(LockedTarget-GetActorLocation()).GetSafeNormal2D();
    CastRemaining=FMath::Max(.75f,Windup); Cooldown=AbilityCooldown+CastRemaining; StopEnemyMovement();
    if (CastMontage && GetMesh()->GetAnimInstance()) GetMesh()->GetAnimInstance()->Montage_Play(CastMontage);
    auto Circle = [&](FVector Position,float Radius,float Active= .18f,float Delay=0.f)
    { AEncounterHazard::Spawn(this,Position,EEncounterShape::Circle,Radius,0,CastRemaining,AbilityDamage,Active,0,Delay); };
    switch (TacticalRole)
    {
    case ETacticalEnemyRole::AshSeer: Circle(LockedTarget,155); break;
    case ETacticalEnemyRole::MireWeaver: Circle(LockedTarget,180,3.4f); break;
    case ETacticalEnemyRole::FrostOracle:
        for (int32 I=0;I<3;++I) Circle(LockedTarget+LockedDirection*I*210,115,.18f,I*.25f); break;
    case ETacticalEnemyRole::HexSniper:
        AEncounterHazard::Spawn(this,GetActorLocation()+LockedDirection*650,EEncounterShape::Lane,1300,110,CastRemaining,AbilityDamage,.2f,LockedDirection.Rotation().Yaw); break;
    case ETacticalEnemyRole::HornLancer:
    case ETacticalEnemyRole::FangStalker:
        ChargeWarning=AEncounterHazard::Spawn(this,GetActorLocation()+LockedDirection*450,EEncounterShape::Lane,900,135,CastRemaining,0,.15f,LockedDirection.Rotation().Yaw);
        break;
    case ETacticalEnemyRole::OgreWarden:
        AEncounterHazard::Spawn(this,GetActorLocation(),EEncounterShape::Ring,410,160,CastRemaining,AbilityDamage); break;
    case ETacticalEnemyRole::StormGorilla:
        for (float Angle : {0.f,90.f}) AEncounterHazard::Spawn(this,GetActorLocation(),EEncounterShape::Lane,1050,155,CastRemaining,AbilityDamage,.18f,LockedDirection.Rotation().Yaw+Angle);
        break;
    default: RoleRing->SetWorldScale3D(FVector(8,8,1)); break;
    }
}

void ATacticalEnemy::FinishAbility()
{
    CastRemaining=0; Recovery=.45f;
    if (TacticalRole==ETacticalEnemyRole::HornLancer || TacticalRole==ETacticalEnemyRole::FangStalker)
    {
        ChargeRemaining=900; bChargeHit=false;
        if (TacticalRole==ETacticalEnemyRole::FangStalker)
        { if (!bSecondDash) FollowupCharges=1; bSecondDash=!bSecondDash; }
        if (ChargeWarning.IsValid()) ChargeWarning->Destroy();
    }
    if (TacticalRole==ETacticalEnemyRole::GraveCantor || TacticalRole==ETacticalEnemyRole::WarDrummer)
    {
        int32 Count=0;
        for (TActorIterator<AEnemyBase> It(GetWorld()); It && Count<8; ++It)
            if (*It!=this && !It->IsDead() && FVector::DistSquared2D(It->GetActorLocation(),GetActorLocation())<FMath::Square(420.f))
            {
                if (TacticalRole==ETacticalEnemyRole::GraveCantor) It->GetHealthComponent()->Heal(It->GetHealthComponent()->GetMaxHealth()*.05f);
                else if (auto* Ally=Cast<ATacticalEnemy>(*It)) Ally->GrantHaste(4.f);
                else continue;
                ++Count;
            }
        RoleRing->SetWorldScale3D(FVector(1.4f,1.4f,1));
    }
}

void ATacticalEnemy::StopEnemyBehavior()
{
    CastRemaining=0; ChargeRemaining=0; StopEnemyMovement();
    if (ChargeWarning.IsValid()) ChargeWarning->Destroy();
    for (TActorIterator<AEncounterHazard> It(GetWorld()); It; ++It)
        if (It->GetOwner()==this) It->Destroy();
}
void ATacticalEnemy::HandleDeath() { if (RoleRing) RoleRing->SetVisibility(false); Super::HandleDeath(); }
