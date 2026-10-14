#include "EnemyAttackCue.h"
#include "IndicatorAttackVFX.h"
#include "EnemyBase.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AEnemyAttackCue::AEnemyAttackCue()
{
    PrimaryActorTick.bCanEverTick=true;
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cue"));SetRootComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCastShadow(false);Mesh->SetReceivesDecals(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Surface(TEXT("/Game/HeavensDivide/Materials/M_AttackIndicatorCircle"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Energy(TEXT("/Game/HeavensDivide/Materials/M_EncounterEnergy"));
    Mesh->SetStaticMesh(Plane.Object);SurfaceTemplate=Surface.Object;EnergyTemplate=Energy.Object;
}
AEnemyAttackCue* AEnemyAttackCue::Spawn(AEnemyBase* Owner,FVector Center,float Radius,float Windup,bool bIsProjectile)
{
    if(!Owner || Owner->GetNetMode()==NM_DedicatedServer) return nullptr;
    FActorSpawnParameters P;P.Owner=Owner;
    auto* Cue=Owner->GetWorld()->SpawnActor<AEnemyAttackCue>(Center,FRotator::ZeroRotator,P);
    if(!Cue)return nullptr;
    Cue->Source=Owner;Cue->LocalOffset=Owner->GetActorTransform().InverseTransformPosition(Center);
    Cue->Duration=FMath::Max(.01f,Windup);Cue->Size=FMath::Max(.1f,Radius/50);Cue->bProjectile=bIsProjectile;
    auto* Base=bIsProjectile?Cue->EnergyTemplate.Get():Cue->SurfaceTemplate.Get();
    if(!Base){Cue->Destroy();return nullptr;}
    Cue->Material=UMaterialInstanceDynamic::Create(Base,Cue);Cue->Mesh->SetMaterial(0,Cue->Material);
    if(bIsProjectile)
    {
        Cue->Material->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1,.26f,.035f));
        Cue->Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
        Cue->Material->SetScalarParameterValue(TEXT("Solid"),1);
    }
    Cue->Mesh->SetWorldScale3D(FVector(Cue->Size,Cue->Size,bIsProjectile?Cue->Size:1));
    Cue->SetLifeSpan(FMath::Max(2.f,Windup+1)); return Cue;
}
void AEnemyAttackCue::Impact(TOptional<FVector> ReleaseLocation)
{
    if(bImpact || IsActorBeingDestroyed() || !Source.IsValid() || Source->IsDead() || Source->IsGameplaySuspended())return;
    if (bProjectile && Source.IsValid())
        AIndicatorAttackVFX::Release(Source.Get(), ReleaseLocation.Get(Source->GetActorTransform().TransformPosition(LocalOffset)));
    bImpact=true;Age=0;SetLifeSpan(.3f);
    if(Material)Material->SetScalarParameterValue(TEXT("Impact"),1);
}
void AEnemyAttackCue::Tick(float Delta)
{
    Super::Tick(Delta);
    if(!Source.IsValid() || Source->IsDead() || Source->IsGameplaySuspended()){Destroy();return;}
    Age+=Delta;
    if(!bImpact)SetActorLocation(Source->GetActorTransform().TransformPosition(LocalOffset));
    if(Material)
    {
        Material->SetScalarParameterValue(TEXT("FillAmount"),FMath::Clamp(Age/Duration,0.f,1.f));
        Material->SetScalarParameterValue(TEXT("Fade"),bImpact?1-FMath::Clamp(Age/.3f,0.f,1.f):1);
    }
    if(bProjectile)
    {
        const float Scale=Size*(bImpact?1+Age*5:.4f+.6f*FMath::Clamp(Age/Duration,0.f,1.f));
        Mesh->SetWorldScale3D(FVector(Scale));
    }
}
