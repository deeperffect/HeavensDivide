#include "EnemyStatusEffectComponent.h"
#include "UpgradeProcVFX.h"
#include "BarrageBuild.h"
#include "BarragePoisonPool.h"
#include "EnemyBase.h"
#include "HealthComponent.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "NinjaCharacter.h"
#include "Engine/World.h"
#include "TimerManager.h"
bool UEnemyStatusEffectComponent::ApplyBarragePoison(UPlayerUpgradeComponent* U,float HitDamage,bool bFromPuddle)
{
 auto* E=Cast<AEnemyBase>(GetOwner());
 if(!E||E->IsDead()||!U||!U->HasUpgradeId(TEXT("BarrageStance"))||!E->CanReceivePlayerDamage(EPlayerAttackSource::Ninja)||!E->GetHealthComponent()->IsDamageEnabled()||HitDamage<=0.f)return false;
 if(PoisonState.Stacks>0&&!PoisonState.bBarragePoison)ClearStatus(EEnemyStatusEffect::Poison);
 auto& S=PoisonState;const int32 Old=S.Stacks;
 const int32 Cap=FMath::Max(1,FMath::RoundToInt(BarrageBuild::Value(U,TEXT("BarrageStance"),TEXT("PoisonCap"),20.f)+BarrageBuild::Scaling(U,TEXT("BarragePoisonCap"),4.f)));
 const int32 Add=bFromPuddle?1:1+FMath::RoundToInt(BarrageBuild::Scaling(U,TEXT("BarragePoisonLoad"),2.f));
 const int32 Accepted=FMath::Clamp(Add,0,FMath::Max(0,Cap-S.Stacks));
 S.SourceUpgrades=U;S.bBarragePoison=true;S.bDirectBarragePoison|=!bFromPuddle;
 S.Stacks+=Accepted;
 S.PoisonDamagePerTick+=HitDamage*BarrageBuild::Value(U,TEXT("BarrageStance"),TEXT("PoisonHitFraction"),.05f)/10.f*Accepted;
 S.RemainingDuration=5.f+BarrageBuild::Scaling(U,TEXT("BarragePoisonDuration"),2.f);
 S.ActiveTickInterval=.5f;
 if(!GetWorld()->GetTimerManager().IsTimerActive(S.TickTimer))GetWorld()->GetTimerManager().SetTimer(S.TickTimer,this,&UEnemyStatusEffectComponent::TickPoison,.5f,true);
 if (Old==0 && !bFromPuddle) PlayUpgradeProcVFX(U,TEXT("BarrageStance"),E->GetActorLocation(),35.f);
 if(S.Stacks!=Old)OnStatusStacksChanged.Broadcast(EEnemyStatusEffect::Poison,S.Stacks);
 return true;
}
void UEnemyStatusEffectComponent::BarragePoisonDeath()
{
 const auto& S=PoisonState;auto* U=S.SourceUpgrades.Get();
 if(!S.bBarragePoison||S.Stacks<=0||!U||!U->HasUpgradeId(TEXT("BarrageStance")))return;
 auto* PC=Cast<ASurvivorPlayerController>(U->GetOwner());
 auto* N=PC&&PC->GetCharacterManager()?PC->GetCharacterManager()->GetNinja():nullptr;
 if(N&&U->HasUpgradeId(TEXT("BarrageRush"))&&!PC->IsPlayerDead()) { N->ApplyViperRush(BarrageBuild::Value(U,TEXT("BarrageRush"),TEXT("MoveSpeedBonus"),.2f),BarrageBuild::Value(U,TEXT("BarrageRush"),TEXT("Duration"),3.f)); }
 // Only poison refreshed by a direct kunai hit can seed another puddle.
 if(!S.bDirectBarragePoison||!U->HasUpgradeId(TEXT("BarragePool")))return;
 FActorSpawnParameters P;P.Owner=GetOwner();P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Pool=GetWorld()->SpawnActor<ABarragePoisonPool>(GetOwner()->GetActorLocation(),FRotator::ZeroRotator,P);
 if(Pool)Pool->Initialize(U,S.PoisonDamagePerTick/S.Stacks*10.f/FMath::Max(.0001f,BarrageBuild::Value(U,TEXT("BarrageStance"),TEXT("PoisonHitFraction"),.05f)));
}
