#include "RunChallengeSubsystem.h"
#include "BalanceLabDirector.h"
#include "CinderOracleBoss.h"
#include "EncounterWall.h"
#include "EnemyBase.h"
#include "EnemySpawner.h"
#include "HealingUrn.h"
#include "SurvivorPlayerController.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "TacticalEnemy.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
const FRunChallengeModifier* FindChallenge(FName Id)
{
    return RunChallenges::Modifiers().FindByPredicate([Id](const FRunChallengeModifier& M){ return M.Id == Id; });
}
bool IsTrainingWorld(const UWorld* World)
{
    return !World || World->GetMapName().Contains(TEXT("BalanceLab")) || ABalanceLabDirector::Find(World);
}
bool IsCombatEnemy(const AEnemyBase* Enemy)
{
    return IsValid(Enemy) && !Enemy->IsStressTestEnemy() && !Enemy->IsA<AHealingUrn>() && !Enemy->IsA<AEncounterWall>()
        && !Enemy->GetClass()->GetName().Contains(TEXT("TestDummy")) && !IsTrainingWorld(Enemy->GetWorld());
}
}

const TArray<FRunChallengeModifier>& RunChallenges::Modifiers()
{
    static const TArray<FRunChallengeModifier> Values = {
        {TEXT("Health"), TEXT("Iron Resolve"), TEXT("Enemy health +15% per rank."), .15f},
        {TEXT("Damage"), TEXT("Cruel Strikes"), TEXT("Enemy damage +10% per rank."), .10f},
        {TEXT("Speed"), TEXT("Relentless Pursuit"), TEXT("Enemy movement speed +4% per rank."), .04f},
        {TEXT("Count"), TEXT("Gathering Horde"), TEXT("Maintained population +6% per rank. Maintained population capped at 200."), .06f},
        {TEXT("SpawnRate"), TEXT("Endless Assault"), TEXT("Enemy replenishment frequency +8% per rank."), .08f}
    };
    return Values;
}
int32 URunChallengeSubsystem::GetModifierRank(FName Id) const
{
    return FindChallenge(Id) ? FMath::Clamp(Ranks.FindRef(Id), 0, 5) : 0;
}
bool URunChallengeSubsystem::SetModifierRank(FName Id, int32 Rank)
{
    if (!FindChallenge(Id) || Rank < 0 || Rank > 5 || !CanEditModifiers()) return false;
    Ranks.Add(Id, Rank);
    return true;
}
float URunChallengeSubsystem::GetMultiplier(FName Id) const
{
    const auto* Modifier = FindChallenge(Id);
    return Modifier ? 1.f + Modifier->PerRank * GetModifierRank(Id) : 1.f;
}
float URunChallengeSubsystem::GetRewardMultiplier() const
{
    int32 Total = 0;
    for (const auto& Modifier : RunChallenges::Modifiers()) Total += GetModifierRank(Modifier.Id);
    return 1.f + .08f * Total;
}
bool URunChallengeSubsystem::CanEditModifiers() const
{
    const UWorld* World = GetWorld();
    if (!World) return true;
    for (auto It = World->GetPlayerControllerIterator(); It; ++It)
        if (Cast<ASurvivorPlayerController>(It->Get())) return false;
    return true;
}
void URunChallengeSubsystem::BeginRun()
{
    CommonKillsTowardEmber = 0;
}
bool URunChallengeSubsystem::ApplyEnemy(AEnemyBase* Enemy)
{
    if (!IsCombatEnemy(Enemy) || Enemy->bRunChallengeApplied) return false;
    Enemy->bRunChallengeApplied = true;
    Enemy->RunChallengeSpeedMultiplier = GetMultiplier(TEXT("Speed"));
    Enemy->ApplySpawnInstanceModifiers(GetMultiplier(TEXT("Health")), GetMultiplier(TEXT("Damage")), GetMultiplier(TEXT("Speed")));
    if (auto* Tactical = Cast<ATacticalEnemy>(Enemy)) Tactical->ChargeSpeed *= Enemy->RunChallengeSpeedMultiplier;
    if (auto* Oracle = Cast<ACinderOracleBoss>(Enemy)) Oracle->SpellDamage *= GetMultiplier(TEXT("Damage"));
    return true;
}
bool URunChallengeSubsystem::ApplySpawner(AEnemySpawner* Spawner)
{
    if (!IsValid(Spawner) || IsTrainingWorld(Spawner->GetWorld()) || Spawner->bRunChallengeApplied) return false;
    Spawner->bRunChallengeApplied = true;
    const float Count = GetMultiplier(TEXT("Count"));
    const float Frequency = GetMultiplier(TEXT("SpawnRate"));
    auto Population = [Count](int32 Value){ return FMath::Clamp(FMath::RoundToInt(Value * Count), 0, 200); };
    Spawner->AbsoluteHardAliveCap = FMath::Max(1, Population(Spawner->AbsoluteHardAliveCap));
    Spawner->MaxTimedThreatsAlive = Population(Spawner->MaxTimedThreatsAlive);
    for (auto& Phase : Spawner->PressurePhases)
    {
        Phase.GlobalMaxAlive = Population(Phase.GlobalMaxAlive);
        for (auto& Entry : Phase.EnemyPopulationEntries)
        {
            Entry.MaxPopulation = Population(Entry.MaxPopulation);
            Entry.DesiredPopulation = FMath::Min(Population(Entry.DesiredPopulation), Entry.MaxPopulation);
        }
        Phase.NormalSpawnInterval /= Frequency;
        Phase.AcceleratedSpawnInterval /= Frequency;
        Phase.EmergencySpawnInterval /= Frequency;
    }
    // Threat replacement uses its own cooldown, independent of the population refill timer.
    for (auto& Entry : Spawner->EnemySpawnEntries)
    {
        Entry.MinRespawnDelayAfterDeath /= Frequency;
        Entry.MaxRespawnDelayAfterDeath /= Frequency;
    }
    return true;
}
bool RunChallenges::CanEarnCurrencies(const UWorld* World)
{
    if (IsTrainingWorld(World)) return false;
    const auto* PC = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(World, 0));
    return PC && PC->IsRunInProgress() && !PC->IsPlayerDead();
}
void RunChallenges::AwardCurrencies(AActor* Source, int32 Embers, int32 Sigils)
{
    if (!IsValid(Source) || !CanEarnCurrencies(Source->GetWorld())) return;
    if (auto* GI = Source->GetGameInstance())
        if (auto* Meta = GI->GetSubsystem<USynergyMetaProgressionSubsystem>()) Meta->AwardCurrencies(Embers, Sigils);
}
void URunChallengeSubsystem::NotifyEnemyKilled(AEnemyBase* Enemy)
{
    if (!IsCombatEnemy(Enemy) || !Enemy->IsDead() || Enemy->bMetaKillRewardPaid || !Enemy->ShouldDropXP()
        || Enemy->GetDropCategory() == EEnemyDropCategory::Boss || !RunChallenges::CanEarnCurrencies(Enemy->GetWorld())) return;
    Enemy->bMetaKillRewardPaid = true;
    if (Enemy->GetDropCategory() == EEnemyDropCategory::Elite) RunChallenges::AwardCurrencies(Enemy, 3, 1);
    else if (Enemy->IsA<ATacticalEnemy>()) RunChallenges::AwardCurrencies(Enemy, 2, 0);
    else if (++CommonKillsTowardEmber >= 5)
    {
        CommonKillsTowardEmber = 0;
        RunChallenges::AwardCurrencies(Enemy, 1, 0);
    }
}
