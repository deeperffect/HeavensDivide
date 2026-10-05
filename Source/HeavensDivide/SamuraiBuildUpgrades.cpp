#include "PlayerUpgradeComponent.h"
#include "CrescentBuild.h"
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
void GrantBloodRush(UPlayerUpgradeComponent* Upgrades)
{
 if (!Upgrades || !Upgrades->HasUpgradeId(TEXT("BattleStance")) || !Upgrades->HasUpgradeId(TEXT("BloodRush"))) return;
 auto* PC = Cast<ASurvivorPlayerController>(Upgrades->GetOwner());
 auto* Samurai = PC && PC->GetCharacterManager() ? Cast<ASamuraiCharacter>(PC->GetCharacterManager()->GetActiveCharacter()) : nullptr;
 if (!Samurai || PC->IsPlayerDead()) return;
 const auto* Card = Upgrades->FindUpgradeDefinition(TEXT("BloodRush"));
 Samurai->ApplyBloodRush(Card ? Card->GetBalanceValue(TEXT("MoveSpeedBonus"), .2f) : .2f,
     Card ? Card->GetBalanceValue(TEXT("Duration"), 3.f) : 3.f);
}
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
void ShowProc(UPlayerUpgradeComponent* Upgrades,FName Id,FVector Position,float Radius,FLinearColor Color)
{
 if(Upgrades&&Upgrades->GetOwner())
  if(auto* FX=Upgrades->GetOwner()->FindComponentByClass<USurvivorAbilityComponent>())
   FX->UpgradeAccent(Id,Position-FVector(0,0,70),Radius,Color);
}
}

void UPlayerUpgradeComponent::HandleSamuraiDirectHit(AEnemyBase* Enemy,float Damage,float HealthBeforeHit)
{
 if (Enemy && !Enemy->IsDead() && HasUpgradeId(TEXT("BladeWave")) && !HasUpgradeId(TEXT("CrescentEruptionPact")))
  Enemy->ApplyCrescentSlow(CrescentBuild::Value(this,TEXT("BladeWave"),TEXT("SlowFraction"),.3f)
   + CrescentBuild::Scaling(this,TEXT("CrescentSlow"),.05f),
   CrescentBuild::Value(this,TEXT("BladeWave"),TEXT("SlowDuration"),5.f)
   + CrescentBuild::Scaling(this,TEXT("CrescentSlowDuration"),1.f));
 if(!Enemy||!Enemy->IsDead()||HealthBeforeHit<=0||!FMath::IsFinite(Damage)||!HasUpgradeId(TEXT("OverkillBurst")))return;
 const auto* Card=FindUpgradeDefinition(TEXT("OverkillBurst"));
 const float Overkill=FMath::Max(0.f,Damage-HealthBeforeHit)*FMath::Max(0.f,Card?Card->GetBalanceValue(TEXT("DamageMultiplier"),1.f):1.f);
 const float Radius=FMath::Max(0.f,Card?Card->GetBalanceValue(TEXT("Radius"),220.f):220.f)*SamuraiArea(this)
  *(1.f+FMath::Max(0.f,GetAccumulatedUpgradeMagnitude(TEXT("BurstRadius"))));
 if(Overkill<=0||Radius<=0)return;
 ShowProc(this,TEXT("OverkillBurst"),Enemy->GetActorLocation(),Radius,FLinearColor(3.f,0.65f,0.1f));
 // Proc damage deliberately bypasses this direct-hit hook: explosions cannot chain themselves.
 for(auto* Target:NearbySamuraiTargets(Enemy,Radius))Target->ApplyPlayerDamage(Overkill,EPlayerAttackSource::Samurai);
}

