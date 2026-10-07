#include "BarragePoisonPool.h"
#include "BarrageBuild.h"
#include "EnemyBase.h"
#include "EnemyStatusEffectComponent.h"
#include "PlayerUpgradeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "Components/SceneComponent.h"
#include "NiagaraComponent.h"
#include "NinjaBuildComponent.h"
#include "NinjaCharacter.h"
#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
ABarragePoisonPool::ABarragePoisonPool()
{
 PrimaryActorTick.bCanEverTick=false;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PoolRoot")));
 Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PoisonPool"));Visual->SetupAttachment(GetRootComponent());
 PoolVFX=CreateDefaultSubobject<UNiagaraComponent>(TEXT("PoolVFX"));PoolVFX->SetupAttachment(GetRootComponent());
 PoolVFX->SetAutoActivate(false);PoolVFX->SetAutoDestroy(false);
 Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);Visual->SetCastShadow(false);
 Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
}
void ABarragePoisonPool::Initialize(UPlayerUpgradeComponent* U,float AverageHitDamage)
{
 Source=U;HitDamage=AverageHitDamage;
 Radius=BarrageBuild::Value(U,TEXT("BarragePool"),TEXT("Radius"),220.f)*(1.f+BarrageBuild::Scaling(U,TEXT("BarragePoolRadius"),.2f));
 bBloom=U&&U->HasUpgradeId(TEXT("BarrageBloom"));
 Visual->SetRelativeScale3D(FVector(Radius/50.f,Radius/50.f,.015f));
 if(auto* M=Visual->CreateAndSetMaterialInstanceDynamic(0))M->SetVectorParameterValue(TEXT("Color"),bBloom?FLinearColor(.4f,.08f,.6f):FLinearColor(.16f,.5f,.035f));
 // Project onto the ground rather than retaining the victim's capsule-center height.
 FHitResult Ground;FCollisionQueryParams Q(SCENE_QUERY_STAT(BarragePoolGround),false,this);
 Q.AddIgnoredActor(GetOwner());
 if(GetWorld()->LineTraceSingleByChannel(Ground,GetActorLocation()+FVector(0,0,50),GetActorLocation()-FVector(0,0,400),ECC_WorldStatic,Q))SetActorLocation(Ground.ImpactPoint+FVector(0,0,2));
 else AddActorWorldOffset(FVector(0,0,-85));
 PulsesRemaining=FMath::Max(1,FMath::RoundToInt(BarrageBuild::Value(U,TEXT("BarragePool"),TEXT("Duration"),3.f)/.5f));
 auto* PC=U?Cast<ASurvivorPlayerController>(U->GetOwner()):nullptr;
 auto* Ninja=PC&&PC->GetCharacterManager()?PC->GetCharacterManager()->GetNinja():nullptr;
 auto* Build=Ninja?Ninja->FindComponentByClass<UNinjaBuildComponent>():nullptr;
 auto* System=Build?(bBloom?Build->VenomBloomVFX.Get():Build->ToxicGroundVFX.Get()):nullptr;
 if(System)
 {
  PoolVFX->SetAsset(System);
  const float Scale=Radius/FMath::Max(1.f,Build->PoisonPoolVFXReferenceRadius);
  PoolVFX->SetRelativeScale3D(FVector(Scale,Scale,Scale*Build->PoisonPoolVFXHeightScale));
  // Parameters are set before activation. The owned component is destroyed with the pool.
  PoolVFX->SetVariableFloat(TEXT("User.Radius"),Radius);
  PoolVFX->SetVariableFloat(TEXT("User.Duration"),PulsesRemaining*.5f);
  PoolVFX->Activate(true);
  Visual->SetVisibility(false);
 }
 GetWorldTimerManager().SetTimer(PulseTimer,this,&ABarragePoisonPool::Pulse,.5f,true);
}
void ABarragePoisonPool::Pulse()
{
 auto* U=Source.Get();if(!U||!U->HasUpgradeId(TEXT("BarrageStance"))){Destroy();return;}
 TArray<FOverlapResult> Hits;FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
 GetWorld()->OverlapMultiByObjectType(Hits,GetActorLocation()+FVector(0,0,60),FQuat::Identity,Objects,FCollisionShape::MakeSphere(Radius),FCollisionQueryParams(SCENE_QUERY_STAT(BarragePoolPulse),false,this));
 TSet<AEnemyBase*> Seen;
 for(const auto& Hit:Hits)
 {
  auto* E=Cast<AEnemyBase>(Hit.GetActor());if(!E||E->IsDead()||Seen.Contains(E)||!E->CanReceivePlayerDamage(EPlayerAttackSource::Ninja))continue;
  Seen.Add(E);auto* S=E->GetStatusEffectComponent();if(!S)continue;
  if(bBloom)E->ApplyStatusDamage(S->CalculateRemainingStatusDamage(EEnemyStatusEffect::Poison)*BarrageBuild::Value(U,TEXT("BarrageBloom"),TEXT("RemainingDamageFraction"),.1f),EPlayerAttackSource::Ninja);
  else S->ApplyBarragePoison(U,HitDamage,true);
 }
 if(--PulsesRemaining<=0)Destroy();
}
void ABarragePoisonPool::EndPlay(const EEndPlayReason::Type Reason)
{GetWorldTimerManager().ClearTimer(PulseTimer);if(PoolVFX)PoolVFX->DestroyComponent();Super::EndPlay(Reason);}
