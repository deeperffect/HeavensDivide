#include "HealingUrn.h"
#include "HealingPickup.h"
#include "HealthComponent.h"
#include "EnemyLightweightMovementComponent.h"
#include "EnemyStatusEffectComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "CombatAudio.h"
#include "EngineUtils.h"

AHealingUrn::AHealingUrn(const FObjectInitializer& Init) : Super(Init)
{
    MoveSpeed=0; bDropsXP=false; XPReward=0; bUseCrowdSpread=false; bUseEnemySeparation=false;
    GetCapsuleComponent()->InitCapsuleSize(38,48);
    Pot=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HealingUrnMesh"));
    Pot->SetupAttachment(RootComponent); Pot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AHealingUrn::BeginPlay()
{
    Super::BeginPlay(); StopBehaviorUpdates(); StopSeparationUpdates(); StopEnemyMovement();
    GetMesh()->SetVisibility(false); GetMesh()->SetComponentTickEnabled(false);
    LightweightMovementComponent->SetMovementEnabled(false);
    if (auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/HeavensDivide/Materials/M_EnemyRim")))
    {
        auto* MID=UMaterialInstanceDynamic::Create(Material,this);
        MID->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.1f,1.f,.45f)); Pot->SetOverlayMaterial(MID);
    }
    SetActorTickEnabled(false);
}
void AHealingUrn::HandleDeath()
{
    if (bIsDead) return;
    bIsDead=true; StopEnemyMovement(); StopBehaviorUpdates(); StopSeparationUpdates();
    StatusEffectComponent->ClearAllStatuses(); HideHealthBar(); HideMarkIndicator();
    SetActorEnableCollision(false); Pot->SetVisibility(false);
    UCombatAudioLibrary::PlayEvent(this,TEXT("UrnBreak"),GetActorLocation(),false,.8f);
    if (HealingClass)
    {
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Pickup=GetWorld()->SpawnActor<AHealingPickup>(HealingClass,GetActorLocation()+FVector(0,0,20),FRotator::ZeroRotator,Params);
        if (Pickup) Pickup->SetLifeSpan(60);
    }
    // Cheap, short-lived ceramic fragments; no Chaos bodies or persistent debris.
    auto* FragmentMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    for (int32 I=0;I<7;++I)
    {
        auto* Fragment=NewObject<UStaticMeshComponent>(this);
        Fragment->SetupAttachment(RootComponent); Fragment->SetStaticMesh(FragmentMesh);
        Fragment->SetCollisionEnabled(ECollisionEnabled::NoCollision); Fragment->SetCastShadow(false);
        Fragment->SetMaterial(0,Pot->GetMaterial(0)); Fragment->RegisterComponent();
        Fragment->SetWorldLocation(GetActorLocation()); Fragment->SetWorldScale3D(FVector(.12f,.035f,.18f));
        Shards.Add(Fragment);
        const float A=2*PI*I/7; ShardVelocities.Add(FVector(FMath::Cos(A)*190,FMath::Sin(A)*190,FMath::FRandRange(180.f,300.f)));
    }
    SetActorTickEnabled(true); SetLifeSpan(1.4f);
}
void AHealingUrn::Tick(float Delta)
{
    if (!bIsDead) return;
    BrokenAge+=Delta;
    for (int32 I=0;I<Shards.Num();++I)
    {
        ShardVelocities[I].Z-=750*Delta;
        auto* S=Shards[I].Get(); FVector P=S->GetComponentLocation()+ShardVelocities[I]*Delta;
        if (P.Z<GetActorLocation().Z-40) { P.Z=GetActorLocation().Z-40; ShardVelocities[I]*=.3f; ShardVelocities[I].Z=FMath::Abs(ShardVelocities[I].Z); }
        S->SetWorldLocation(P); S->AddLocalRotation(FRotator(180,110,90)*Delta);
        S->SetWorldScale3D(FVector(.12f,.035f,.18f)*FMath::Clamp((1.4f-BrokenAge)/.4f,0.f,1.f));
    }
}
