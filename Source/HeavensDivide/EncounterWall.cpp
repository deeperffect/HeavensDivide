#include "EncounterWall.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnemyLightweightMovementComponent.h"
#include "EnemyStatusEffectComponent.h"
#include "HealthComponent.h"
#include "CombatAudio.h"
#include "UObject/ConstructorHelpers.h"

AEncounterWall::AEncounterWall(const FObjectInitializer& Init):Super(Init)
{
    MoveSpeed=0; bDropsXP=false; XPReward=0; bUseCrowdSpread=false; bUseEnemySeparation=false;
    GetCapsuleComponent()->InitCapsuleSize(110,120);
    Stone=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stone"));
    Stone->SetupAttachment(RootComponent); Stone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/HeavensDivide/Materials/EnvironmentPolish/M_EncounterSealStone"));
    Stone->SetStaticMesh(Cube.Object); Stone->SetMaterial(0,Material.Object);
    Stone->SetRelativeScale3D(FVector(1.1f,2.05f,1.8f));
    Stone->SetRelativeLocation(FVector(0,0,-30));
    Barrier=CreateDefaultSubobject<UBoxComponent>(TEXT("PlayerBarrier"));
    Barrier->SetupAttachment(RootComponent); Barrier->SetBoxExtent(FVector(55,120,90));
    Barrier->SetRelativeLocation(FVector(0,0,-30));
    Barrier->SetCollisionObjectType(ECC_WorldStatic);
    Barrier->SetCollisionResponseToAllChannels(ECR_Ignore);
    Barrier->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
    Barrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AEncounterWall::BeginPlay()
{
    Super::BeginPlay(); StopBehaviorUpdates(); StopSeparationUpdates(); StopEnemyMovement();
    LightweightMovementComponent->SetMovementEnabled(false);
    GetMesh()->SetVisibility(false); GetMesh()->SetComponentTickEnabled(false);
    HealthComponent->SetMaxHealthPreservePercent(60.f);
    HealthComponent->RestoreCurrentHealth(60.f);
    SetActorTickEnabled(false);
    SetSealed(false);
}
void AEncounterWall::SetSealed(bool bSealed)
{
    if (bIsDead) return;
    Barrier->SetCollisionEnabled(bSealed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    Stone->SetRelativeScale3D(FVector(1.1f,2.05f,bSealed ? 1.8f : .12f));
    Stone->SetRelativeLocation(FVector(0,0,bSealed ? -30 : -114));
}
void AEncounterWall::HandleDeath()
{
    if (bIsDead) return;
    bIsDead=true; SetActorEnableCollision(false);
    StopBehaviorUpdates(); StopSeparationUpdates(); StopEnemyMovement();
    StatusEffectComponent->ClearAllStatuses(); HideHealthBar(); HideMarkIndicator();
    UCombatAudioLibrary::PlayEvent(this,TEXT("UrnBreak"),GetActorLocation(),false,.8f);
    SetActorTickEnabled(true); SetLifeSpan(.65f);
}
void AEncounterWall::Tick(float Delta)
{
    if (!bIsDead) return;
    BrokenAge+=Delta;
    Stone->SetRelativeLocation(FVector(0,0,-FMath::Square(BrokenAge/.65f)*250));
    Stone->SetRelativeRotation(FRotator(0,0,BrokenAge*22));
}
