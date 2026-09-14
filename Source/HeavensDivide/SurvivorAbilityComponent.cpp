#include "SurvivorAbilityComponent.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "CharacterStatsComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnemyStatusEffectComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NinjaCharacter.h"
#include "PlayerUpgradeComponent.h"
#include "SamuraiCharacter.h"
#include "SharedPlayerStatsComponent.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FLinearColor Colors[] = {FLinearColor(3, 1.2f, 0.12f), FLinearColor(2, 2, 0.65f), FLinearColor(1.7f, 0.2f, 3),
                               FLinearColor(0.3f, 2, 0.55f)};
}
USurvivorAbilityComponent::USurvivorAbilityComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}
void USurvivorAbilityComponent::BeginPlay()
{
    Super::BeginPlay();
    Controller = Cast<ASurvivorPlayerController>(GetOwner());
    Upgrades = Controller ? Controller->GetPlayerUpgrades() : nullptr;
    GetWorld()->GetTimerManager().SetTimer(Scheduler, this, &USurvivorAbilityComponent::UpdateAbilities, 0.1f, true);
}
void USurvivorAbilityComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorld()->GetTimerManager().ClearTimer(Scheduler);
    ClearBuildFamilies();
    for (auto Effect : ActiveAccents)
        if (Effect.IsValid())
            Effect->Destroy();
    Super::EndPlay(Reason);
}
float USurvivorAbilityComponent::Power(ACharacterBase *Character) const
{
    const float CharacterPower =
        Character->GetCharacterStats() ? Character->GetCharacterStats()->GetFinalDamageMultiplier() : 1;
    const float Shared =
        Controller->GetSharedPlayerStats() ? Controller->GetSharedPlayerStats()->GetFinalDamageMultiplier() : 1;
    return CharacterPower * Shared *
           (Character->IsA<ASamuraiCharacter>() ? Upgrades->GetSamuraiPowerMultiplier()
                                                : Upgrades->GetNinjaPowerMultiplier());
}
TArray<AEnemyBase *> USurvivorAbilityComponent::FindEnemies(FVector Position, float Radius,
                                                            EPlayerAttackSource Source) const
{
    TArray<FOverlapResult> Hits;
    TArray<AEnemyBase *> Enemies;
    TSet<AEnemyBase *> Seen;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
    GetWorld()->OverlapMultiByObjectType(Hits, Position, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius),
                                         FCollisionQueryParams(SCENE_QUERY_STAT(SurvivorAbility), false, GetOwner()));
    for (const auto &Hit : Hits)
    {
        auto *Enemy = Cast<AEnemyBase>(Hit.GetActor());
        if (IsValid(Enemy) && !Enemy->IsDead() && Enemy->CanReceivePlayerDamage(Source) && !Seen.Contains(Enemy))
        {
            Seen.Add(Enemy);
            Enemies.Add(Enemy);
        }
    }
    Enemies.Sort([Position](const AEnemyBase &A, const AEnemyBase &B) {
        return FVector::DistSquared(A.GetActorLocation(), Position) <
               FVector::DistSquared(B.GetActorLocation(), Position);
    });
    if (Enemies.Num() > 128)
        Enemies.SetNum(128);
    return Enemies;
}
AAbilityAccent *USurvivorAbilityComponent::Accent(FVector Position, FVector End, float Radius, FLinearColor Color,
                                                  float Duration, bool bBeam, const FUpgradePresentation *Settings,
                                                  int32 Stage)
{
    if (GetNetMode() == NM_DedicatedServer)
        return nullptr;
    if (Settings && Settings->MinimumSpawnInterval > 0)
    {
        const FName Key(FString::Printf(TEXT("VFX:%p:%d:%d"), Settings, Stage, bBeam));
        const double Now = GetWorld()->GetTimeSeconds();
        if (const auto *Last = LastVisualSpawn.Find(Key); Last && Now - *Last < Settings->MinimumSpawnInterval)
            return nullptr;
        LastVisualSpawn.Add(Key, Now);
    }
    ActiveAccents.RemoveAll([](const auto &E) { return !E.IsValid(); });
    if (ActiveAccents.Num() >= 64)
        return nullptr;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto *Effect = GetWorld()->SpawnActor<AAbilityAccent>(Position, FRotator::ZeroRotator, Params);
    if (Effect)
    {
        Effect->Initialize(End, Radius, Color, Duration, bBeam, Settings, Stage);
        ActiveAccents.Add(Effect);
    }
    return Effect;
}
void USurvivorAbilityComponent::UpdateAbilities()
{
    if (!Controller || !Upgrades)
        return;
    if (!Controller->IsRunInProgress() || Controller->IsPlayerDead())
    {
        ClearBuildFamilies();
        for (auto E : ActiveAccents)
            if (E.IsValid())
                E->Destroy();
        ActiveAccents.Reset();
        return;
    }
    UpdateBuildFamilies(nullptr);
}
bool USurvivorAbilityComponent::ExecuteSetupAssist(ACharacterBase *Character)
{
    if (!Controller || !Upgrades || !IsValid(Character) || !Controller->IsRunInProgress() ||
        Controller->IsPlayerDead() || Character->GetCharacterMode() != ECharacterMode::Assisting)
        return false;
    auto *Definition = Upgrades->FindUpgradeDefinition(TEXT("TagTeam"));
    const auto AssistTune = [Definition](FName Key, float Default) {
        return Definition ? Definition->GetBalanceValue(Key, Default) : Default;
    };
    const bool bSamurai = Character->IsA<ASamuraiCharacter>();
    if (!bSamurai && !Character->IsA<ANinjaCharacter>())
        return false;
    const auto Source = bSamurai ? EPlayerAttackSource::Samurai : EPlayerAttackSource::Ninja;
    const FVector Origin = Character->GetActorLocation();
    const FVector Forward = Character->GetVisualForwardVector().GetSafeNormal2D();
    int32 Hits = 0;
    auto Targets = FindEnemies(
        Origin, bSamurai ? AssistTune(TEXT("SamuraiRange"), 420) : AssistTune(TEXT("NinjaRange"), 1000), Source);
    PrioritizePreparedTargets(Source, Targets);
    for (auto *Enemy : Targets)
    {
        if (!Controller->IsRunInProgress() || Controller->IsPlayerDead())
            break;
        const FVector Position = Enemy->GetActorLocation();
        const FVector Direction = (Position - Origin).GetSafeNormal2D();
        if (!Direction.IsNearlyZero() &&
            FVector::DotProduct(Forward, Direction) <
                FMath::Cos(FMath::DegreesToRadians(AssistTune(TEXT("ConeHalfAngle"), 69.51268f))))
            continue;
        // Death clears statuses, so snapshot them before the successful assist hit.
        const uint8 StatusBeforeHit = (Enemy->HasStatus(EEnemyStatusEffect::Bleed) ? 1 : 0) |
                                      (Enemy->HasStatus(EEnemyStatusEffect::Poison) ? 2 : 0);
        if (!Enemy->ApplyPlayerDamage(
                (bSamurai ? AssistTune(TEXT("SamuraiDamage"), 12) : AssistTune(TEXT("NinjaDamage"), 10)) *
                    Power(Character),
                Source))
            continue;
        NotifyPartnerHit(Source, Enemy, true, StatusBeforeHit);
        Accent(Origin, Position, 0, bSamurai ? Colors[0] : Colors[3], 0.3f, true,
               Definition ? &Definition->Presentation : nullptr);
        if (!Enemy->IsDead())
        {
            if (bSamurai || Upgrades->HasUpgradeId(TEXT("VenomousKunai")))
                Enemy->GetStatusEffectComponent()->ApplyStatus(
                    bSamurai ? EEnemyStatusEffect::Bleed : EEnemyStatusEffect::Poison, Upgrades, Source, bSamurai);
            if (bSamurai)
            {
                if (Upgrades->HasUpgradeId(TEXT("MarkedBlade")))
                    Enemy->ApplyMark();
                Enemy->ApplyAttackPushback(Origin, Source, AssistTune(TEXT("PushDistance"), 65),
                                           AssistTune(TEXT("PushDuration"), 0.15f));
            }
        }
        // Preserve the existing per-character assist hit limit.
        if (++Hits >= FMath::Clamp(FMath::RoundToInt(bSamurai ? AssistTune(TEXT("SamuraiTargets"), 12)
                                                              : AssistTune(TEXT("NinjaTargets"), 5)),
                                   1, 128))
            break;
    }
    return true;
}
