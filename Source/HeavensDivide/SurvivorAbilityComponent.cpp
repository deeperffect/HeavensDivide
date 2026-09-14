#include "SurvivorAbilityComponent.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "CharacterStatsComponent.h"
#include "SharedPlayerStatsComponent.h"
#include "PlayerUpgradeComponent.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "SurvivorPlayerController.h"
#include "EnemyStatusEffectComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace {
 const TCHAR* Families[]={TEXT("SteelTempest"),TEXT("Heavenfall"),TEXT("NightThread"),TEXT("VenomGarden")};
 const TCHAR* Evolutions[]={TEXT("RazorHalo"),TEXT("Starfall"),TEXT("BlackWeb"),TEXT("WitheringGarden")};
 const FLinearColor Colors[]={FLinearColor(3,1.2f,0.12f),FLinearColor(2,2,0.65f),FLinearColor(1.7f,0.2f,3),FLinearColor(0.3f,2,0.55f)};
}
AAbilityAccent::AAbilityAccent()
{
 PrimaryActorTick.bCanEverTick=true;
 Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Accent")); SetRootComponent(Visual);
 Niagara=CreateDefaultSubobject<UNiagaraComponent>(TEXT("UpgradeNiagara"));Niagara->SetupAttachment(Visual);
 Niagara->SetAutoActivate(false);Niagara->SetAutoDestroy(false);Niagara->SetAbsolute(false,false,true);
 Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visual->SetCastShadow(false);
 // These tiny, short-lived primitives do not benefit from Nanite.
 Visual->SetForceDisableNanite(true);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
 Visual->SetStaticMesh(Plane.Object);
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Ring(TEXT("/Game/HeavensDivide/Materials/M_AbilityRing"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Beam(TEXT("/Game/HeavensDivide/Materials/M_AbilityStreak"));
 RingMaterial=Ring.Object; BeamMaterial=Beam.Object;
}
void AAbilityAccent::Initialize(FVector End,float Radius,FLinearColor Color,float Duration,bool bBeam,const FUpgradePresentation* Settings,int32 Stage)
{
 const FVector Start=GetActorLocation();
 if(Settings)End+=Settings->EndOffset;
 if(Settings)Duration=(Settings->LifetimeOverride>0?Settings->LifetimeOverride:Duration)*FMath::Max(0.01f,Settings->LifetimeMultiplier);
 Lifetime=FMath::Max(0.05f,Duration); bIsBeam=bBeam;
 if(bBeam)
 {
  Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  const FVector Delta=End-GetActorLocation();
  SetActorLocation(GetActorLocation()+Delta*0.5f); SetActorRotation(Delta.Rotation());
  InitialScale=FVector(FMath::Max(1.0f,Delta.Size())/100.0f,0.035f,0.035f);
 }
 else InitialScale=FVector(Radius/50.0f,Radius/50.0f,1);
 SetActorScale3D(InitialScale);
 if(Settings)
 {
  FallbackIntensityParameter=Settings->FallbackIntensityParameter;bFadeFallback=Settings->bFadeFallback;
  if(Settings->bOverrideColor)Color=Settings->Color;
  if(Settings->FallbackRingMaterial)RingMaterial=Settings->FallbackRingMaterial;
  if(Settings->FallbackLineMaterial)BeamMaterial=Settings->FallbackLineMaterial;
  if(bBeam){InitialScale.Y=InitialScale.Z=FMath::Max(0.001f,Settings->FallbackLineThickness/100.0f);SetActorScale3D(InitialScale);}
  InitialScale*=Settings->Scale;SetActorScale3D(InitialScale);AddActorLocalRotation(Settings->RotationOffset);
  UNiagaraSystem* System=Stage==1?Settings->WarningSystem:Stage==2?Settings->ImpactSystem:Stage==3?Settings->DetonationSystem:bBeam?Settings->LineSystem:Settings->PulseSystem;
  if(!System&&Stage==3)System=Settings->PulseSystem;
  Visual->SetVisibility(System?Settings->bShowFallbackWithNiagara:Settings->bShowFallbackWithoutNiagara,false);
  PresentationOffset=Settings->LocationOffset;AddActorWorldOffset(PresentationOffset);
  if(System)
  {
   Niagara->SetAsset(System);Niagara->SetWorldLocation(Start+Settings->LocationOffset);
   Niagara->SetWorldRotation((bBeam?(End-Start).Rotation():FRotator::ZeroRotator)+Settings->RotationOffset);
   const float AreaScale=Settings->bScaleSystemToRadius&&Radius>0?Radius/FMath::Max(1.0f,Settings->AuthoredRadius):1;
   Niagara->SetWorldScale3D(Settings->Scale*AreaScale);
   if(!Settings->RadiusParameter.IsNone())Niagara->SetVariableFloat(Settings->RadiusParameter,Radius);
   if(!Settings->DurationParameter.IsNone())Niagara->SetVariableFloat(Settings->DurationParameter,Lifetime);
   if(!Settings->StartParameter.IsNone())Niagara->SetVariableVec3(Settings->StartParameter,Start+Settings->LocationOffset);
   if(!Settings->EndParameter.IsNone())Niagara->SetVariableVec3(Settings->EndParameter,End+Settings->LocationOffset);
   if(!Settings->ColorParameter.IsNone())Niagara->SetVariableLinearColor(Settings->ColorParameter,Color);
   NiagaraStartParameter=Settings->StartParameter;NiagaraEndParameter=Settings->EndParameter;
   NiagaraLocalEnd=Niagara->GetComponentQuat().UnrotateVector(End-Start);
   Niagara->Activate(true);
  }
  if(Settings->Sound)UGameplayStatics::PlaySoundAtLocation(this,Settings->Sound,Start,FRotator::ZeroRotator,Settings->SoundVolume,FMath::Max(0.01f,Settings->SoundPitch),0,nullptr,Settings->SoundConcurrency);
 }
 Material=UMaterialInstanceDynamic::Create(bBeam?BeamMaterial:RingMaterial,this);
 if(Material){Material->SetVectorParameterValue(Settings?Settings->FallbackColorParameter:FName(TEXT("Tint")),Color);Visual->SetMaterial(0,Material);}
 SetLifeSpan(Lifetime+0.1f);
}
void AAbilityAccent::Tick(float Delta)
{
 Super::Tick(Delta);Age+=Delta;
 if(Niagara&&Niagara->GetAsset())
 {
  const FVector Start=Niagara->GetComponentLocation();
  if(!NiagaraStartParameter.IsNone())Niagara->SetVariableVec3(NiagaraStartParameter,Start);
  if(!NiagaraEndParameter.IsNone())Niagara->SetVariableVec3(NiagaraEndParameter,Start+Niagara->GetComponentQuat().RotateVector(NiagaraLocalEnd));
 }
 const float Alpha=FMath::Clamp(Age/Lifetime,0.0f,1.0f);
 if(Material) Material->SetScalarParameterValue(FallbackIntensityParameter,bFadeFallback?1-Alpha:1);
 // Keep rings aligned to their damage footprint; brightness provides the pulse.
 if(bIsBeam) SetActorScale3D(InitialScale*FVector(1,FMath::Max(0.1f,1-Alpha),FMath::Max(0.1f,1-Alpha)));
 if(Age>=Lifetime) Destroy();
}
USurvivorAbilityComponent::USurvivorAbilityComponent(){PrimaryComponentTick.bCanEverTick=false;}
void USurvivorAbilityComponent::BeginPlay()
{
 Super::BeginPlay();Controller=Cast<ASurvivorPlayerController>(GetOwner());
 Upgrades=Controller?Controller->GetPlayerUpgrades():nullptr;
 GetWorld()->GetTimerManager().SetTimer(Scheduler,this,&USurvivorAbilityComponent::UpdateAbilities,0.1f,true);
}
void USurvivorAbilityComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 GetWorld()->GetTimerManager().ClearTimer(Scheduler);Pending.Reset();ClearBuildFamilies();
 for(auto Effect:ActiveAccents) if(Effect.IsValid()) Effect->Destroy();
 Super::EndPlay(Reason);
}
float USurvivorAbilityComponent::Magnitude(int32 Index,const TCHAR* Suffix) const
{
 return Upgrades?FMath::Max(0.0f,Upgrades->GetAccumulatedUpgradeMagnitude(FName(FString(Families[Index])+Suffix))):0;
}
float USurvivorAbilityComponent::Cooldown(int32 Index) const{return FamilySpec(Index).Cooldown/(1+Magnitude(Index,TEXT("Haste")));}
float USurvivorAbilityComponent::Power(ACharacterBase* Character) const
{
 const float CharacterPower=Character->GetCharacterStats()?Character->GetCharacterStats()->GetFinalDamageMultiplier():1;
 const float Shared=Controller->GetSharedPlayerStats()?Controller->GetSharedPlayerStats()->GetFinalDamageMultiplier():1;
 return CharacterPower*Shared*(Character->IsA<ASamuraiCharacter>()?Upgrades->GetSamuraiPowerMultiplier():Upgrades->GetNinjaPowerMultiplier());
}
TArray<AEnemyBase*> USurvivorAbilityComponent::FindEnemies(FVector Position,float Radius,EPlayerAttackSource Source) const
{
 TArray<FOverlapResult> Hits; TArray<AEnemyBase*> Enemies; TSet<AEnemyBase*> Seen;
 FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
 GetWorld()->OverlapMultiByObjectType(Hits,Position,FQuat::Identity,Objects,FCollisionShape::MakeSphere(Radius),FCollisionQueryParams(SCENE_QUERY_STAT(SurvivorAbility),false,GetOwner()));
 for(const auto& Hit:Hits)
 {
  auto* Enemy=Cast<AEnemyBase>(Hit.GetActor());
  if(IsValid(Enemy)&&!Enemy->IsDead()&&Enemy->CanReceivePlayerDamage(Source)&&!Seen.Contains(Enemy))
  {Seen.Add(Enemy);Enemies.Add(Enemy);}
 }
 Enemies.Sort([Position](const AEnemyBase& A,const AEnemyBase& B){return FVector::DistSquared(A.GetActorLocation(),Position)<FVector::DistSquared(B.GetActorLocation(),Position);});
 if(Enemies.Num()>128) Enemies.SetNum(128);
 return Enemies;
}
AAbilityAccent* USurvivorAbilityComponent::Accent(FVector Position,FVector End,float Radius,FLinearColor Color,float Duration,bool bBeam,const FUpgradePresentation* Settings,int32 Stage)
{
 if(GetNetMode()==NM_DedicatedServer) return nullptr;
 if(Settings&&Settings->MinimumSpawnInterval>0)
 {
  const FName Key(FString::Printf(TEXT("VFX:%p:%d:%d"),Settings,Stage,bBeam));
  const double Now=GetWorld()->GetTimeSeconds();
  if(const auto* Last=LastVisualSpawn.Find(Key);Last&&Now-*Last<Settings->MinimumSpawnInterval)return nullptr;
  LastVisualSpawn.Add(Key,Now);
 }
 ActiveAccents.RemoveAll([](const auto& E){return !E.IsValid();});
 if(ActiveAccents.Num()>=64) return nullptr;
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Effect=GetWorld()->SpawnActor<AAbilityAccent>(Position,FRotator::ZeroRotator,Params);
 if(Effect){Effect->Initialize(End,Radius,Color,Duration,bBeam,Settings,Stage);ActiveAccents.Add(Effect);}
 return Effect;
}
void USurvivorAbilityComponent::Pulse(const FPendingPulse& P)
{
 // Ground rings show the actual attack radius; damage is resolved at pulse time.
 const int32 VF=P.VisualFamily>=0?P.VisualFamily:P.Family;
 FamilyAccent(VF,P.Position-FVector(0,0,70),P.Position,P.Radius,P.Color,0.35f,false,P.VisualSlot,P.VisualStage);
 if(P.bSkyStrike) FamilyAccent(VF,P.Position+FVector(0,0,550),P.Position,0,P.Color,0.22f,true,P.VisualSlot);
 for(AEnemyBase* Enemy:FindEnemies(P.Position,P.Radius,P.Source))
 {
  if(!Controller->IsRunInProgress()||Controller->IsPlayerDead()) break;
  const bool bApplied=Enemy->ApplyPlayerDamage(P.Damage,P.Source);
  if(bApplied)
  {
   FamilyAccent(VF,Enemy->GetActorLocation(),Enemy->GetActorLocation(),P.Radius,P.Color,0.25f,false,P.VisualSlot,2);
   if(!P.bReaction) NotifyPartnerHit(P.Source,Enemy);
   if(!P.bReaction && P.Family>=0 && ((FCString::Strcmp(BuildFamilies[P.Family].Owner,TEXT("Samurai"))==0)==(P.Source==EPlayerAttackSource::Samurai))) RegisterFamilyHit(P.Family,Enemy,P.Damage);
  }
  if(bApplied&&!Enemy->IsDead())
  {
   if(P.bPoison && (!P.bGarden || !Enemy->HasStatus(EEnemyStatusEffect::Poison))) ApplyConfiguredStatus(VF,Enemy,EEnemyStatusEffect::Poison,P.Source);
   if(P.bPush) Enemy->ApplyAttackPushback(P.Position,P.Source,Tuning(3,TEXT("PushDistance"),45.0f,2),Tuning(3,TEXT("PushDuration"),0.15f,2));
   if(P.bBleed)
   {
    ApplyConfiguredStatus(VF,Enemy,EEnemyStatusEffect::Bleed,P.Source);
    if(Upgrades->HasUpgradeId(TEXT("MarkedBlade"))) Enemy->ApplyMark();
   }
  }
 }
}
void USurvivorAbilityComponent::UpdateAbilities()
{
 if(!Controller||!Upgrades)return;
 if(!Controller->IsRunInProgress()||Controller->IsPlayerDead())
 {
  Pending.Reset();ClearBuildFamilies();for(auto E:ActiveAccents)if(E.IsValid())E->Destroy();ActiveAccents.Reset();return;
 }
 UpdateBuildFamilies(nullptr);
}
bool USurvivorAbilityComponent::ActivateAbility(int32 Index,ACharacterBase* Character)
{
 // Retired automatic abilities cannot execute, including from old debug callers.
 return false;
}

FVector USurvivorAbilityComponent::FindCrowdCenter(const TArray<AEnemyBase*>& Targets,float Radius) const
{
 // Bounded in-memory scoring; no overlap query per candidate. Nearest wins ties.
 FVector Best=Targets[0]->GetActorLocation();int32 BestCount=0;
 for(const auto* Candidate:Targets)
 {
  int32 Count=0;
  for(const auto* Other:Targets)
   if(FVector::DistSquared(Candidate->GetActorLocation(),Other->GetActorLocation())<=FMath::Square(Radius)) ++Count;
  if(Count>BestCount){BestCount=Count;Best=Candidate->GetActorLocation();}
 }
 return Best;
}

void USurvivorAbilityComponent::HandleSamuraiMeleeHit(FVector HitPosition)
{
 // The retired Venom Garden detonation no longer exists.
}

bool USurvivorAbilityComponent::ExecuteSetupAssist(ACharacterBase* Character)
{
 if(!Controller||!Upgrades||!IsValid(Character)||!Controller->IsRunInProgress()||Controller->IsPlayerDead()||
    Character->GetCharacterMode()!=ECharacterMode::Assisting) return false;
 auto* Definition=Upgrades->FindUpgradeDefinition(TEXT("TagTeam"));
 const auto AssistTune=[Definition](FName Key,float Default){return Definition?Definition->GetBalanceValue(Key,Default):Default;};
 const bool bSamurai=Character->IsA<ASamuraiCharacter>();
 if(!bSamurai&&!Character->IsA<ANinjaCharacter>()) return false;
 const auto Source=bSamurai?EPlayerAttackSource::Samurai:EPlayerAttackSource::Ninja;
 const FVector Origin=Character->GetActorLocation();
 const FVector Forward=Character->GetVisualForwardVector().GetSafeNormal2D();
 int32 Hits=0;
 auto Targets=FindEnemies(Origin,bSamurai?AssistTune(TEXT("SamuraiRange"),420):AssistTune(TEXT("NinjaRange"),1000),Source);
 PrioritizePreparedTargets(Source,Targets);
 for(auto* Enemy:Targets)
 {
  if(!Controller->IsRunInProgress()||Controller->IsPlayerDead()) break;
  const FVector Position=Enemy->GetActorLocation();
  const FVector Direction=(Position-Origin).GetSafeNormal2D();
  if(!Direction.IsNearlyZero()&&FVector::DotProduct(Forward,Direction)<FMath::Cos(FMath::DegreesToRadians(AssistTune(TEXT("ConeHalfAngle"),69.51268f)))) continue;
  // Death clears statuses, so snapshot them before the successful assist hit.
  const uint8 StatusBeforeHit=(Enemy->HasStatus(EEnemyStatusEffect::Bleed)?1:0)|(Enemy->HasStatus(EEnemyStatusEffect::Poison)?2:0);
  if(!Enemy->ApplyPlayerDamage((bSamurai?AssistTune(TEXT("SamuraiDamage"),12):AssistTune(TEXT("NinjaDamage"),10))*Power(Character),Source)) continue;
  NotifyPartnerHit(Source,Enemy,true,StatusBeforeHit);
  Accent(Origin,Position,0,bSamurai?Colors[0]:Colors[3],0.3f,true,Definition?&Definition->Presentation:nullptr);
  if(!Enemy->IsDead())
  {
   Enemy->GetStatusEffectComponent()->ApplyStatus(bSamurai?EEnemyStatusEffect::Bleed:EEnemyStatusEffect::Poison,Upgrades,Source,true);
   if(bSamurai)
   {
    if(Upgrades->HasUpgradeId(TEXT("MarkedBlade"))) Enemy->ApplyMark();
    Enemy->ApplyAttackPushback(Origin,Source,AssistTune(TEXT("PushDistance"),65),AssistTune(TEXT("PushDuration"),0.15f));
   }
  }
  // Preserve the existing per-character assist hit limit.
  if(++Hits>=FMath::Clamp(FMath::RoundToInt(bSamurai?AssistTune(TEXT("SamuraiTargets"),12):AssistTune(TEXT("NinjaTargets"),5)),1,128)) break;
 }
 return true;
}
