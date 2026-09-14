#include "PlayerUpgradeComponent.h"
#include "EnemyStatusEffectComponent.h"
#include "EnemyBase.h"
#include "HealthComponent.h"
#include "CharacterManagerComponent.h"
#include "SamuraiCharacter.h"
#include "SurvivorPlayerController.h"
#include "SurvivorAbilityComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

namespace
{
float SamuraiArea(const UPlayerUpgradeComponent* Upgrades)
{
 const auto* PC=Upgrades?Cast<ASurvivorPlayerController>(Upgrades->GetOwner()):nullptr;
 const auto* Samurai=PC&&PC->GetCharacterManager()?PC->GetCharacterManager()->GetSamurai():nullptr;
 return Samurai&&Samurai->GetCharacterStats()?Samurai->GetCharacterStats()->GetFinalAttackAreaMultiplier():1.f;
}
TArray<AEnemyBase*> NearbySamuraiTargets(AEnemyBase* Origin,float Radius)
{
 TArray<AEnemyBase*> Targets;
 if(!Origin||!Origin->GetWorld()||Radius<=0)return Targets;
 TArray<FOverlapResult> Hits;
 FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
 Origin->GetWorld()->OverlapMultiByObjectType(Hits,Origin->GetActorLocation(),FQuat::Identity,Objects,FCollisionShape::MakeSphere(Radius),FCollisionQueryParams(SCENE_QUERY_STAT(SamuraiBuildProc),false,Origin));
 for(const auto& Hit:Hits)
 {
  auto* Enemy=Cast<AEnemyBase>(Hit.GetActor());
  if(Enemy&&Enemy!=Origin&&!Enemy->IsDead()&&Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Samurai))Targets.AddUnique(Enemy);
 }
 const FVector Position=Origin->GetActorLocation();
 Targets.Sort([Position](const AEnemyBase& A,const AEnemyBase& B){return FVector::DistSquared(A.GetActorLocation(),Position)<FVector::DistSquared(B.GetActorLocation(),Position);});
 if(Targets.Num()>128)Targets.SetNum(128);
 return Targets;
}
void ShowProc(UPlayerUpgradeComponent* Upgrades,FVector Position,float Radius,FLinearColor Color)
{
 if(Upgrades&&Upgrades->GetOwner())
  if(auto* FX=Upgrades->GetOwner()->FindComponentByClass<USurvivorAbilityComponent>())
   FX->FamilyAccent(INDEX_NONE,Position-FVector(0,0,70),Position,Radius,Color,0.35f);
}
}

void UPlayerUpgradeComponent::HandleSamuraiDirectHit(AEnemyBase* Enemy,float Damage,float HealthBeforeHit)
{
 if(!Enemy||!Enemy->IsDead()||HealthBeforeHit<=0||!FMath::IsFinite(Damage)||!HasUpgradeId(TEXT("OverkillBurst")))return;
 const auto* Card=FindUpgradeDefinition(TEXT("OverkillBurst"));
 const float Overkill=FMath::Max(0.f,Damage-HealthBeforeHit)*FMath::Max(0.f,Card?Card->GetBalanceValue(TEXT("DamageMultiplier"),1.f):1.f);
 const float Radius=FMath::Max(0.f,Card?Card->GetBalanceValue(TEXT("Radius"),220.f):220.f)*SamuraiArea(this)
  *(1.f+FMath::Max(0.f,GetAccumulatedUpgradeMagnitude(TEXT("BurstRadius"))));
 if(Overkill<=0||Radius<=0)return;
 ShowProc(this,Enemy->GetActorLocation(),Radius,FLinearColor(3.f,0.65f,0.1f));
 // Proc damage deliberately bypasses this direct-hit hook: explosions cannot chain themselves.
 for(auto* Target:NearbySamuraiTargets(Enemy,Radius))Target->ApplyPlayerDamage(Overkill,EPlayerAttackSource::Samurai);
}

void UEnemyStatusEffectComponent::ReceiveBleedTransfer(UPlayerUpgradeComponent* Upgrades,float DamageBudget,float Duration)
{
 if(!Upgrades||!FMath::IsFinite(DamageBudget)||DamageBudget<=0||!FMath::IsFinite(Duration)||Duration<=0)return;
 const float PreviousDuration=BleedState.RemainingDuration;
 const float PreviousWeight=BleedState.BleedBaseStackWeight;
 if(!ApplyStatus(EEnemyStatusEffect::Bleed,Upgrades,EPlayerAttackSource::Samurai,true))return;
 // A transferred stack represents only its finite budget, without a free base-damage stack.
 BleedState.BleedBaseStackWeight=PreviousWeight;
 BleedState.RemainingDuration=FMath::Max(PreviousDuration,Duration);
 BleedState.TransferredDamageRemaining+=DamageBudget;
}

void UEnemyStatusEffectComponent::TransferBleedOnDeath()
{
 auto* Enemy=Cast<AEnemyBase>(GetOwner());
 auto* Upgrades=BleedState.SourceUpgrades.Get();
 if(!Enemy||!Upgrades||!HasStatus(EEnemyStatusEffect::Bleed)||!Upgrades->HasUpgradeId(TEXT("BloodTransfer")))return;
 const auto* Card=Upgrades->FindUpgradeDefinition(TEXT("BloodTransfer"));
 const float Fraction=FMath::Clamp(Card?Card->GetBalanceValue(TEXT("TransferFraction"),0.5f):0.5f,0.f,1.f);
 const float Budget=CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed)*Fraction;
 const float Radius=FMath::Max(0.f,Card?Card->GetBalanceValue(TEXT("Radius"),300.f):300.f)*SamuraiArea(Upgrades);
 const int32 Limit=FMath::Clamp(FMath::RoundToInt(Card?Card->GetBalanceValue(TEXT("Targets"),5.f):5.f),1,128);
 auto Targets=NearbySamuraiTargets(Enemy,Radius);
 if(Targets.Num()>Limit)Targets.SetNum(Limit);
 if(Targets.IsEmpty()||Budget<=0||BleedState.RemainingDuration<=0)return;
 ShowProc(Upgrades,Enemy->GetActorLocation(),Radius,FLinearColor(2.5f,0.08f,0.15f));
 const float PerTarget=Budget/Targets.Num();
 for(auto* Target:Targets)Target->GetStatusEffectComponent()->ReceiveBleedTransfer(Upgrades,PerTarget,BleedState.RemainingDuration);
}
