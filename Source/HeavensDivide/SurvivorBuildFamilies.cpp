#include "SurvivorAbilityComponent.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "EnemyStatusEffectComponent.h"
#include "HealthComponent.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"

namespace
{
 EPlayerAttackSource FamilySource(int32 Family)
 { return FCString::Strcmp(BuildFamilies[Family].Owner,TEXT("Samurai"))==0?EPlayerAttackSource::Samurai:EPlayerAttackSource::Ninja; }
 FLinearColor FamilyColor(int32 Family)
 { return FLinearColor::MakeFromHSV8(static_cast<uint8>(FamilySource(Family)==EPlayerAttackSource::Samurai?18+Family*5:125+Family*6),190,255)*2.0f; }
}
bool USurvivorAbilityComponent::Branch(int32 Family,int32 Index) const
{ return Upgrades&&Upgrades->HasUpgradeId(BuildFamilies[Family].Branches[Index]); }
float USurvivorAbilityComponent::BuildMagnitude(int32 Family,const TCHAR* Suffix) const
{
 if(!Upgrades) return 0;
 const FName Id=Family==4&&FCString::Strcmp(Suffix,TEXT("Area"))==0?FName(TEXT("WideArc")):FName(FString(BuildFamilies[Family].Id)+Suffix);
 return FMath::Max(0.0f,Upgrades->GetAccumulatedUpgradeMagnitude(Id));
}
void USurvivorAbilityComponent::ClearBuildFamilies()
{ BuildCasts.Reset();BuildMarks.Reset();bTrailInitialized=false; }
void USurvivorAbilityComponent::UpdateBuildFamilies(ACharacterBase* Character)
{
 for(float& Gate:ReactionGates) Gate=FMath::Max(0.0f,Gate-0.1f);
 for(auto& Mark:BuildMarks) Mark.Remaining-=0.1f;
 BuildMarks.RemoveAll([](const auto& M){return M.Remaining<=0||!M.Enemy.IsValid()||M.Enemy->IsDead();});
 // Remove a cast before invoking damage: callbacks can safely append follow-up work.
 const int32 InitialCount=BuildCasts.Num();
 for(int32 i=InitialCount-1;i>=0;--i)
 {
  FBuildCast Cast=MoveTemp(BuildCasts[i]);BuildCasts.RemoveAtSwap(i);
  Cast.Remaining-=0.1f;Cast.Life-=0.1f;
  if(Cast.Life<=0) continue;
  bool bKeep=true;
  if(Cast.Remaining<=0.001f){bKeep=StepBuildCast(Cast);Cast.Remaining+=FMath::Max(0.1f,Cast.Interval);}
  if(!Controller->IsRunInProgress()||Controller->IsPlayerDead()){ClearBuildFamilies();return;}
  if(bKeep && BuildCasts.Num()<96) BuildCasts.Add(MoveTemp(Cast));
 }
 if(!IsValid(Character)||Character->GetCharacterMode()!=ECharacterMode::Active) return;
 const auto Source=AEnemyBase::ResolvePlayerAttackSource(Character);
 for(int32 i=5;i<20;++i)
 {
  if(FamilySource(i)!=Source||!Upgrades->HasUpgradeId(BuildFamilies[i].Id)) continue;
  BuildCooldowns[i]=FMath::Max(0.0f,BuildCooldowns[i]-0.1f);
  if(BuildCooldowns[i]<=0 && ActivateBuildFamily(i,Character))
   BuildCooldowns[i]=FamilySpec(i).Cooldown/(1+BuildMagnitude(i,TEXT("Haste")));
 }
}
bool USurvivorAbilityComponent::ActivateBuildFamily(int32 Family,ACharacterBase* Character)
{
 if(Family<5||Family>=20||!IsValid(Character)||!Upgrades||!Upgrades->HasUpgradeId(BuildFamilies[Family].Id)||BuildCasts.Num()>80) return false;
 const auto Spec=FamilySpec(Family);
 const auto Source=FamilySource(Family);
 FBuildCast C;C.Family=Family;C.Character=Character;C.Origin=Character->GetActorLocation();
 C.Area=1+BuildMagnitude(Family,TEXT("Area"));C.Damage=Spec.Damage*Power(Character)*(1+BuildMagnitude(Family,TEXT("Power")));
 C.Steps=Spec.Count;C.Interval=Spec.Interval;C.Remaining=Tuning(C.Family,TEXT("InitialDelay"),0.1f);C.Life=Tuning(C.Family,TEXT("Lifetime"),8.0f);
 for(int32 i=0;i<3;++i) if(Branch(Family,i)) C.Branches|=1<<i;
 if(Spec.Pattern==EBuildPattern::Trail)
 {
  if(!bTrailInitialized){LastTrailPosition=C.Origin;bTrailInitialized=true;return false;}
  if(FVector::DistSquared2D(C.Origin,LastTrailPosition)<FMath::Square(Tuning(C.Family,TEXT("MovementSpacing"),160.0f))) return false;
  LastTrailPosition=C.Origin;C.Remaining=Tuning(C.Family,TEXT("ArmDelay"),0.4f);C.Interval=Tuning(C.Family,TEXT("TriggerPollInterval"),0.2f);C.Steps=(C.Branches&2)?FMath::Clamp(FMath::RoundToInt(Tuning(C.Family,TEXT("Charges"),2.0f,1)),1,64):1;
  const FVector Side=Character->GetVisualForwardVector().RotateAngleAxis(90,FVector::UpVector)*Tuning(C.Family,TEXT("SideOffset"),90.0f,0)*C.Area;
  if(C.Branches&1){C.Origin+=Side;BuildCasts.Add(C);C.Origin-=Side*2;}
  BuildCasts.Add(C);return true;
 }
 auto Targets=FindEnemies(C.Origin,Tuning(C.Family,TEXT("TargetRange"),1200.0f)*C.Area,Source);if(Targets.IsEmpty()) return false;
 C.Target=Targets[0];C.Direction=(Targets[0]->GetActorLocation()-C.Origin).GetSafeNormal2D();
 if(C.Direction.IsNearlyZero()) C.Direction=Character->GetVisualForwardVector().GetSafeNormal2D();
 C.End=FindCrowdCenter(Targets,Spec.Radius*C.Area);
 switch(Spec.Pattern)
 {
 case EBuildPattern::Travel:
  if(C.Branches&1) C.Steps*=2;
  if(C.Branches&2){auto Side=C;Side.Direction=Side.Direction.RotateAngleAxis(Tuning(C.Family,TEXT("SideAngle"),30.0f,1),FVector::UpVector);BuildCasts.Add(Side);}break;
 case EBuildPattern::Rupture:
  C.Remaining=Tuning(C.Family,TEXT("WarningDelay"),0.4f);if(C.Branches&2) C.Steps*=2;break;
 case EBuildPattern::Zone: case EBuildPattern::Smoke:
  if(!(C.Branches&1)) C.Origin=C.End;
  C.End=Targets.Last()->GetActorLocation();break;
 case EBuildPattern::Lance: if(C.Branches&2) C.Steps=2;break;
 case EBuildPattern::Ring: if(C.Branches&1) C.Steps*=2;break;
 case EBuildPattern::Mine:
  {
   const int32 Count=Spec.Count+((C.Branches&1)?FMath::Clamp(FMath::RoundToInt(Tuning(C.Family,TEXT("AdditionalMines"),2.0f,0)),0,32):0);
   const FVector Center=C.End;
   C.Steps=(C.Branches&2)?FMath::Clamp(FMath::RoundToInt(Tuning(C.Family,TEXT("Charges"),2.0f,1)),1,64):1;C.Remaining=Tuning(C.Family,TEXT("ArmDelay"),0.6f);C.Interval=Tuning(C.Family,TEXT("TriggerPollInterval"),0.2f);
   for(int32 i=0;i<Count;++i){C.Origin=Center+FVector(Tuning(C.Family,TEXT("ScatterRadius"),130.0f)*C.Area,0,0).RotateAngleAxis(i*360.0f/Count,FVector::UpVector);BuildCasts.Add(C);FamilyAccent(C.Family,C.Origin-FVector(0,0,70),C.Origin,Spec.Radius*C.Area,FamilyColor(Family)*0.3f,C.Remaining,false,-1,1);}
   return true;
  }
 case EBuildPattern::Ambush:
  {
   const int32 Count=(C.Branches&1)?FMath::Clamp(FMath::RoundToInt(Tuning(C.Family,TEXT("PhantomCount"),3.0f,0)),1,32):Spec.Count;const FVector Center=C.End;
   C.Steps=(C.Branches&2)?FMath::Clamp(FMath::RoundToInt(Tuning(C.Family,TEXT("SlashCount"),2.0f,1)),1,32):1;C.Remaining=Tuning(C.Family,TEXT("WarningDelay"),0.35f);
   for(int32 i=0;i<Count;++i){C.Origin=Center+FVector(Tuning(C.Family,TEXT("SpawnRadius"),240.0f)*C.Area,0,0).RotateAngleAxis(i*360.0f/Count,FVector::UpVector);C.Direction=(Center-C.Origin).GetSafeNormal2D();BuildCasts.Add(C);FamilyAccent(C.Family,C.Origin,Center,0,FamilyColor(Family)*0.3f,C.Remaining,true,-1,1);}
   return true;
  }
 default:break;
 }
 BuildCasts.Add(C);return true;
}
bool USurvivorAbilityComponent::BuildDamage(FBuildCast& C,AEnemyBase* Enemy,float Multiplier,bool bStatus,bool bMark,bool bPush,bool bOnce)
{
 if(!IsValid(Enemy)||Enemy->IsDead()||!Controller->IsRunInProgress()||Controller->IsPlayerDead()) return false;
 if(bOnce&&C.HitEnemies.Contains(Enemy)) return false;
 const auto Source=FamilySource(C.Family);const float Damage=C.Damage*Multiplier;
 if(!Enemy->ApplyPlayerDamage(Damage,Source)) return false;
 if(bOnce) C.HitEnemies.Add(Enemy);
 FamilyAccent(C.Family,Enemy->GetActorLocation(),Enemy->GetActorLocation(),FamilySpec(C.Family).Radius*C.Area,FamilyColor(C.Family),0.25f,false,-1,2);
 NotifyPartnerHit(Source,Enemy);
 if(!Enemy->IsDead())
 {
  if(bStatus) ApplyConfiguredStatus(C.Family,Enemy,Source==EPlayerAttackSource::Samurai?EEnemyStatusEffect::Bleed:EEnemyStatusEffect::Poison,Source);
  if(bMark) Enemy->ApplyMark();
  if(bPush) Enemy->ApplyAttackPushback(C.Origin,Source,Tuning(C.Family,TEXT("PushDistance"),65.0f),Tuning(C.Family,TEXT("PushDuration"),0.15f));
  RegisterFamilyHit(C.Family,Enemy,Damage);
 }
 return true;
}
void USurvivorAbilityComponent::BuildCircle(FBuildCast& C,FVector Position,float Radius,float Multiplier,bool bStatus,bool bMark,bool bPush,bool bOnce,float InnerRadius)
{
 FamilyAccent(C.Family,Position-FVector(0,0,70),Position,Radius,FamilyColor(C.Family),0.3f);
 for(auto* E:FindEnemies(Position,Radius,FamilySource(C.Family)))
  if(FVector::DistSquared2D(Position,E->GetActorLocation())>=FMath::Square(InnerRadius)) BuildDamage(C,E,Multiplier,bStatus,bMark,bPush,bOnce);
}
void USurvivorAbilityComponent::BuildLine(FBuildCast& C,FVector Start,FVector End,float Width,float Multiplier,bool bStatus,bool bMark)
{
 FamilyAccent(C.Family,Start,End,Width,FamilyColor(C.Family),0.25f,true);
 const FVector Delta=End-Start;const float Length=Delta.SizeSquared();
 for(auto* E:FindEnemies((Start+End)*0.5f,FVector::Distance(Start,End)*0.5f+Width,FamilySource(C.Family)))
 {
  const float T=Length>1?FMath::Clamp(FVector::DotProduct(E->GetActorLocation()-Start,Delta)/Length,0.0f,1.0f):0;
  if(FVector::DistSquared(E->GetActorLocation(),Start+Delta*T)<=FMath::Square(Width)) BuildDamage(C,E,Multiplier,bStatus,bMark,false,true);
 }
}
bool USurvivorAbilityComponent::StepBuildCast(FBuildCast& C)
{
 const auto S=FamilySpec(C.Family);const bool A=C.Branches&1,B=C.Branches&2,D=C.Branches&4;
 const float R=S.Radius*C.Area;const auto Source=FamilySource(C.Family);
 switch(S.Pattern)
 {
 case EBuildPattern::Travel:
  {
   const bool Returning=C.Step>=S.Count;if(C.Step==S.Count) C.HitEnemies.Reset();
   const int32 TravelStep=Returning?C.Steps-C.Step:C.Step+1;
   BuildCircle(C,C.Origin+C.Direction*TravelStep*Tuning(C.Family,TEXT("StepDistance"),120.0f)*C.Area,R,Returning?Tuning(C.Family,TEXT("ReturnDamageMultiplier"),0.7f,0):1,D,false,false,true);break;
  }
 case EBuildPattern::Orbit:
  {
   if(C.Character.IsValid()&&C.Character->GetCharacterMode()==ECharacterMode::Active) C.Origin=C.Character->GetActorLocation();
   C.HitEnemies.Reset();
   for(int32 Ring=0;Ring<(B?FMath::Clamp(FMath::RoundToInt(Tuning(C.Family,TEXT("OrbitCount"),2.0f,1)),1,8):1);++Ring)
    for(int32 i=0;i<(A?FMath::Clamp(FMath::RoundToInt(Tuning(C.Family,TEXT("BladeCount"),4.0f,0)),1,16):FMath::Clamp(FMath::RoundToInt(Tuning(C.Family,TEXT("BladeCount"),3.0f)),1,16));++i)
    {const float Angle=(i==3?-1:1)*C.Step*Tuning(C.Family,TEXT("DegreesPerStep"),32.0f)+i*Tuning(C.Family,TEXT("BladeSpacingDegrees"),120.0f);BuildCircle(C,C.Origin+FVector((Tuning(C.Family,TEXT("OrbitRadius"),220.0f)+Ring*Tuning(C.Family,TEXT("OuterOrbitSpacing"),140.0f,1))*C.Area,0,0).RotateAngleAxis(Angle,FVector::UpVector),R,1,D,false,false,true);}
   break;
  }
 case EBuildPattern::Rupture:
  {
   const int32 Step=C.Step%S.Count;const float Mult=C.Step>=S.Count?Tuning(C.Family,TEXT("EchoDamageMultiplier"),0.5f,1):1;
   BuildCircle(C,C.Origin+C.Direction*(Step+1)*Tuning(C.Family,TEXT("StepDistance"),220.0f)*C.Area,R,Mult,false,false,D);
   if(A) BuildCircle(C,C.Origin+C.Direction.RotateAngleAxis(Tuning(C.Family,TEXT("CrossAngle"),90.0f,0),FVector::UpVector)*(Step+1)*Tuning(C.Family,TEXT("StepDistance"),220.0f)*C.Area,R,Mult,false,false,D);
   break;
  }
 case EBuildPattern::Zone: case EBuildPattern::Smoke:
  {
   const bool Smoke=S.Pattern==EBuildPattern::Smoke;
   if(A&&C.Character.IsValid()&&C.Character->GetCharacterMode()==ECharacterMode::Active) C.Origin=C.Character->GetActorLocation();
   const float Mult=Smoke&&D&&C.Step==C.Steps-1?Tuning(C.Family,TEXT("FinalDamageMultiplier"),3.0f,2):1;
   BuildCircle(C,C.Origin,R,Mult,Smoke||D,!Smoke,Smoke);
   if(B) BuildCircle(C,C.End,R*Tuning(C.Family,TEXT("SecondaryRadiusMultiplier"),1,1),Mult*Tuning(C.Family,TEXT("SecondaryDamageMultiplier"),1,1),Smoke||D,!Smoke,Smoke);break;
  }
 case EBuildPattern::Flurry: case EBuildPattern::Swarm:
  {
   const bool Swarm=S.Pattern==EBuildPattern::Swarm;
   if(!C.Target.IsValid()||C.Target->IsDead())
   {if(!A) return false;auto Targets=FindEnemies(C.End,Tuning(C.Family,TEXT("RetargetRange"),600.0f,0)*C.Area,Source);if(Targets.IsEmpty())return false;C.Target=Targets[0];}
   C.End=C.Target->GetActorLocation();FamilyAccent(C.Family,C.End+FVector(0,0,160),C.End,0,FamilyColor(C.Family),0.2f,true);
   BuildDamage(C,C.Target.Get(),1,Swarm||D);
   if(B) for(auto* E:FindEnemies(C.End,R,Source)) if(E!=C.Target.Get()) BuildDamage(C,E,Tuning(C.Family,TEXT("SplashDamageMultiplier"),0.5f,1),Swarm||D);
   if(Swarm&&D&&C.Step==C.Steps-1) BuildCircle(C,C.End,R*Tuning(C.Family,TEXT("FinalRadiusMultiplier"),2.0f,2),Tuning(C.Family,TEXT("FinalDamageMultiplier"),3.0f,2),true);break;
  }
 case EBuildPattern::Lance:
  C.HitEnemies.Reset();
  BuildLine(C,C.Origin,C.Origin+C.Direction*Tuning(C.Family,TEXT("TargetRange"),1200.0f)*C.Area,R,C.Step?Tuning(C.Family,TEXT("EchoDamageMultiplier"),0.5f,1):1,false,D);
  if(A) for(float Angle:{-Tuning(C.Family,TEXT("SideAngle"),25.0f,0),Tuning(C.Family,TEXT("SideAngle"),25.0f,0)}) BuildLine(C,C.Origin,C.Origin+C.Direction.RotateAngleAxis(Angle,FVector::UpVector)*Tuning(C.Family,TEXT("TargetRange"),1200.0f)*C.Area,R,C.Step?Tuning(C.Family,TEXT("EchoDamageMultiplier"),0.5f,1):1,false,D);
  break;
 case EBuildPattern::Ring:
  {
   if(C.Step==S.Count) C.HitEnemies.Reset();
   const int32 RingStep=C.Step>=S.Count?C.Steps-C.Step:C.Step+1;
   BuildCircle(C,C.Origin,R*RingStep,1,true,false,D,true,FMath::Max(0.0f,R*(RingStep-1)));
   if(B) BuildCircle(C,C.End,R*RingStep*Tuning(C.Family,TEXT("SecondaryRadiusMultiplier"),1,1),Tuning(C.Family,TEXT("SecondaryDamageMultiplier"),1,1),true,false,D,true,FMath::Max(0.0f,R*(RingStep-1)));break;
  }
 case EBuildPattern::Mine: case EBuildPattern::Trail:
  {
   const bool Trail=S.Pattern==EBuildPattern::Trail;
   if(FindEnemies(C.Origin,R*Tuning(C.Family,TEXT("TriggerRadiusMultiplier"),0.75f),Source).IsEmpty())
   {FamilyAccent(C.Family,C.Origin-FVector(0,0,70),C.Origin,R,FamilyColor(C.Family)*0.2f,0.25f);return true;}
   BuildCircle(C,C.Origin,R,1,Trail||D,false,Trail&&D);C.Remaining=Tuning(C.Family,TEXT("RearmDelay"),0.4f,1);break;
  }
 case EBuildPattern::Ricochet:
  {
   FVector From=C.Origin;TWeakObjectPtr<AEnemyBase> First;
   for(int32 i=0;i<S.Count;++i)
   {
    auto Targets=FindEnemies(From,i?R:Tuning(C.Family,TEXT("TargetRange"),1200.0f)*C.Area,Source);AEnemyBase* Target=nullptr;
    for(auto* E:Targets)if(!C.HitEnemies.Contains(E)){Target=E;break;}
    if(!Target) break;if(i==0) First=Target;
    const FVector To=Target->GetActorLocation();FamilyAccent(C.Family,From,To,0,FamilyColor(C.Family),0.3f,true);
    BuildDamage(C,Target,1,D,false,false,true);
    if(B) for(auto* E:FindEnemies(To,Tuning(C.Family,TEXT("SplashRadius"),120.0f,1)*C.Area,Source)) if(E!=Target) BuildDamage(C,E,Tuning(C.Family,TEXT("SplashDamageMultiplier"),0.3f,1),D);
    From=To;
   }
   if(A&&First.IsValid()){FamilyAccent(C.Family,From,First->GetActorLocation(),0,FamilyColor(C.Family),0.3f,true);BuildDamage(C,First.Get(),Tuning(C.Family,TEXT("ReturnDamageMultiplier"),0.5f,0),D);}
   return false;
  }
 case EBuildPattern::Wire:
  {
   C.HitEnemies.Reset();const FVector End=C.Origin+C.Direction*Tuning(C.Family,TEXT("WireLength"),1000.0f)*C.Area;
   BuildLine(C,C.Origin,End,R,1,D);
   if(A){const FVector Side=C.Direction.RotateAngleAxis(Tuning(C.Family,TEXT("CrossAngle"),90.0f,0),FVector::UpVector)*Tuning(C.Family,TEXT("CrossHalfLength"),450.0f,0)*C.Area;BuildLine(C,C.End-Side,C.End+Side,R,1,D);}
   if(B){BuildCircle(C,C.Origin,Tuning(C.Family,TEXT("EndpointRadius"),150.0f,1)*C.Area,Tuning(C.Family,TEXT("EndpointDamageMultiplier"),0.5f,1),D);BuildCircle(C,End,Tuning(C.Family,TEXT("EndpointRadius"),150.0f,1)*C.Area,Tuning(C.Family,TEXT("EndpointDamageMultiplier"),0.5f,1),D);}break;
  }
 case EBuildPattern::Ambush:
  FamilyAccent(C.Family,C.Origin,C.Origin+C.Direction*R,0,FamilyColor(C.Family),0.3f,true);
  for(auto* E:FindEnemies(C.Origin,R,Source))
   if(FVector::DotProduct((E->GetActorLocation()-C.Origin).GetSafeNormal2D(),C.Direction)>FMath::Cos(FMath::DegreesToRadians(Tuning(C.Family,TEXT("ConeHalfAngle"),75.52249f)))) BuildDamage(C,E,C.Step?Tuning(C.Family,TEXT("EchoDamageMultiplier"),0.5f,1):1,D);
  break;
 case EBuildPattern::Needle:
  {
   auto Targets=FindEnemies(C.Origin,Tuning(C.Family,TEXT("TargetRange"),1200.0f)*C.Area,Source);
   Targets.Sort([](const auto& X,const auto& Y){return X.GetHealthComponent()->GetCurrentHealth()<Y.GetHealthComponent()->GetCurrentHealth();});
   for(int32 i=0;i<FMath::Min(Targets.Num(),A?FMath::Clamp(FMath::RoundToInt(Tuning(C.Family,TEXT("TargetCount"),2.0f,0)),1,32):S.Count);++i)
   {
    auto* E=Targets[i];const FVector Position=E->GetActorLocation();
    FamilyAccent(C.Family,C.Origin,Position,0,FamilyColor(C.Family),0.3f,true);
    BuildDamage(C,E,E->HasStatus(EEnemyStatusEffect::Bleed)?Tuning(C.Family,TEXT("BleedDamageMultiplier"),1.5f):1,D);
    if(B) for(auto* Other:FindEnemies(Position,R*Tuning(C.Family,TEXT("SplashRadiusMultiplier"),2.0f,1),Source)) if(Other!=E) BuildDamage(C,Other,Tuning(C.Family,TEXT("SplashDamageMultiplier"),0.5f,1),D);
   }return false;
  }
 default:return false;
 }
 return ++C.Step<C.Steps;
}
void USurvivorAbilityComponent::RegisterFamilyHit(int32 Family,AEnemyBase* Enemy,float Damage)
{
 if(bResolvingReaction||Family<0||Family>=20||!IsValid(Enemy)||Enemy->IsDead()||!Upgrades||!Upgrades->HasUpgradeId(BuildFamilies[Family].Synergy)) return;
 for(auto& Mark:BuildMarks) if(Mark.Enemy==Enemy&&Mark.Family==Family)
 {if(!Mark.bSpent){Mark.Damage=Damage;Mark.Remaining=(Tuning(Family,TEXT("PreparationDuration"),6.0f,3)+Upgrades->GetMetaSkillBonus(TEXT("Preparation")));}return;}
 if(BuildMarks.Num()<512){FBuildMark Mark;Mark.Enemy=Enemy;Mark.Family=Family;Mark.Damage=Damage;Mark.Remaining=(Tuning(Family,TEXT("PreparationDuration"),6.0f,3)+Upgrades->GetMetaSkillBonus(TEXT("Preparation")));BuildMarks.Add(Mark);}
}
void USurvivorAbilityComponent::NotifyPartnerHit(EPlayerAttackSource Source,AEnemyBase* Enemy)
{
 if(bResolvingReaction||!Controller||!Upgrades||!IsValid(Enemy)||!Controller->IsRunInProgress()||Controller->IsPlayerDead()) return;
 if(Source!=EPlayerAttackSource::Samurai&&Source!=EPlayerAttackSource::Ninja) return;
 TGuardValue<bool> Guard(bResolvingReaction,true);
 // Each family has a shared trigger throttle, plus a short per-victim re-prime lockout.
 for(auto& Mark:BuildMarks)
 {
  if(Mark.Enemy!=Enemy||Mark.bSpent||FamilySource(Mark.Family)==Source||ReactionGates[Mark.Family]>0) continue;
  Mark.bSpent=true;Mark.Remaining=Tuning(Mark.Family,TEXT("ReprimeDelay"),2.0f,3);
  const auto Spec=FamilySpec(Mark.Family);ReactionGates[Mark.Family]=FMath::Max(0.1f,Tuning(Mark.Family,TEXT("TriggerCooldown"),Spec.Reaction==EBuildReaction::Recharge?1:0.35f,3));
  FBuildCast C;C.Family=Mark.Family;C.Origin=Enemy->GetActorLocation();C.Damage=Mark.Damage*Spec.ReactionFactor*(1.f+Upgrades->GetMetaSkillBonus(TEXT("Reaction")));
  C.Area=1+BuildMagnitude(C.Family,TEXT("Area"));const float Radius=Spec.ReactionRadius*C.Area;
  switch(Spec.Reaction)
  {
  case EBuildReaction::Bloom:BuildCircle(C,C.Origin,Radius,1,true);break;
  case EBuildReaction::Pull:
   for(auto* E:FindEnemies(C.Origin,Radius,FamilySource(C.Family)))
   {BuildDamage(C,E);if(!E->IsDead()) E->ApplyAttackPushback(E->GetActorLocation()*2-C.Origin,FamilySource(C.Family),Tuning(C.Family,TEXT("PullDistance"),100.0f,3),Tuning(C.Family,TEXT("PullDuration"),0.2f,3));}
   FamilyAccent(C.Family,C.Origin-FVector(0,0,70),C.Origin,Radius,FamilyColor(C.Family));break;
  case EBuildReaction::Chain:
   {
    FVector From=C.Origin;auto Targets=FindEnemies(From,Radius,FamilySource(C.Family));
    for(int32 i=0;i<FMath::Min(Targets.Num(),Spec.ReactionCount);++i)
    {const FVector To=Targets[i]->GetActorLocation();FamilyAccent(C.Family,From,To,0,FamilyColor(C.Family),0.3f,true);BuildDamage(C,Targets[i],1,true);From=To;}
    break;
   }
  case EBuildReaction::Echo:
   for(int32 i=0;i<Spec.ReactionCount&&Pending.Num()<64;++i)
   {FPendingPulse P;P.Position=C.Origin;P.Damage=C.Damage/Spec.ReactionCount;P.Radius=Radius;P.Source=FamilySource(C.Family);P.Color=FamilyColor(C.Family);P.Remaining=Tuning(C.Family,TEXT("EchoInterval"),0.25f,3)*(i+1);P.bSkyStrike=true;P.bReaction=true;P.VisualFamily=C.Family;P.VisualSlot=3;Pending.Add(P);}break;
  case EBuildReaction::Recharge:
   if(C.Family<4) Cooldowns[C.Family]=FMath::Max(0.0f,Cooldowns[C.Family]-Tuning(C.Family,TEXT("CooldownRefund"),1.0f,3));
   else BuildCooldowns[C.Family]=FMath::Max(0.0f,BuildCooldowns[C.Family]-Tuning(C.Family,TEXT("CooldownRefund"),1.0f,3));
   BuildDamage(C,Enemy,1,true);FamilyAccent(C.Family,C.Origin+FVector(0,0,150),C.Origin,0,FamilyColor(C.Family),0.3f,true);break;
  case EBuildReaction::Focus:
   BuildDamage(C,Enemy,1,true);FamilyAccent(C.Family,C.Origin+FVector(0,0,250),C.Origin,0,FamilyColor(C.Family),0.3f,true);break;
  }
 }
}
void USurvivorAbilityComponent::BladeWaveImpact(AEnemyBase* Enemy,float Damage,bool bSplinter)
{
 if(!Controller||!Upgrades) return;
 NotifyPartnerHit(EPlayerAttackSource::Samurai,Enemy);RegisterFamilyHit(4,Enemy,Damage);
 if(bSplinter&&Branch(4,2))
 {FBuildCast C;C.Family=4;C.Damage=Damage*Tuning(4,TEXT("SplinterDamageMultiplier"),0.3f,2);C.Origin=Enemy->GetActorLocation();BuildCircle(C,C.Origin,Tuning(4,TEXT("SplinterRadius"),120.0f,2)*(1+BuildMagnitude(4,TEXT("Area"))),1,true);}
}
void USurvivorAbilityComponent::GrantBuildPreview(FString FamilyId,int32 SelectedBranch)
{
#if !UE_BUILD_SHIPPING
 if(!Controller||!Upgrades||!Controller->IsRunInProgress())return;
 for(int32 i=0;i<20;++i)
 {
  const auto& S=BuildFamilies[i];if(!FamilyId.Equals(S.Id,ESearchCase::IgnoreCase)&&!FamilyId.Equals(TEXT("All"),ESearchCase::IgnoreCase))continue;
  auto Grant=[this](const FString& Path,int32 Level)
  {if(auto* U=LoadObject<UUpgradeDefinition>(nullptr,*Path))while(Upgrades->GetUpgradeLevel(U)<Level)if(!Upgrades->AcquireUpgrade(U))break;};
  const FString Root=FString(TEXT("/Game/HeavensDivide/Upgrades/"))+S.Owner+TEXT("/DA_Upgrade_")+S.Owner;
  Grant(Root+S.Id,1);
  for(const TCHAR* Suffix:{TEXT("Power"),TEXT("Area"),TEXT("Haste")}) Grant(Root+(i==4&&FCString::Strcmp(Suffix,TEXT("Area"))==0?FString(TEXT("WideArc")):FString(S.Id)+Suffix),2);
  for(int32 b=0;b<3;++b) if(SelectedBranch==0||SelectedBranch==b+1) Grant(Root+S.Branches[b],1);
  Grant(FString(TEXT("/Game/HeavensDivide/Upgrades/Synergy/DA_BuildSynergy_"))+S.Synergy,1);
  Controller->ClientMessage(FString::Printf(TEXT("%s build ready: scaling rank 2, selected branches, partner synergy. This run only."),S.Title));
 }
#endif
}
