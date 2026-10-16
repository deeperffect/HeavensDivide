#include "CinderOracleBoss.h"
#include "CharacterBase.h"
#include "SurvivorPlayerController.h"
#include "HealthComponent.h"
#include "EncounterHazard.h"
#include "EnemyAttackCue.h"
#include "CombatGround.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

FVector ACinderOracleBoss::GetVelocity() const
{
    return bCharging && !IsDead() && !bGameplaySuspended ? ChargeDirection*FMath::Max(1.f,ChargeSpeed) : AEnemyBase::GetVelocity();
}

void ACinderOracleBoss::BeginChargeSequence()
{
    StopEnemyMovement(); CastRecovery=0;
    if(CastCue.IsValid()) CastCue->Destroy();
    bChargeEnraged=HealthComponent && HealthComponent->GetHealthPercent()<EnrageHealthFraction;
    ChargesRemaining=bChargeEnraged?4:2;
    PrepareCharge(bChargeEnraged?PhaseTwoWarning:PhaseOneWarning);
}

void ACinderOracleBoss::PrepareCharge(float WarningSeconds)
{
    if(!EnsureTargetFromCharacterManager()) { CancelCharge(); return; }
    bCharging=false; bChargeHitPlayer=false; TrailDistance=0;
    ChargeWait=FMath::Max(.1f,WarningSeconds);
    ChargeDirection=(CurrentTarget->GetActorLocation()-GetActorLocation()).GetSafeNormal2D();
    if(ChargeDirection.IsNearlyZero()) ChargeDirection=GetActorForwardVector().GetSafeNormal2D();
    SetActorRotation(ChargeDirection.Rotation());
    const float Radius=GetCapsuleComponent()->GetScaledCapsuleRadius();
    // Lock a straight route per charge. Each pause acquires the player again.
    ChargeRemainingDistance=FMath::Max(1.f,ChargeDistance);
    if(!CombatGround::IsInsideArena(GetWorld(),GetActorLocation()+ChargeDirection*ChargeRemainingDistance,Radius+8))
    {
        // Clip along the actual ray. Projecting a clamped corner back onto the ray can exit the bounds.
        float Inside=0,Outside=ChargeRemainingDistance;
        for(int32 I=0;I<14;++I)
        {
            const float Mid=(Inside+Outside)*.5f;
            if(CombatGround::IsInsideArena(GetWorld(),GetActorLocation()+ChargeDirection*Mid,Radius+8))Inside=Mid;
            else Outside=Mid;
        }
        ChargeRemainingDistance=Inside;
    }
    if(ChargeWarning.IsValid()) ChargeWarning->Destroy();
    ChargeWarning=AEncounterHazard::Spawn(this,GetActorLocation()+ChargeDirection*(ChargeRemainingDistance*.5f),
        EEncounterShape::Lane,ChargeRemainingDistance+Radius*2,Radius*2,ChargeWait,0,.1f,ChargeDirection.Rotation().Yaw,0,false);
}

void ACinderOracleBoss::HitChargePlayer(FVector Before,FVector After)
{
    if(bChargeHitPlayer || !CachedSurvivorController || CachedSurvivorController->IsPlayerDead()) return;
    auto* Player=Cast<ACharacterBase>(CachedSurvivorController->GetPawn());
    if(!Player) return;
    const auto* Capsule=Player->GetCapsuleComponent();
    if(FMath::Abs(Player->GetActorLocation().Z-After.Z)>Capsule->GetScaledCapsuleHalfHeight()+GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) return;
    FVector P=Player->GetActorLocation(); P.Z=Before.Z; After.Z=Before.Z;
    if(FMath::PointDistToSegment(P,Before,After)>GetCapsuleComponent()->GetScaledCapsuleRadius()+Capsule->GetScaledCapsuleRadius()+3.f) return;
    bChargeHitPlayer=true;
    const float BeforeHP=CachedSurvivorController->GetPlayerHealthComponent()->GetCurrentHealth();
    CachedSurvivorController->ApplyDamageToPlayer(ChargeContactDamage);
    // Respect dash/swap invulnerability and damage grace; an avoided hit cannot shove the player.
    if(CachedSurvivorController->GetPlayerHealthComponent()->GetCurrentHealth()<BeforeHP && !CachedSurvivorController->IsPlayerDead())
    {
        FVector Away=(Player->GetActorLocation()-After).GetSafeNormal2D();
        if(Away.IsNearlyZero()) Away=ChargeDirection;
        Player->LaunchCharacter(Away*ChargePushSpeed+FVector(0,0,160),true,true);
    }
}

void ACinderOracleBoss::TickCharge(float Delta)
{
    if(!EnsureTargetFromCharacterManager()) { CancelCharge(); return; }
    if(!bCharging)
    {
        ChargeWait=FMath::Max(0.f,ChargeWait-Delta);
        if(ChargeWait>0) return;
        if(ChargeWarning.IsValid()) ChargeWarning->Destroy();
        bCharging=true;
        // Never catch up a long paused/hitched frame with an untelegraphed leap.
        return;
    }
    float Travel=FMath::Min(ChargeRemainingDistance,FMath::Max(1.f,ChargeSpeed)*FMath::Clamp(Delta,0.f,.15f));
    while(Travel>0 && bCharging)
    {
        const float Step=FMath::Min(Travel,80.f);
        const FVector Before=GetActorLocation();
        if(!CombatGround::IsInsideArena(GetWorld(),Before+ChargeDirection*Step,GetCapsuleComponent()->GetScaledCapsuleRadius()+4))
        { EndCharge(); break; }
        FHitResult Hit;
        AddActorWorldOffset(ChargeDirection*Step,true,&Hit);
        const FVector After=GetActorLocation();
        HitChargePlayer(Before,After);
        const float Moved=FVector::Dist2D(Before,After);
        ChargeRemainingDistance=FMath::Max(0.f,ChargeRemainingDistance-Moved);
        TrailDistance+=Moved; Travel-=Step;
        if(bChargeEnraged && TrailDistance>=160.f)
        {
            TrailDistance=FMath::Fmod(TrailDistance,160.f);
            if(auto* Trail=AEncounterHazard::Spawn(this,After,EEncounterShape::Circle,100,0,0,ChargeTrailDamage,3,0,0,false))
                Trail->ActivateAsTrail();
        }
        if(Hit.bBlockingHit || Moved<Step*.5f || ChargeRemainingDistance<=1) EndCharge();
    }
    if(ChargeRemainingDistance<=1 && bCharging) EndCharge();
}

void ACinderOracleBoss::EndCharge()
{
    bCharging=false; StopEnemyMovement(); --ChargesRemaining;
    if(ChargesRemaining>0) PrepareCharge(1.f);
    else { CastRecovery=CastRecoverySeconds; NextCast=bChargeEnraged?PhaseTwoInterval:PhaseOneInterval; }
}

void ACinderOracleBoss::CancelCharge()
{
    if(ChargeWarning.IsValid()) ChargeWarning->Destroy();
    ChargeWarning.Reset(); ChargesRemaining=0; bCharging=false; ChargeWait=0; ChargeRemainingDistance=0;
}
