#include "CinderOracleBoss.h"
#include "EnemyAttackCue.h"
#include "EncounterHazard.h"
#include "TacticalEnemy.h"
#include "HealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "CombatGround.h"

ACinderOracleBoss::ACinderOracleBoss(const FObjectInitializer& Init) : Super(Init)
{
    BossDisplayName=FText::FromString(TEXT("KAGUTSU — THE CINDER ORACLE"));
    BossMaxHealth=32000; BossMoveSpeed=150; Phase2HealthThreshold=0; BossBaseDamage=22;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    for(int32 I=0;I<3;++I)
    {
        auto* Ember=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("OrbitingEmber%d"),I));
        Ember->SetupAttachment(RootComponent);Ember->SetStaticMesh(Sphere.Object);
        Ember->SetCollisionEnabled(ECollisionEnabled::NoCollision);Ember->SetCastShadow(false);
        Ember->SetRelativeScale3D(FVector(.32f));Embers.Add(Ember);
    }
}
void ACinderOracleBoss::BeginPlay()
{
    Super::BeginPlay();
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/HeavensDivide/Materials/M_EnemyRim")))
    {
        auto* MID=UMaterialInstanceDynamic::Create(Base,this);
        MID->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1,.18f,.6f));GetMesh()->SetOverlayMaterial(MID);
        for(auto Ember:Embers) Ember->SetMaterial(0,MID);
    }
}
void ACinderOracleBoss::StartBossCombat()
{
    Super::StartBossCombat(); bOracleActive=true; NextCast=InitialCastDelay; SpellIndex=0;
}
void ACinderOracleBoss::StopBossCombat()
{
    bOracleActive=false; StopEnemyMovement();
    if(CastCue.IsValid())CastCue->Destroy();
    for (TActorIterator<AEncounterHazard> It(GetWorld()); It; ++It) if (It->GetOwner()==this) It->Destroy();
    for (auto& Guardian:Guardians) if (Guardian.IsValid()) Guardian->Destroy();
    Guardians.Reset(); Super::StopBossCombat();
}
void ACinderOracleBoss::HandleDeath() { StopBossCombat(); for(auto Ember:Embers) Ember->SetVisibility(false); Super::HandleDeath(); }
void ACinderOracleBoss::Tick(float Delta)
{
    // This boss owns a spell deck; the swordsman's montage state machine is not ticked.
    AEnemyBase::Tick(Delta);
    if(!IsDead()) for(int32 I=0;I<Embers.Num();++I)
    {
        const float A=GetWorld()->GetTimeSeconds()*(CastRecovery>0?2.8f:.8f)+I*2*PI/3;
        Embers[I]->SetRelativeLocation(FVector(FMath::Cos(A)*145,FMath::Sin(A)*145,130+20*FMath::Sin(A*2)));
        Embers[I]->SetRelativeScale3D(FVector(CastRecovery>0 ? .60f : .32f));
    }
    if (!bOracleActive || IsDead() || bGameplaySuspended || IsPlayerTargetDead()) return;
    CastRecovery=FMath::Max(0.f,CastRecovery-Delta); NextCast-=Delta;
    if(CastCue.IsValid() && CastRecovery<=.45f)CastCue->Impact();
    if (NextCast<=0 && EnsureTargetFromCharacterManager())
    {
        const bool PhaseTwo=HealthComponent->GetHealthPercent()<EnrageHealthFraction;
        CastAbility(SpellIndex++ % (PhaseTwo ? 7 : 6));
        NextCast=PhaseTwo ? PhaseTwoInterval : PhaseOneInterval;
    }
}
void ACinderOracleBoss::UpdateEnemyBehavior(float Delta)
{
    if (!bOracleActive || IsDead() || bGameplaySuspended || !EnsureTargetFromCharacterManager() || CastRecovery>0)
    { StopEnemyMovement(); return; }
    if (FVector::DistSquared2D(CurrentTarget->GetActorLocation(),GetActorLocation())>FMath::Square(PreferredDistance)) MoveTowardCurrentTarget();
    else { StopEnemyMovement(); FaceTarget(); }
}
void ACinderOracleBoss::CastAbility(int32 Ability)
{
    if (!bOracleActive || IsDead() || !EnsureTargetFromCharacterManager()) return;
    StopEnemyMovement(); FaceTarget(); CastRecovery=CastRecoverySeconds;
    if(CastCue.IsValid())CastCue->Destroy();
    CastCue=AEnemyAttackCue::Spawn(this,GetActorLocation()+FVector(0,0,80),55,FMath::Max(.1f,CastRecoverySeconds-.45f),true);
    if (OracleCastMontage && GetMesh()->GetAnimInstance()) GetMesh()->GetAnimInstance()->Montage_Play(OracleCastMontage);
    const FVector Player=CurrentTarget->GetActorLocation(), Center=GetActorLocation();
    const FVector Direction=(Player-Center).GetSafeNormal2D();
    const float Yaw=Direction.Rotation().Yaw;
    const float Warning=HealthComponent->GetHealthPercent()<EnrageHealthFraction ? PhaseTwoWarning : PhaseOneWarning;
    auto Circle=[&](FVector P,float R,float Delay=0.f,float Active=-1.f)
    { AEncounterHazard::Spawn(this,P,EEncounterShape::Circle,R,0,Warning,SpellDamage,Active<0?BurstActiveTime:Active,0,Delay); };
    switch (FMath::Clamp(Ability,0,6))
    {
    case 0: // Falling embers: a sequence locks a route, leaving time to change direction.
        for (int32 I=0;I<FMath::Clamp(EmberCount,1,32);++I) Circle(Player+Direction*(I-(EmberCount-1)*.5f)*EmberSpacing,EmberRadius,I*EmberStagger); break;
    case 1: // Three lances, with traversable gaps between them.
        for (float Angle : {-LanceSideAngle,0.f,LanceSideAngle})
        { const FVector D=Direction.RotateAngleAxis(Angle,FVector::UpVector);
          AEncounterHazard::Spawn(this,Center+D*(LanceLength*.5f),EEncounterShape::Lane,LanceLength,LanceWidth,Warning,SpellDamage,BurstActiveTime,Yaw+Angle); }
        break;
    case 2: // Hollow nova: close range is safe, unlike the following attacks.
        AEncounterHazard::Spawn(this,Center,EEncounterShape::Ring,NovaRadius,FMath::Min(NovaSafeRadius,NovaRadius),NovaWarning,SpellDamage,.18f); break;
    case 3: // Cursed ground: three short-lived zones deny part of the arena.
        for (int32 I=0;I<FMath::Clamp(GroundCount,1,32);++I) Circle(Player+FVector(GroundSpreadRadius,0,0).RotateAngleAxis(I*360.f/FMath::Max(1,GroundCount),FVector::UpVector),GroundRadius,I*GroundStagger,GroundDuration); break;
    case 4: // Summon interruptible support, capped across repeated casts.
        Guardians.RemoveAll([](const auto& G){return !G.IsValid() || G->IsDead();});
        if (GuardianClass)
            for (int32 I=0;I<FMath::Clamp(GuardiansPerCast,1,16) && Guardians.Num()<FMath::Clamp(GuardianLimit,1,32);++I)
            {
                const FVector Offset=FVector(GuardianForwardOffset,(I%2==0?-1.f:1.f)*GuardianSideOffset*(1+I/2),0).RotateAngleAxis(Yaw,FVector::UpVector);
                FVector SpawnLocation;
                const auto* CapsuleDefaults=GuardianClass->GetDefaultObject<ATacticalEnemy>()->GetCapsuleComponent();
                const float CapsuleScale=CapsuleDefaults->GetRelativeScale3D().GetAbsMax();
                if (!CombatGround::IsInsideArena(GetWorld(),Center+Offset,CapsuleDefaults->GetUnscaledCapsuleRadius()*CapsuleScale)) continue;
                if (!CombatGround::FindSurface(GetWorld(),Center+Offset,this,SpawnLocation)) continue;
                SpawnLocation.Z+=CapsuleDefaults->GetUnscaledCapsuleHalfHeight()*CapsuleScale;
                FActorSpawnParameters Params; Params.Owner=this; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
                if (auto* G=GetWorld()->SpawnActor<ATacticalEnemy>(GuardianClass,SpawnLocation,FRotator::ZeroRotator,Params))
                {
                    if (!CombatGround::IsInsideArena(GetWorld(),G->GetActorLocation(),G->GetCapsuleComponent()->GetScaledCapsuleRadius())) { G->Destroy(); continue; }
                    G->DisablePermanentCurrencyReward();
                    Guardians.Add(G);
                }
            }
        Circle(Center,SummonBlastRadius); break;
    case 5: // Spiral eruptions move around a fixed center rather than following the player.
        for (int32 I=0;I<FMath::Clamp(SpiralCount,1,32);++I) Circle(Player+FVector(SpiralStartRadius+I*SpiralRadiusStep,0,0).RotateAngleAxis(I*SpiralAngleStep,FVector::UpVector),SpiralBlastRadius,I*SpiralStagger); break;
    case 6: // Phase-two eclipse: choose an inner pocket while crossing the marked lanes.
        AEncounterHazard::Spawn(this,Player,EEncounterShape::Ring,EclipseOuterRadius,FMath::Min(EclipseInnerRadius,EclipseOuterRadius),EclipseWarning,SpellDamage,.18f);
        for (float Angle : {45.f,135.f}) AEncounterHazard::Spawn(this,Player,EEncounterShape::Lane,EclipseLaneLength,EclipseLaneWidth,EclipseWarning,SpellDamage,BurstActiveTime,Yaw+Angle);
        break;
    }
}
