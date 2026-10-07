#include "NinjaBuildProjectile.h"
#include "UpgradeProcVFX.h"
#include "NinjaBuildComponent.h"
#include "NinjaCharacter.h"
#include "ShadowClone.h"
#include "EnemyBase.h"
#include "PlayerUpgradeComponent.h"
#include "ShurikenBuild.h"
#include "FangBuild.h"
#include "InactiveCharacterAssistComponent.h"
#include "SurvivorAbilityComponent.h"
#include "NiagaraFunctionLibrary.h"

float ANinjaBuildProjectile::SerrationBonus(AEnemyBase* Enemy) const
{
 const auto* B=Build.Get();
 if (!B || !B->Has(TEXT("SerratedEdge"))) return 1.f;
 const int32 Rank=B->Upgrades()->GetUpgradeLevelById(TEXT("ShurikenGrooves"));
 const float PerHit=B->Tune(TEXT("SerratedEdge"),TEXT("DamagePerHit"),.15f)+Rank*B->Tune(TEXT("ShurikenGrooves"),TEXT("PerHitPerRank"),.03f);
 const float Cap=B->Tune(TEXT("SerratedEdge"),TEXT("DamageCap"),.75f)+Rank*B->Tune(TEXT("ShurikenGrooves"),TEXT("CapPerRank"),.15f);
 return 1.f+FMath::Clamp(HitCounts.FindRef(Enemy)*PerHit,0.f,Cap);
}

void ANinjaBuildProjectile::UpdateShurikenGrowth()
{
 const auto* B=Build.Get(); if (!B) return;
 if (InitialRadius<=0) InitialRadius=Radius;
 const float MaxGrowth=FMath::Max(1.f,B->Tune(TEXT("WideOrbit"),TEXT("MaxSizeMultiplier"),2.f)
     +FangBuild::Scaling(B->Upgrades(),TEXT("ShurikenGrowth"),.2f));
 float Growth=B->Has(TEXT("WideOrbit")) ? (B->Tune(TEXT("WideOrbit"),TEXT("MaxSizeMultiplier"),2.f)-1.f
     +FangBuild::Scaling(B->Upgrades(),TEXT("ShurikenGrowth"),.2f))*FMath::Clamp(Age/B->GetShurikenLifetime(),0.f,1.f) : 0.f;
 if (B->Has(TEXT("ShurikenHunger"))) Growth+=ShurikenKills*B->Tune(TEXT("ShurikenHunger"),TEXT("GrowthPerKill"),.1f)
     *(1.f+FangBuild::Scaling(B->Upgrades(),TEXT("ShurikenSize"),.15f));
 Radius=InitialRadius*FMath::Clamp(1.f+Growth,1.f,MaxGrowth);
 UpdateShurikenVisualScale();
}

void ANinjaBuildProjectile::ShurikenHit(AEnemyBase* Enemy, bool bBurst)
{
 auto* B=Build.Get();
 if (!B || !Enemy || Enemy->IsDead() || !Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Ninja)) return;
 float Amount=Damage*SerrationBonus(Enemy);
 if (bBurst) Amount*=B->Tune(TEXT("BreakingWheel"),TEXT("DamageFraction"),.75f)
     *(1.f+FangBuild::Scaling(B->Upgrades(),TEXT("ShurikenBurstPower"),.15f));
 else
 {
     if (B->Has(TEXT("ShurikenOrbit"))) Amount*=B->Tune(TEXT("ShurikenOrbit"),TEXT("ContactMultiplier"),.7f);
     if (B->Has(TEXT("ShurikenPulse"))) Amount*=B->Tune(TEXT("ShurikenPulse"),TEXT("ContactMultiplier"),.65f);
 }
 const bool bGrind=!bBurst && !bGrindingUsed && B->Has(TEXT("GrindingHalt")) && Enemy->GetDropCategory()!=EEnemyDropCategory::Normal;
 const float Dealt=B->Hit(Enemy,Amount,false,bAssistProjectile,true);
 if (Dealt<=0) return;
 if (!bBurst) {
     ++HitCounts.FindOrAdd(Enemy);
     if (B->Has(TEXT("SerratedEdge")) && SerrationBonus(Enemy) >= 1.f+B->Tune(TEXT("SerratedEdge"),TEXT("DamageCap"),.75f))
         PlayUpgradeProcVFX(B->Upgrades(), TEXT("SerratedEdge"), Enemy->GetActorLocation(), 45.f);
 }
 if (bGrind)
 {
     bGrindingUsed=true;
     PlayUpgradeProcVFX(B->Upgrades(), TEXT("GrindingHalt"), GetActorLocation(), Radius);
     SlowRemaining=B->Tune(TEXT("GrindingHalt"),TEXT("SlowDuration"),1.f)
         +FangBuild::Scaling(B->Upgrades(),TEXT("ShurikenGrindDuration"),.2f);
 }
 if (!Enemy->IsDead()) return;
 ++ShurikenKills;
 if (B->Has(TEXT("ShurikenHunger"))) PlayUpgradeProcVFX(B->Upgrades(), TEXT("ShurikenHunger"), GetActorLocation(), Radius*.5f);
 // Contact and burst kills share procs. Neither kill path creates another burst.
 if (!bCloneProjectile && !bAssistProjectile && B->IsActive() && B->Has(TEXT("ShurikenAssist"))
     && FMath::FRand()<FMath::Clamp(B->Tune(TEXT("ShurikenAssist"),TEXT("Chance"),.05f)
         +FangBuild::Scaling(B->Upgrades(),TEXT("ShurikenAssistChance"),.05f),0.f,1.f))
     if (auto* Assist=B->Upgrades()->GetOwner()->FindComponentByClass<UInactiveCharacterAssistComponent>()) Assist->TryShurikenAssist();
}

