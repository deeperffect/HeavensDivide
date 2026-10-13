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
#include "BossSpellGeometry.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

ACinderOracleBoss::ACinderOracleBoss(const FObjectInitializer& Init) : Super(Init)
{
    BossDisplayName=FText::FromString(TEXT("VESHRA - THE PALE STINGER"));
    BossMaxHealth=32000; BossMoveSpeed=150; Phase2HealthThreshold=0; BossBaseDamage=22;
    PushbackMultiplier=0.f;
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
    // The boss keeps its authored skin and cannot be displaced by player hits.
    PushbackMultiplier=0.f;
    GetMesh()->SetOverlayMaterial(nullptr);
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/HeavensDivide/Materials/M_EnemyRim")))
    {
        auto* MID=UMaterialInstanceDynamic::Create(Base,this);
        MID->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.62f,.82f,.32f));
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
    for (auto& Mob:Horde) if (Mob.IsValid()) Mob->Destroy();
    Guardians.Reset(); Horde.Reset(); Super::StopBossCombat();
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
          AEncounterHazard::Spawn(this,Center+D*(LanceLength*.5f),EEncounterShape::Lane,LanceLength,LanceWidth,Warning,SpellDamage,BurstActiveTime,Yaw+Angle,0,false); }
        break;
    case 2: // Hollow nova: close range is safe, unlike the following attacks.
        AEncounterHazard::Spawn(this,Center,EEncounterShape::Ring,NovaRadius,FMath::Min(NovaSafeRadius,NovaRadius),NovaWarning,SpellDamage,.18f); break;
    case 3: // Snapshot the visible floor, leaving small paths between the fields.
        CastScreenGround(Player,Warning); break;
    case 4: // Summon interruptible support, capped across repeated casts.
        Guardians.RemoveAll([](const auto& G){return !G.IsValid() || G->IsDead();});
        if (GuardianClass)
            for (int32 I=0;I<FMath::Clamp(GuardiansPerCast,1,16) && Guardians.Num()<FMath::Clamp(GuardianLimit,1,32);++I)
            {
                const FVector Offset=FVector(GuardianForwardOffset,(I%2==0?-1.f:1.f)*GuardianSideOffset*(1+I/2),0).RotateAngleAxis(Yaw,FVector::UpVector);
                if (auto* G=Cast<ATacticalEnemy>(SpawnSummon(GuardianClass.Get(),Center+Offset))) Guardians.Add(G);
            }
        Horde.RemoveAll([](const auto& M){return !M.IsValid() || M->IsDead();});
        if (!HordeClasses.IsEmpty())
            for (int32 I=0;I<FMath::Clamp(HordePerCast,0,128) && Horde.Num()<FMath::Clamp(HordeLimit,0,128);++I)
            {
                const float Angle=I*137.508f+Yaw;
                const float Radius=FMath::Max(100.f,HordeSpawnRadius)+100.f*(I/10);
                const FVector Offset=FVector(Radius,0,0).RotateAngleAxis(Angle,FVector::UpVector);
                if (auto* M=SpawnSummon(HordeClasses[I%HordeClasses.Num()],Center+Offset)) Horde.Add(M);
            }
        Circle(Center,SummonBlastRadius); break;
    case 5: // Spiral eruptions move around a fixed center rather than following the player.
        for (int32 I=0;I<FMath::Clamp(SpiralCount,1,32);++I) Circle(Player+FVector(SpiralStartRadius+I*SpiralRadiusStep,0,0).RotateAngleAxis(I*SpiralAngleStep,FVector::UpVector),SpiralBlastRadius,I*SpiralStagger); break;
    case 6: // Phase-two eclipse: choose an inner pocket while crossing the marked lanes.
        {
            float ArenaRadius=0;
            CombatGround::ArenaCoveringRadius(GetWorld(),Player,ArenaRadius);
            const float Outer=FMath::Max(EclipseOuterRadius,ArenaRadius+256.f);
            AEncounterHazard::Spawn(this,Player,EEncounterShape::Ring,Outer,FMath::Min(EclipseInnerRadius,Outer),EclipseWarning,SpellDamage,.18f,0,0,false);
            for (float Angle : {45.f,135.f}) AEncounterHazard::Spawn(this,Player,EEncounterShape::Lane,EclipseLaneLength,EclipseLaneWidth,EclipseWarning,SpellDamage,BurstActiveTime,Yaw+Angle,0,false);
        }
        break;
    }
}

