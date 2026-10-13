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
#include "Engine/StaticMesh.h"

AEncounterWall::AEncounterWall(const FObjectInitializer& Init):Super(Init)
{
    MoveSpeed=0; bDropsXP=false; XPReward=0; bUseCrowdSpread=false; bUseEnemySeparation=false;
    GetCapsuleComponent()->InitCapsuleSize(110,120);
    Stone=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stone"));
    Stone->SetupAttachment(RootComponent); Stone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    const TCHAR* Names[]={TEXT("SM_PondRock01a"),TEXT("SM_PondRock02a"),TEXT("SM_PondRock03a"),
        TEXT("SM_Rock_Ground_01a"),TEXT("SM_Rock_Ground_02a"),TEXT("SM_Rock_Ground_03a")};
    for (const TCHAR* Name:Names)
    {
        const FString Path=FString::Printf(TEXT("/Game/Assets/Environment/JapaneseShrine/Meshes/%s.%s"),Name,Name);
        ConstructorHelpers::FObjectFinder<UStaticMesh> Rock(*Path);
        if (Rock.Succeeded()) RockMeshes.Add(Rock.Object);
    }
    Barrier=CreateDefaultSubobject<UBoxComponent>(TEXT("PlayerBarrier"));
    Barrier->SetupAttachment(RootComponent); Barrier->SetBoxExtent(FVector(55,120,90));
    Barrier->SetRelativeLocation(FVector(0,0,-30));
    Barrier->SetCollisionObjectType(ECC_WorldStatic);
    Barrier->SetCollisionResponseToAllChannels(ECR_Ignore);
    Barrier->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
    Barrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ConfigureRock(0);
}
void AEncounterWall::ConfigureRock(int32 VariantIndex)
{
    if (RockMeshes.IsEmpty()) return;
    const int32 Index=FMath::Abs(VariantIndex)%RockMeshes.Num();
    UStaticMesh* Rock=RockMeshes[Index];
    if (!Rock) return;
    Stone->SetStaticMesh(Rock);
    // Preserve each rock's authored material. Stand the flat pond rock on its edge.
    FRandomStream Variation(VariantIndex+173);
    RaisedRotation=FRotator(Rock->GetName()==TEXT("SM_PondRock02a")?90.f:0.f,Variation.FRandRange(-12.f,12.f),0);
    const FBox Rotated=Rock->GetBoundingBox().TransformBy(FTransform(RaisedRotation));
    const FVector Size=Rotated.GetSize().ComponentMax(FVector(1));
    RockHeight=Variation.FRandRange(185.f,220.f);
    const FVector FitScale=FVector(130.f,240.f,RockHeight)/Size;
    // A yaw changes fitted extents slightly; pitch only swaps the flat rock's X/Z axes.
    const FVector Scale=RaisedRotation.Pitch!=0 ? FVector(FitScale.Z,FitScale.Y,FitScale.X) : FitScale;
    Stone->SetRelativeScale3D(Scale);
    const FBox Fitted=Rock->GetBoundingBox().TransformBy(FTransform(RaisedRotation,FVector::ZeroVector,Scale));
    RaisedLocation=FVector(-Fitted.GetCenter().X,-Fitted.GetCenter().Y,
        -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()-8.f-Fitted.Min.Z);
    Stone->SetRelativeRotation(RaisedRotation);
    SetRevealProgress(0.f);
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
    SetRevealProgress(bSealed?1.f:0.f);
}
void AEncounterWall::SetRevealProgress(float Progress)
{
    if (bIsDead) return;
    const float Alpha=FMath::Clamp(Progress,0.f,1.f);
    const float Eased=1.f-FMath::Square(1.f-Alpha);
    Barrier->SetCollisionEnabled(Alpha>0 ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    Stone->SetVisibility(Alpha>0);
    Stone->SetRelativeLocation(RaisedLocation-FVector(0,0,(1.f-Eased)*(RockHeight+20.f)));
}
void AEncounterWall::HandleDeath()
{
    if (bIsDead) return;
    bIsDead=true; SetActorEnableCollision(false);
    BrokenLocation=Stone->GetRelativeLocation();
    StopBehaviorUpdates(); StopSeparationUpdates(); StopEnemyMovement();
    StatusEffectComponent->ClearAllStatuses(); HideHealthBar(); HideMarkIndicator();
    UCombatAudioLibrary::PlayEvent(this,TEXT("UrnBreak"),GetActorLocation(),false,.8f);
    SetActorTickEnabled(true); SetLifeSpan(.65f);
}
void AEncounterWall::Tick(float Delta)
{
    if (!bIsDead) return;
    BrokenAge+=Delta;
    Stone->SetRelativeLocation(BrokenLocation-FVector(0,0,FMath::Square(BrokenAge/.65f)*250));
    Stone->SetRelativeRotation(RaisedRotation+FRotator(0,0,BrokenAge*22));
}