void ANinjaBuildProjectile::ShurikenBurst()
{
 auto* B=Build.Get(); if (!B || !B->Has(TEXT("BreakingWheel"))) return;
 const float BurstRadius=Radius*B->Tune(TEXT("BreakingWheel"),TEXT("RadiusMultiplier"),2.f);
 for (auto* Enemy:B->Targets(GetActorLocation(),BurstRadius)) ShurikenHit(Enemy,true);
 if (B->ShurikenBurstVFX)
     PlayScaledUpgradeBurst(this,B->ShurikenBurstVFX,GetActorLocation(),BurstRadius/190.f);
 else if (auto* FX=B->Upgrades()->GetOwner()->FindComponentByClass<USurvivorAbilityComponent>())
     FX->UpgradeAccent(TEXT("BreakingWheel"),GetActorLocation(),BurstRadius,FLinearColor(.65f,.25f,1.f));
 UpdateShurikenGrowth();
}

void ANinjaBuildProjectile::TickShuriken(float Delta)
{
 auto* B=Build.Get(); if (!B || bFinished) return;
 if (!B->Has(TEXT("GreatShuriken")) || (bCloneProjectile && !Clone.IsValid())) { Destroy(); return; }
 // Substeps preserve lifetime and contact cadence across frame hitches.
 if (Age>=B->GetShurikenLifetime()) { Finish(); return; }
 float Remaining=FMath::Max(0.f,Delta);
 while (Remaining>UE_SMALL_NUMBER && !bFinished)
 {
     const float Life=B->GetShurikenLifetime();
     if (Age>=Life) { Finish(); return; }
     const float Step=FMath::Min3(Remaining,.05f,Life-Age);
     Remaining-=Step; Age+=Step;
     const FVector Start=GetActorLocation();
     const float Movement=SlowRemaining>0 ? B->Tune(TEXT("GrindingHalt"),TEXT("SpeedMultiplier"),.15f):1.f;
     FVector End=Start+Direction*Speed*Step*Movement;
     if (B->Has(TEXT("ShurikenOrbit")))
     {
         // Assists orbit their arrival point. Player blades follow while Ninja is active,
         // then finish at their last center rather than following the hidden character.
         if (bCloneProjectile) OrbitOrigin=Clone->GetActorLocation()+FVector(0,0,50);
         else if (!bAssistProjectile && B->IsActive()) OrbitOrigin=B->Ninja()->GetActorLocation()+FVector(0,0,50);
         const float OrbitRadius=FMath::Max(1.f,B->Tune(TEXT("ShurikenOrbit"),TEXT("OrbitRadius"),230.f));
         OrbitAngle=FMath::Fmod(OrbitAngle+Speed/OrbitRadius*Step*Movement,2.f*PI);
         End=OrbitOrigin+FVector(FMath::Cos(OrbitAngle),FMath::Sin(OrbitAngle),0)*OrbitRadius;
     }
     SlowRemaining=FMath::Max(0.f,SlowRemaining-Step);
     UpdateShurikenGrowth();
     for (auto* Enemy:B->Sweep(Start,End,Radius))
     {
         const float* Last=LastHits.Find(Enemy);
         if (Last && Age-*Last+UE_KINDA_SMALL_NUMBER<ShurikenBuild::ContactInterval(B->Upgrades())) continue;
         LastHits.Add(Enemy,Age); ShurikenHit(Enemy,false);
     }
     SetActorLocation(End); AddActorLocalRotation(FRotator(0,Step*900.f,0));
     if (B->Has(TEXT("ShurikenPulse")) && B->Has(TEXT("BreakingWheel")))
     {
         PulseElapsed+=Step;
         if (PulseElapsed+UE_KINDA_SMALL_NUMBER>=ShurikenBuild::PulseInterval(B->Upgrades()))
         { PulseElapsed=FMath::Max(0.f,PulseElapsed-ShurikenBuild::PulseInterval(B->Upgrades())); ShurikenBurst(); }
     }
     if (Age+UE_SMALL_NUMBER>=Life) { Finish(); return; }
 }
}