void UEnemyStatusEffectComponent::ReceiveBloodStacks(UPlayerUpgradeComponent* Upgrades, int32 Stacks, float DamagePerTick, float Duration)
{
 if (!Upgrades || !Upgrades->HasUpgradeId(TEXT("BattleStance")) || Stacks <= 0) return;
 const int32 Before = BleedState.Stacks;
 const float PreviousDamage = BleedState.BleedHitBonusPerTick;
 if (!ApplyStatus(EEnemyStatusEffect::Bleed, Upgrades, EPlayerAttackSource::Samurai, true, 0.f)) return;
 const int32 Cap = 5 + FMath::Clamp(Upgrades->GetUpgradeLevelById(TEXT("BloodCapacity")),0,5);
 const int32 Accepted = FMath::Min(Stacks, Cap - Before);
 BleedState.Stacks = Before + Accepted;
 BleedState.BleedHitBonusPerTick = PreviousDamage + DamagePerTick * Accepted / Stacks;
 BleedState.RemainingDuration = FMath::Max(BleedState.RemainingDuration, Duration);
 OnStatusStacksChanged.Broadcast(EEnemyStatusEffect::Bleed, BleedState.Stacks);
}

void UEnemyStatusEffectComponent::TryBloodDetonation()
{
 auto* Upgrades = BleedState.SourceUpgrades.Get();
 auto* Enemy = Cast<AEnemyBase>(GetOwner());
 if (!Upgrades || !Enemy || !Upgrades->HasUpgradeId(TEXT("BloodDetonation")) ||
     BleedState.Stacks < 5 + FMath::Clamp(Upgrades->GetUpgradeLevelById(TEXT("BloodCapacity")),0,5)) return;
 const float Damage = 2.f * CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed);
 const float Radius = 300.f * SamuraiArea(Upgrades) * (1.f + .1f * Upgrades->GetUpgradeLevelById(TEXT("BloodTransferArea")));
 ClearStatus(EEnemyStatusEffect::Bleed); // Consume before any lethal damage invokes death callbacks.
 ShowProc(Upgrades,TEXT("BloodDetonation"),Enemy->GetActorLocation(),Radius,FLinearColor(3.f,.05f,.1f));
 const auto Targets = NearbySamuraiTargets(Enemy,Radius);
 Enemy->ApplyPlayerDamage(Damage,EPlayerAttackSource::Samurai);
 // The detonation consumed this victim's Bleed before its death callback.
 if (Enemy->IsDead()) GrantBloodRush(Upgrades);
 for(auto* Target:Targets) if(IsValid(Target) && !Target->IsDead()) Target->ApplyPlayerDamage(Damage,EPlayerAttackSource::Samurai);
}

void UEnemyStatusEffectComponent::TransferBleedOnDeath()
{
 auto* Enemy=Cast<AEnemyBase>(GetOwner());
 auto* Upgrades=BleedState.SourceUpgrades.Get();
 if(!Enemy||!Upgrades||!HasStatus(EEnemyStatusEffect::Bleed)||!Upgrades->HasUpgradeId(TEXT("BattleStance"))||
    !Upgrades->HasUpgradeId(TEXT("BloodTransfer"))||Upgrades->HasUpgradeId(TEXT("BloodDetonation")))return;
 const auto* Card=Upgrades->FindUpgradeDefinition(TEXT("BloodTransfer"));
 const float Radius=FMath::Max(0.f,Card?Card->GetBalanceValue(TEXT("Radius"),300.f):300.f)*SamuraiArea(Upgrades)
    *(1.f+.1f*Upgrades->GetUpgradeLevelById(TEXT("BloodTransferArea")));
 auto Targets=NearbySamuraiTargets(Enemy,Radius);
 if(Targets.IsEmpty()||BleedState.RemainingDuration<=0)return;
 ShowProc(Upgrades,TEXT("BloodTransfer"),Enemy->GetActorLocation(),Radius,FLinearColor(2.5f,.08f,.15f));
 for(auto* Target:Targets) Target->GetStatusEffectComponent()->ReceiveBloodStacks(Upgrades,BleedState.Stacks,BleedState.BleedHitBonusPerTick,BleedState.RemainingDuration);
}

void UEnemyStatusEffectComponent::GrantBloodRushOnDeath()
{
 if (HasStatus(EEnemyStatusEffect::Bleed)) GrantBloodRush(BleedState.SourceUpgrades.Get());
}
