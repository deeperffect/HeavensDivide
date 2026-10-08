#include "CinderOracleBoss.h"
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
    Super::StartBossCombat(); bOracleActive=true; NextCast=2; SpellIndex=0;
}
void ACinderOracleBoss::StopBossCombat()
{
    bOracleActive=false; StopEnemyMovement();
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
        const float A=GetWorld()->GetTimeSeconds()*.8f+I*2*PI/3;
        Embers[I]->SetRelativeLocation(FVector(FMath::Cos(A)*145,FMath::Sin(A)*145,130+20*FMath::Sin(A*2)));
    }
    if (!bOracleActive || IsDead() || bGameplaySuspended || IsPlayerTargetDead()) return;
    CastRecovery=FMath::Max(0.f,CastRecovery-Delta); NextCast-=Delta;
    if (NextCast<=0 && EnsureTargetFromCharacterManager())
    {
        const bool PhaseTwo=HealthComponent->GetHealthPercent()<.5f;
        CastAbility(SpellIndex++ % (PhaseTwo ? 7 : 6));
        NextCast=PhaseTwo ? 4.1f : 5.2f;
    }
}
void ACinderOracleBoss::UpdateEnemyBehavior(float Delta)
{
    if (!bOracleActive || IsDead() || bGameplaySuspended || !EnsureTargetFromCharacterManager() || CastRecovery>0)
    { StopEnemyMovement(); return; }
    if (FVector::DistSquared2D(CurrentTarget->GetActorLocation(),GetActorLocation())>FMath::Square(650.f)) MoveTowardCurrentTarget();
    else { StopEnemyMovement(); FaceTarget(); }
}
void ACinderOracleBoss::CastAbility(int32 Ability)
{
    if (!bOracleActive || IsDead() || !EnsureTargetFromCharacterManager()) return;
    StopEnemyMovement(); FaceTarget(); CastRecovery=1.8f;
    if (OracleCastMontage && GetMesh()->GetAnimInstance()) GetMesh()->GetAnimInstance()->Montage_Play(OracleCastMontage);
    const FVector Player=CurrentTarget->GetActorLocation(), Center=GetActorLocation();
    const FVector Direction=(Player-Center).GetSafeNormal2D();
    const float Yaw=Direction.Rotation().Yaw;
    const float Warning=HealthComponent->GetHealthPercent()<.5f ? 1.05f : 1.35f;
    auto Circle=[&](FVector P,float R,float Delay=0.f,float Active=.2f)
    { AEncounterHazard::Spawn(this,P,EEncounterShape::Circle,R,0,Warning,SpellDamage,Active,0,Delay); };
    switch (FMath::Clamp(Ability,0,6))
    {
    case 0: // Falling embers: a sequence locks a route, leaving time to change direction.
        for (int32 I=0;I<5;++I) Circle(Player+Direction*(I-2)*175,125,I*.24f); break;
    case 1: // Three lances, with traversable gaps between them.
        for (float Angle : {-32.f,0.f,32.f})
        { const FVector D=Direction.RotateAngleAxis(Angle,FVector::UpVector);
          AEncounterHazard::Spawn(this,Center+D*650,EEncounterShape::Lane,1300,125,Warning,SpellDamage,.2f,Yaw+Angle); }
        break;
    case 2: // Hollow nova: close range is safe, unlike the following attacks.
        AEncounterHazard::Spawn(this,Center,EEncounterShape::Ring,950,340,1.5f,SpellDamage); break;
    case 3: // Cursed ground: three short-lived zones deny part of the arena.
        for (int32 I=0;I<3;++I) Circle(Player+FVector(230,0,0).RotateAngleAxis(I*120.f,FVector::UpVector),150,I*.18f,3.f); break;
    case 4: // Summon interruptible support, capped across repeated casts.
        Guardians.RemoveAll([](const auto& G){return !G.IsValid() || G->IsDead();});
        if (GuardianClass)
            for (int32 I=0;I<2 && Guardians.Num()<4;++I)
            {
                const FVector Offset=FVector(280,I==0 ? -210 : 210,0).RotateAngleAxis(Yaw,FVector::UpVector);
                FActorSpawnParameters Params; Params.Owner=this; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
                if (auto* G=GetWorld()->SpawnActor<ATacticalEnemy>(GuardianClass,Center+Offset,FRotator::ZeroRotator,Params)) Guardians.Add(G);
            }
        Circle(Center,200); break;
    case 5: // Spiral eruptions move around a fixed center rather than following the player.
        for (int32 I=0;I<8;++I) Circle(Player+FVector(200+I*32,0,0).RotateAngleAxis(I*55.f,FVector::UpVector),100,I*.2f); break;
    case 6: // Phase-two eclipse: choose an inner pocket while crossing the marked lanes.
        AEncounterHazard::Spawn(this,Player,EEncounterShape::Ring,620,290,1.7f,SpellDamage);
        for (float Angle : {45.f,135.f}) AEncounterHazard::Spawn(this,Player,EEncounterShape::Lane,1350,110,1.7f,SpellDamage,.2f,Yaw+Angle);
        break;
    }
}