AEnemyBase* ACinderOracleBoss::SpawnSummon(TSubclassOf<AEnemyBase> Class,FVector Desired)
{
    if (!Class) return nullptr;
    const auto* Capsule=Class->GetDefaultObject<AEnemyBase>()->GetCapsuleComponent();
    const float Radius=Capsule->GetScaledCapsuleRadius(), Height=Capsule->GetScaledCapsuleHalfHeight();
    for (int32 Attempt=0;Attempt<24;++Attempt)
    {
        FVector Candidate=Desired;
        if (Attempt>0) Candidate+=FVector(140.f*FMath::Sqrt(float(Attempt)),0,0).RotateAngleAxis(Attempt*137.508f,FVector::UpVector);
        Candidate=CombatGround::ClampInsideArena(GetWorld(),Candidate,Radius+10.f);
        if (!CombatGround::IsInsideArena(GetWorld(),Candidate,Radius+10.f)) continue;
        if (CurrentTarget && FVector::DistSquared2D(Candidate,CurrentTarget->GetActorLocation())<FMath::Square(Radius+150.f)) continue;
        FVector Surface;
        if (!CombatGround::FindSurface(GetWorld(),Candidate,this,Surface)) continue;
        Surface.Z+=Height;
        FActorSpawnParameters Params; Params.Owner=this;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
        if (auto* Mob=GetWorld()->SpawnActor<AEnemyBase>(Class,Surface,FRotator::ZeroRotator,Params))
        {
            if (!CombatGround::IsInsideArena(GetWorld(),Mob->GetActorLocation(),Mob->GetCapsuleComponent()->GetScaledCapsuleRadius())) { Mob->Destroy(); continue; }
            Mob->DisablePermanentCurrencyReward();
            Mob->SetTarget(CurrentTarget);
            return Mob;
        }
    }
    return nullptr;
}

void ACinderOracleBoss::CastScreenGround(FVector Player,float Warning)
{
    TArray<FVector> Corners,Centers;
    if (bGroundFillsScreen)
    {
        FVector Surface=Player;
        CombatGround::FindSurface(GetWorld(),Player,this,Surface);
        const auto* Character=Cast<ACharacter>(CurrentTarget);
        auto* PC=Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
        if (!PC) PC=UGameplayStatics::GetPlayerController(this,0);
        if (!BossSpellGeometry::GroundViewCorners(PC,Surface.Z,Corners))
        {
            // Dedicated/headless worlds still have a usable local pattern.
            FMinimalViewInfo View; View.Location=Surface+FVector(-900,0,1400);
            View.Rotation=(Surface-View.Location).Rotation(); View.FOV=70; View.AspectRatio=16.f/9;
            BossSpellGeometry::GroundViewCorners(View,View.AspectRatio,Surface.Z,Corners);
        }
        BossSpellGeometry::ScreenCircleCenters(Corners,GroundRadius*FMath::Max(2.1f,GroundScreenSpacingMultiplier),GroundMaxScreenCircles,Centers);
    }
    if (Centers.IsEmpty())
        for (int32 I=0;I<FMath::Clamp(GroundCount,1,32);++I)
            Centers.Add(Player+FVector(GroundSpreadRadius,0,0).RotateAngleAxis(I*360.f/FMath::Max(1,GroundCount),FVector::UpVector));
    for (int32 I=0;I<Centers.Num();++I)
    {
        if (!CombatGround::IsInsideArena(GetWorld(),Centers[I],0)) continue;
        const float Delay=bGroundFillsScreen ? FMath::Max(0.f,GroundScreenSweepSeconds)*I/FMath::Max(1,Centers.Num()-1) : I*GroundStagger;
        AEncounterHazard::Spawn(this,Centers[I],EEncounterShape::Circle,GroundRadius,0,Warning,SpellDamage,GroundDuration,0,Delay,false);
    }
}
