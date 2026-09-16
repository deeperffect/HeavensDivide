#include "CharacterBase.h"
#include "CharacterStatsComponent.h"
#include "EnemyBase.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "NinjaCharacter.h"
#include "PlayerUpgradeComponent.h"
#include "SharedPlayerStatsComponent.h"
#include "SurvivorPlayerController.h"

void ACharacterBase::ExecuteComboAbility_Implementation(float Radius, float Damage)
{
    auto* PC = Cast<ASurvivorPlayerController>(GetOwner());
    if (!PC || !PC->IsRunInProgress() || PC->IsPlayerDead() || !GetWorld()) return;
    const bool bNinja = IsA<ANinjaCharacter>();
    const auto Source = bNinja ? EPlayerAttackSource::Ninja : EPlayerAttackSource::Samurai;
    float Power = GetCharacterStats() ? GetCharacterStats()->GetFinalDamageMultiplier() : 1.f;
    if (auto* Shared = PC->GetSharedPlayerStats()) Power *= Shared->GetFinalDamageMultiplier();
    if (auto* Upgrades = PC->GetPlayerUpgrades()) Power *= bNinja ? Upgrades->GetNinjaPowerMultiplier() : Upgrades->GetSamuraiPowerMultiplier();
    TArray<FOverlapResult> Hits;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
    GetWorld()->OverlapMultiByObjectType(Hits, GetActorLocation(), FQuat::Identity, Objects,
        FCollisionShape::MakeSphere(FMath::Max(1.f, Radius)), FCollisionQueryParams(SCENE_QUERY_STAT(ComboAbility), false, this));
    TSet<AEnemyBase*> Seen;
    for (const auto& Hit : Hits)
    {
        if (!PC->IsRunInProgress() || PC->IsPlayerDead()) break;
        auto* Enemy = Cast<AEnemyBase>(Hit.GetActor());
        if (!IsValid(Enemy) || Enemy->IsDead() || Seen.Contains(Enemy)) continue;
        Seen.Add(Enemy);
        Enemy->ApplyPlayerDamage(Damage * Power, Source);
    }
}
