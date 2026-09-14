#include "NinjaBuildComponent.h"
#include "NinjaCharacter.h"
#include "ShadowClone.h"
#include "AttackProjectileBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "AutoAttackComponent.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "EnemyBase.h"
#include "EnemyStatusEffectComponent.h"
#include "HealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "UObject/ConstructorHelpers.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"

ANinjaBuildProjectile::ANinjaBuildProjectile()
{
 PrimaryActorTick.bCanEverTick=true;
 Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Blade"));SetRootComponent(Visual);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
 Visual->SetStaticMesh(Mesh.Object);Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);Visual->SetCastShadow(false);
 Visual->SetRelativeScale3D(FVector(.6f,.12f,.035f));
 auto* Cross=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrossBlade"));Cross->SetupAttachment(Visual);
 Cross->SetStaticMesh(Mesh.Object);Cross->SetCollisionEnabled(ECollisionEnabled::NoCollision);Cross->SetCastShadow(false);
 Cross->SetRelativeScale3D(FVector(.2f,5.f,1.f));
}
UNinjaBuildComponent::UNinjaBuildComponent()
{
 PrimaryComponentTick.bCanEverTick=true;
 static ConstructorHelpers::FObjectFinder<USoundBase> ThrowSound(TEXT("/Game/Assets/Sounds/Ninja/KunaiThrow.KunaiThrow"));
 KunaiThrowSound=ThrowSound.Object;
}
ANinjaCharacter* UNinjaBuildComponent::Ninja() const{return Cast<ANinjaCharacter>(GetOwner());}
UAutoAttackComponent* UNinjaBuildComponent::Attack() const{return GetOwner()?GetOwner()->FindComponentByClass<UAutoAttackComponent>():nullptr;}
UPlayerUpgradeComponent* UNinjaBuildComponent::Upgrades() const
{auto* PC=GetOwner()?Cast<ASurvivorPlayerController>(GetOwner()->GetOwner()):nullptr;return PC?PC->GetPlayerUpgrades():nullptr;}
bool UNinjaBuildComponent::Has(FName Id) const{return Upgrades()&&Upgrades()->HasUpgradeId(Id);}
float UNinjaBuildComponent::Tune(FName Id,FName Key,float Default) const
{auto* Card=Upgrades()?Upgrades()->FindUpgradeDefinition(Id):nullptr;return Card?Card->GetBalanceValue(Key,Default):Default;}
bool UNinjaBuildComponent::IsRunning() const
{auto* PC=GetOwner()?Cast<ASurvivorPlayerController>(GetOwner()->GetOwner()):nullptr;return PC&&PC->IsRunInProgress()&&!PC->IsPlayerDead();}
bool UNinjaBuildComponent::IsActive() const
{return IsRunning()&&Ninja()&&Ninja()->GetCharacterMode()==ECharacterMode::Active;}
TArray<AEnemyBase*> UNinjaBuildComponent::Sweep(FVector Start,FVector End,float Radius) const
{
 TArray<AEnemyBase*> Out;if(!GetWorld())return Out;
 TArray<FHitResult> Hits;FCollisionObjectQueryParams Types;Types.AddObjectTypesToQuery(ECC_Pawn);Types.AddObjectTypesToQuery(ECC_GameTraceChannel1);
 GetWorld()->SweepMultiByObjectType(Hits,Start,End,FQuat::Identity,Types,FCollisionShape::MakeSphere(FMath::Max(1.f,Radius)),FCollisionQueryParams(SCENE_QUERY_STAT(NinjaBuild),false,GetOwner()));
 for(auto& Hit:Hits)if(auto* E=Cast<AEnemyBase>(Hit.GetActor());E&&!E->IsDead()&&E->CanReceivePlayerDamage(EPlayerAttackSource::Ninja)&&E->GetHealthComponent()&&!E->GetHealthComponent()->IsDead())Out.AddUnique(E);
 Out.Sort([Start](const AEnemyBase& A,const AEnemyBase& B){return FVector::DistSquared(Start,A.GetActorLocation())<FVector::DistSquared(Start,B.GetActorLocation());});
 if(Out.Num()>128)Out.SetNum(128);return Out;
}
TArray<AEnemyBase*> UNinjaBuildComponent::Targets(FVector Position,float Radius) const{return Sweep(Position,Position,Radius);}
AEnemyBase* UNinjaBuildComponent::Nearest(FVector Position,float Radius,AEnemyBase* Ignore) const
{for(auto* E:Targets(Position,Radius))if(E!=Ignore)return E;return nullptr;}
ANinjaBuildProjectile* UNinjaBuildComponent::SpawnBlade(int32 Kind,FVector Position)
{
 Projectiles.RemoveAll([](auto& P){return !P.IsValid();});
 if(Projectiles.Num()>=128)return nullptr;
 if(Kind==1&&Projectiles.FilterByPredicate([](auto& P){return P.IsValid()&&P->Kind==1;}).Num()>=8)return nullptr;
 FActorSpawnParameters P;P.Owner=GetOwner();P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Blade=GetWorld()->SpawnActor<ANinjaBuildProjectile>(Position,FRotator::ZeroRotator,P);
 if(Blade){Blade->Build=this;Blade->Kind=Kind;Projectiles.Add(Blade);if(Kind!=1)Blade->SetupKunaiPresentation();}return Blade;
}
void UNinjaBuildComponent::ClearProjectiles()
{
 for(auto P:Projectiles)if(P.IsValid())P->Destroy();Projectiles.Reset();Fang.Reset();ConsecutiveVolleys=VolleyCount=0;
 for(auto& Pair:Embedded){if(Pair.Key.IsValid())Pair.Key->OnEnemyDied.RemoveDynamic(this,&UNinjaBuildComponent::ScatterEmbedded);for(auto V:Pair.Value.Visuals)if(V.IsValid())V->DestroyComponent();}
 Embedded.Reset();
}
void UNinjaBuildComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 ClearProjectiles();
 for(auto& Pair:Embedded){if(Pair.Key.IsValid())Pair.Key->OnEnemyDied.RemoveDynamic(this,&UNinjaBuildComponent::ScatterEmbedded);for(auto V:Pair.Value.Visuals)if(V.IsValid())V->DestroyComponent();}
 Embedded.Reset();Super::EndPlay(Reason);
}
void UNinjaBuildComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Tick)
{
 Super::TickComponent(Delta,Type,Tick);
 if(!IsRunning()){ClearProjectiles();return;}
 const bool Active=IsActive();
 if(bWasActive&&!Active)VolleyCount=0;
 bWasActive=Active;
 for(auto It=Embedded.CreateIterator();It;++It)if(!It.Key().IsValid())It.RemoveCurrent();
 if(!Has(TEXT("ReturningFang"))){if(Fang.IsValid())Fang->Destroy();Fang.Reset();return;}
 TargetCheck-=Delta;
 if(Active&&!Ninja()->IsDashing()&&!Fang.IsValid()&&TargetCheck<=0&&Attack()&&Attack()->IsAutoAttackEnabled())
 {
  TargetCheck=.1f;
  if(Nearest(Ninja()->GetActorLocation(),Attack()->GetEffectiveTargetingRange()))
   if(auto* P=SpawnBlade(0,Ninja()->GetActorLocation()+FVector(0,0,50))){Fang=P;P->LaunchFang();}
 }
}
void ANinjaBuildProjectile::LaunchFang()
{
 auto* B=Build.Get();if(!B||!B->IsRunning()||!B->Attack()||(bCloneProjectile?!Clone.IsValid():!B->IsActive())){Destroy();return;}
 Target=B->Nearest(GetActorLocation(),B->Attack()->GetEffectiveTargetingRange());
 if(!Target.IsValid()){Destroy();return;}
 auto* A=B->Attack();int32 Extra=FMath::Max(0,A->GetEffectiveProjectileCount()-1);
 if(const auto* Grand=bCloneProjectile?nullptr:A->GetReadyGrandEntranceUpgrade()){Extra+=FMath::Max(0,FMath::RoundToInt(Grand->GetBalanceValue(TEXT("NinjaBonusProjectiles"),8)));A->bGrandEntranceReady=false;}
 Damage=A->GetEffectiveAttackDamage()*(1+Extra*B->Tune(TEXT("ReturningFang"),TEXT("CountDamage"),.25f));
 Speed=A->GetEffectiveProjectileSpeed()*A->GetBaseAttackInterval()/A->GetEffectiveAttackInterval();
 if(bBoostNext&&B->Has(TEXT("Bloodhound")))Speed*=B->Tune(TEXT("Bloodhound"),TEXT("SpeedMultiplier"),1.5f);
 if(bCloneProjectile)Speed*=Clone->GetFangSpeedMultiplier();
 if(B->KunaiThrowSound&&(!bCloneProjectile||FlightAge>0))UGameplayStatics::PlaySoundAtLocation(this,B->KunaiThrowSound,GetActorLocation());
 bBoostNext=false;bReturning=false;bPursued=false;FlightAge=0;LastHits.Reset();
 if(!bCloneProjectile)A->OnAutoAttack.Broadcast(A,EAutoAttackSource::NormalAutoAttack);
}
bool UNinjaBuildComponent::ReplaceVolley(FVector Direction)
{
 if(Has(TEXT("ReturningFang")))return true;
 if(!Has(TEXT("GreatShuriken")))return false;
 auto* A=Attack();if(!A)return true;
 if(Direction.IsNearlyZero()){if(auto* E=Nearest(Ninja()->GetActorLocation(),A->GetEffectiveTargetingRange()))Direction=E->GetActorLocation()-Ninja()->GetActorLocation();else Direction=Ninja()->GetVisualForwardVector();}
 Direction.Z=0;Direction.Normalize();
 SpawnShuriken(Ninja()->GetActorLocation()+Direction*65+FVector(0,0,50),Direction,true);
 return true;
}
ANinjaBuildProjectile* UNinjaBuildComponent::SpawnShuriken(FVector Position,FVector Direction,bool bUseGrandEntrance)
{
 auto* A=Attack();if(!A)return nullptr;
 auto* P=SpawnBlade(1,Position);if(!P)return nullptr;
 int32 Extra=FMath::Max(0,A->GetEffectiveProjectileCount()-1);
 if(const auto* G=bUseGrandEntrance?A->GetReadyGrandEntranceUpgrade():nullptr){Extra+=FMath::RoundToInt(G->GetBalanceValue(TEXT("NinjaBonusProjectiles"),8));A->bGrandEntranceReady=false;}
 P->Damage=A->GetEffectiveAttackDamage();P->Speed=FMath::Max(1.f,Tune(TEXT("GreatShuriken"),TEXT("TravelSpeed"),900.f));P->Direction=Direction.GetSafeNormal();
 P->Radius=FMath::Clamp(Tune(TEXT("GreatShuriken"),TEXT("Radius"),95)*(1+Extra*Tune(TEXT("GreatShuriken"),TEXT("CountSize"),.12f)),25.f,300.f);
 P->Visual->SetWorldScale3D(FVector(P->Radius/50.f,P->Radius/250.f,.08f));return P;
}
ANinjaBuildProjectile* UNinjaBuildComponent::SpawnCloneFang(AShadowClone* InClone,FVector Position)
{
 if(!InClone||!Attack()||!Nearest(Position,Attack()->GetEffectiveTargetingRange()))return nullptr;
 auto* P=SpawnBlade(0,Position);if(P){P->Clone=InClone;P->bCloneProjectile=true;P->LaunchFang();}return P;
}
void UNinjaBuildComponent::ModifyVolley(FVector& Direction,int32& Count,float& Spacing)
{ModifyVolleyWithCounters(Direction,Count,Spacing,VolleyCount,ConsecutiveVolleys);}
void UNinjaBuildComponent::ModifyVolleyWithCounters(FVector& Direction,int32& Count,float& Spacing,int32& Volley,int32& Consecutive)
{
 if(!Has(TEXT("BarrageStance")))return;
 ++Volley;
 if(Has(TEXT("Crescendo")))
 {
  const int32 Rank=Upgrades()->GetUpgradeLevelById(TEXT("Crescendo"));
  const int32 Cap=FMath::Clamp(FMath::RoundToInt(Tune(TEXT("Crescendo"),TEXT("BaseCap"),5)+FMath::Max(0,Rank-1)*Tune(TEXT("Crescendo"),TEXT("CapPerRank"),2)),1,64);
  const int32 Interval=FMath::Clamp(FMath::RoundToInt(Tune(TEXT("Crescendo"),TEXT("AttacksPerProjectile"),3)),1,100);
  Consecutive=FMath::Min(Consecutive+1,Cap*Interval);
  const int32 Bonus=FMath::Min(Consecutive/Interval,Cap);Count+=Bonus;
  if(Bonus>=Cap)Consecutive=0; // Fire the peak volley, then begin a new buildup.
 }
 if(Has(TEXT("NeedleRain"))&&Volley%4==0)Count*=2;
 if(Has(TEXT("FocusedVolley")))Spacing*=.35f;
 if(Has(TEXT("AlternatingFans")))Direction=Direction.RotateAngleAxis(Volley%2?12.f:-12.f,FVector::UpVector);
 Count=FMath::Clamp(Count,1,128);
}
float UNinjaBuildComponent::Hit(AEnemyBase* Enemy,float Damage,bool bEmbed)
{
 if(!Enemy||!Upgrades()||Enemy->IsDead()||!Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Ninja))return 0;
 if(Enemy->IsMarked()&&Enemy->ConsumeMark())Damage*=2;
 if(!ApplyEmbeddedHit(Enemy,Damage,Upgrades(),bEmbed))return 0;
 if(Attack()&&Attack()->ProjectileClass)UImpactFeedbackLibrary::PlayImpactFeedback(this,Attack()->ProjectileClass->GetDefaultObject<AAttackProjectileBase>()->ImpactFeedback,Enemy->GetActorLocation()+FVector(0,0,45),FVector::UpVector,false);
 if(!Enemy->IsDead()&&Has(TEXT("VenomousKunai")))Enemy->ApplyStatus(EEnemyStatusEffect::Poison,Upgrades(),EPlayerAttackSource::Ninja);
 return Damage;
}
bool UNinjaBuildComponent::ApplyEmbeddedHit(AEnemyBase* Enemy,float Damage,UPlayerUpgradeComponent* U,bool bEmbed)
{
 if(!Enemy||!Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Ninja)||!Enemy->GetHealthComponent()||!Enemy->GetHealthComponent()->IsDamageEnabled()||!FMath::IsFinite(Damage)||Damage<=0)return false;
 if(bEmbed&&U&&U->HasUpgradeId(TEXT("EmbeddedBlades")))
  if(auto* PC=Cast<ASurvivorPlayerController>(U->GetOwner());PC&&PC->GetCharacterManager()&&PC->GetCharacterManager()->GetNinja())
   if(auto* B=PC->GetCharacterManager()->GetNinja()->FindComponentByClass<UNinjaBuildComponent>())B->StageEmbedded(Enemy,Damage);
 return Enemy->ApplyPlayerDamage(Damage,EPlayerAttackSource::Ninja);
}
void UNinjaBuildComponent::StageEmbedded(AEnemyBase* Enemy,float Damage)
{
 if(!Enemy||!Upgrades()||(Embedded.Num()>=512&&!Embedded.Contains(Enemy)))return;
 auto& E=Embedded.FindOrAdd(Enemy);
 const int32 Added=FMath::Min(1+Upgrades()->GetUpgradeLevelById(TEXT("FragmentLoad")),12-E.Count);if(Added<=0)return;
 E.Count+=Added;E.Damage+=Damage*Added*Tune(TEXT("EmbeddedBlades"),TEXT("DamageFraction"),.4f)*(1+Upgrades()->GetAccumulatedUpgradeMagnitude(TEXT("FragmentDamage")));
 Enemy->OnEnemyDied.AddUniqueDynamic(this,&UNinjaBuildComponent::ScatterEmbedded);
 if(E.Visuals.Num()<4)
 {
  auto* V=NewObject<UStaticMeshComponent>(Enemy);V->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  V->SetCollisionEnabled(ECollisionEnabled::NoCollision);V->SetCastShadow(false);V->SetupAttachment(Enemy->GetRootComponent());V->RegisterComponent();
  V->SetRelativeLocation(FVector(0,20*(E.Visuals.Num()-1.f),35));V->SetRelativeRotation(FRotator(0,30+E.Visuals.Num()*40,25));V->SetRelativeScale3D(FVector(.55f,.05f,.04f));E.Visuals.Add(V);
 }
}
void UNinjaBuildComponent::ScatterEmbedded(AEnemyBase* Enemy)
{
 auto* Found=Embedded.Find(Enemy);if(!Found)return;auto E=*Found;Embedded.Remove(Enemy);
 Enemy->OnEnemyDied.RemoveDynamic(this,&UNinjaBuildComponent::ScatterEmbedded);
 for(auto V:E.Visuals)if(V.IsValid())V->DestroyComponent();
 if(IsRunning())Scatter(Enemy->GetActorLocation()+FVector(0,0,45),E.Count,E.Damage/FMath::Max(1,E.Count),Tune(TEXT("EmbeddedBlades"),TEXT("Range"),700)*(1+Upgrades()->GetAccumulatedUpgradeMagnitude(TEXT("FragmentReach"))));
}
void UNinjaBuildComponent::Scatter(FVector Position,int32 Count,float Damage,float Range)
{
 auto Enemies=Targets(Position,Range);if(Enemies.IsEmpty())return;
 for(int32 i=0;i<FMath::Min(Count,24);++i)if(auto* P=SpawnBlade(2,Position))
 {P->Damage=Damage;P->Speed=1400;P->Direction=(Enemies[i%Enemies.Num()]->GetActorLocation()+FVector(0,0,45)-Position).GetSafeNormal();}
}
void ANinjaBuildProjectile::Finish()
{
 auto* B=Build.Get();
 if(Kind==1&&B&&B->IsRunning()&&B->Has(TEXT("BreakingWheel")))B->Scatter(GetActorLocation(),6,Damage*.3f,700);
 Destroy();
}
void ANinjaBuildProjectile::Tick(float Delta)
{
 Super::Tick(Delta);auto* B=Build.Get();if(!B||!B->Ninja()||!B->IsRunning()){Destroy();return;}
 Delta=FMath::Min(Delta,.1f);Age+=Delta;FlightAge+=Delta;SlowRemaining=FMath::Max(0.f,SlowRemaining-Delta);
 const FVector Start=GetActorLocation();FVector End=Start;
 if(Kind==0)
 {
  if(!B->Has(TEXT("ReturningFang"))){Destroy();return;}
  if(bCloneProjectile&&!Clone.IsValid()){Destroy();return;}
  if((!bCloneProjectile&&!B->IsActive())||FlightAge>4)bReturning=true;
  if(!bReturning&&(!Target.IsValid()||Target->IsDead())){Target=B->Nearest(Start,B->Attack()->GetEffectiveTargetingRange());if(!Target.IsValid())bReturning=true;}
  const FVector ReturnOrigin=bCloneProjectile?Clone->GetActorLocation()+FVector(0,0,60):B->Ninja()->GetActorLocation()+FVector(0,0,50);
  const FVector Goal=bReturning?ReturnOrigin:Target->GetActorLocation()+FVector(0,0,45);
  const float Distance=FVector::Distance(Start,Goal);End=FMath::VInterpConstantTo(Start,Goal,Delta,FMath::Max(100.f,Speed));
  if(bReturning&&Distance<=FMath::Max(25.f,Speed*Delta)){SetActorLocation(Goal);if(bCloneProjectile){if(Clone->CompleteFangCycle())LaunchFang();else Destroy();}
   else if(B->IsActive()&&!B->Ninja()->IsDashing())LaunchFang();else Destroy();return;}
  for(auto* E:B->Sweep(Start,End,Radius))
  {
   if(bReturning){if(!B->Has(TEXT("CuttingReturn"))||LastHits.Contains(E))continue;LastHits.Add(E,Age);B->Hit(E,Damage);continue;}
   const bool Status=E->HasStatus(EEnemyStatusEffect::Bleed)||E->HasStatus(EEnemyStatusEffect::Poison);
   Streak=LastVictim==E?FMath::Min(Streak+1,5):0;LastVictim=E;
   B->Hit(E,Damage*(B->Has(TEXT("RelentlessFang"))?1+Streak*.15f:1));bBoostNext=Status;LastHits.Reset();
   if(E->IsDead()&&B->Has(TEXT("FinalPursuit"))&&!bPursued){Target=B->Nearest(E->GetActorLocation(),350,E);bPursued=true;if(Target.IsValid())break;}
   bReturning=true;break;
  }
 }
 else
 {
  if(Kind==1&&Age>=B->Tune(TEXT("GreatShuriken"),TEXT("Lifetime"),2.5f))
  {
   Finish();return;
  }
  End=Start+Direction*Speed*Delta*(SlowRemaining>0?.15f:1.f);
  if(Kind==1&&B->Has(TEXT("WideOrbit")))
  {
   if(InitialRadius<=0)InitialRadius=Radius;
   TravelDistance+=FVector::Distance(Start,End);
   const float Growth=FMath::Clamp(TravelDistance/FMath::Max(1.f,B->Tune(TEXT("WideOrbit"),TEXT("GrowthDistance"),1000.f)),0.f,1.f);
   Radius=InitialRadius*FMath::Lerp(1.f,FMath::Clamp(B->Tune(TEXT("WideOrbit"),TEXT("MaxSizeMultiplier"),2.f),1.f,4.f),Growth);
   Visual->SetWorldScale3D(FVector(Radius/50.f,Radius/250.f,.08f));
  }
  if(Kind==2&&Age>2){Destroy();return;}
  for(auto* E:B->Sweep(Start,End,Radius))
  {
   if(auto* Last=LastHits.Find(E);Last&&Age-*Last<B->Tune(TEXT("GreatShuriken"),TEXT("HitInterval"),.25f))continue;
   LastHits.Add(E,Age);int32& Count=HitCounts.FindOrAdd(E);
   B->Hit(E,Damage*(Kind==1&&B->Has(TEXT("SerratedEdge"))?1+FMath::Min(Count,5)*.15f:1),Kind!=2);
   if(Kind==2){Destroy();return;}
   if(Count==0&&B->Has(TEXT("GrindingHalt"))&&E->GetDropCategory()!=EEnemyDropCategory::Normal)SlowRemaining=.6f;
   ++Count;
  }
 }
 SetActorLocation(End);if(Kind==1)AddActorLocalRotation(FRotator(0,Delta*900,0));else if(!End.Equals(Start))SetActorRotation((End-Start).Rotation());
}

void ANinjaBuildProjectile::SetupKunaiPresentation()
{
 auto* B=Build.Get();if(!B||!B->Attack()||!B->Attack()->ProjectileClass)return;
 FActorSpawnParameters Params;Params.Owner=this;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* P=GetWorld()->SpawnActor<AAttackProjectileBase>(B->Attack()->ProjectileClass,GetActorLocation(),GetActorRotation(),Params);
 if(!P)return;
 P->SetActorEnableCollision(false);P->SetLifeSpan(0);
 P->ProjectileMovement->StopMovementImmediately();P->ProjectileMovement->Deactivate();P->ProjectileMovement->SetComponentTickEnabled(false);
 Visual->SetVisibility(false,true);
 P->AttachToActor(this,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
 KunaiPresentation=P;
}
void ANinjaBuildProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
 if(auto* P=KunaiPresentation.Get())
 {
  P->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
  if(Reason==EEndPlayReason::Destroyed)P->BeginImpactTrailFade();else P->Destroy();
 }
 Super::EndPlay(Reason);
}
