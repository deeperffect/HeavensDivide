#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "EnemyStatusEffectComponent.h"
#include "HealthComponent.h"
#include "NinjaCharacter.h"
#include "PlayerUpgradeComponent.h"
#include "SamuraiCharacter.h"
#include "SurvivorAbilityComponent.h"
#include "SurvivorPlayerController.h"

namespace
{
EPlayerAttackSource FamilySource(int32 Family)
{
    return FCString::Strcmp(BuildFamilies[Family].Owner, TEXT("Samurai")) == 0 ? EPlayerAttackSource::Samurai
                                                                               : EPlayerAttackSource::Ninja;
}
FLinearColor FamilyColor(int32 Family)
{
    return FLinearColor::MakeFromHSV8(static_cast<uint8>(FamilySource(Family) == EPlayerAttackSource::Samurai
                                                             ? 18 + Family * 5
                                                             : 125 + Family * 6),
                                      190, 255) *
           2.0f;
}
} // namespace
bool USurvivorAbilityComponent::Branch(int32 Family, int32 Index) const
{
    return Upgrades && Upgrades->HasUpgradeId(BuildFamilies[Family].Branches[Index]);
}
float USurvivorAbilityComponent::BuildMagnitude(int32 Family, const TCHAR *Suffix) const
{
    if (!Upgrades)
        return 0;
    const FName Id = Family == 4 && FCString::Strcmp(Suffix, TEXT("Area")) == 0
                         ? FName(TEXT("WideArc"))
                         : FName(FString(BuildFamilies[Family].Id) + Suffix);
    return FMath::Max(0.0f, Upgrades->GetAccumulatedUpgradeMagnitude(Id));
}
void USurvivorAbilityComponent::ClearBuildFamilies()
{
    BuildMarks.Reset();
}
void USurvivorAbilityComponent::UpdateBuildFamilies(ACharacterBase *Character)
{
    for (auto &Mark : BuildMarks)
        Mark.Remaining -= 0.1f;
    BuildMarks.RemoveAll([](const auto &M) { return M.Remaining <= 0 || !M.Enemy.IsValid() || M.Enemy->IsDead(); });
}
void USurvivorAbilityComponent::RegisterFamilyHit(int32 Family, AEnemyBase *Enemy, float Damage)
{
    if (bResolvingReaction || Family < 0 || Family >= BuildFamilyCount || !BuildFamilies[Family].Available ||
        !IsValid(Enemy) || Enemy->IsDead() || !Upgrades || Damage <= 0)
        return;
    const auto Source = FamilySource(Family);
    const float Duration = FMath::Max(0.1f, PreparationDuration) + Upgrades->GetMetaSkillBonus(TEXT("Preparation"));
    for (auto &Mark : BuildMarks)
        if (Mark.Enemy == Enemy)
        {
            // An opposite-source lingering ability must not steal the existing preparation.
            if (Mark.Source == Source || Mark.Remaining <= 0)
            {
                Mark.Source = Source;
                Mark.Damage = Damage;
                Mark.Remaining = Duration;
            }
            return;
        }
    if (BuildMarks.Num() < 512)
    {
        FBuildMark Mark;
        Mark.Enemy = Enemy;
        Mark.Source = Source;
        Mark.Damage = Damage;
        Mark.Remaining = Duration;
        BuildMarks.Add(Mark);
    }
}
bool USurvivorAbilityComponent::HasTriggerablePreparation(EPlayerAttackSource Source, const AEnemyBase *Enemy) const
{
    if (!Upgrades || !IsValid(Enemy) || Enemy->IsDead() || !Enemy->CanReceivePlayerDamage(Source))
        return false;
    if (Source != EPlayerAttackSource::Samurai && Source != EPlayerAttackSource::Ninja)
        return false;
    return BuildMarks.ContainsByPredicate([Source, Enemy](const FBuildMark &Mark) {
        return Mark.Enemy.Get() == Enemy && Mark.Remaining > 0 && Mark.Source != Source;
    });
}
void USurvivorAbilityComponent::PrioritizePreparedTargets(EPlayerAttackSource Source,
                                                          TArray<AEnemyBase *> &Targets) const
{
    if (!Upgrades || (Source != EPlayerAttackSource::Samurai && Source != EPlayerAttackSource::Ninja))
        return;
    TSet<const AEnemyBase *> Prepared;
    for (const auto &Mark : BuildMarks)
    {
        const auto *Enemy = Mark.Enemy.Get();
        if (Mark.Remaining > 0 && Mark.Source != Source && IsValid(Enemy) && !Enemy->IsDead() &&
            Enemy->CanReceivePlayerDamage(Source))
            Prepared.Add(Enemy);
    }
    // Preserve the existing distance order within each priority group.
    Targets.StableSort([&Prepared](const AEnemyBase &Left, const AEnemyBase &Right) {
        return Prepared.Contains(&Left) && !Prepared.Contains(&Right);
    });
}
void USurvivorAbilityComponent::NotifyPartnerHit(EPlayerAttackSource Source, AEnemyBase *Enemy, bool bAssistHit,
                                                 uint8 StatusBeforeHit)
{
    if (!bAssistHit || bResolvingReaction || !Controller || !Upgrades || !IsValid(Enemy) ||
        !Controller->IsRunInProgress() || Controller->IsPlayerDead())
        return;
    if (Source != EPlayerAttackSource::Samurai && Source != EPlayerAttackSource::Ninja)
        return;
    if (!Enemy->IsDead() && !Enemy->CanReceivePlayerDamage(Source))
        return;
    const int32 Index = BuildMarks.IndexOfByPredicate([Enemy, Source](const FBuildMark &Mark) {
        return Mark.Enemy.Get() == Enemy && Mark.Remaining > 0 && Mark.Source != Source;
    });
    if (Index == INDEX_NONE)
        return;
    const FBuildMark Prepared = BuildMarks[Index];
    // Remove before damage/status callbacks, so this preparation can pay out only once.
    BuildMarks.RemoveAtSwap(Index);
    TGuardValue<bool> Guard(bResolvingReaction, true);
    const FVector Position = Enemy->GetActorLocation();
    const bool bBleed =
        StatusBeforeHit == 255 ? Enemy->HasStatus(EEnemyStatusEffect::Bleed) : (StatusBeforeHit & 1) != 0;
    const bool bPoison =
        StatusBeforeHit == 255 ? Enemy->HasStatus(EEnemyStatusEffect::Poison) : (StatusBeforeHit & 2) != 0;
    Enemy->ApplyPlayerDamage(Prepared.Damage * FMath::Max(0.0f, PreparationDamageMultiplier) *
                                 (1.f + Upgrades->GetMetaSkillBonus(TEXT("Reaction"))),
                             Source);
    const float Radius = FMath::Max(0.0f, PreparationSpreadRadius);
    Accent(Position - FVector(0, 0, 70), Position, Radius, FLinearColor(2.0f, 0.3f, 2.0f), 0.3f);
    // Copy only statuses present before the bonus hit, using their original damage source.
    for (const auto Status : {EEnemyStatusEffect::Bleed, EEnemyStatusEffect::Poison})
    {
        if (!(Status == EEnemyStatusEffect::Bleed ? bBleed : bPoison) || Radius <= 0)
            continue;
        const auto StatusSource =
            Status == EEnemyStatusEffect::Bleed ? EPlayerAttackSource::Samurai : EPlayerAttackSource::Ninja;
        for (auto *Target : FindEnemies(Position, Radius, StatusSource))
            if (Target != Enemy)
                Target->GetStatusEffectComponent()->ApplyStatus(Status, Upgrades, StatusSource, true);
    }
}
void USurvivorAbilityComponent::BladeWaveImpact(AEnemyBase *Enemy, float Damage, bool bSplinter)
{
    if (!Controller || !Upgrades)
        return;
    NotifyPartnerHit(EPlayerAttackSource::Samurai, Enemy);
    RegisterFamilyHit(4, Enemy, Damage);
    if (bSplinter && Branch(4, 2))
    {
        const FVector Origin = Enemy->GetActorLocation();
        const float SplinterDamage = Damage * Tuning(4, TEXT("SplinterDamageMultiplier"), 0.3f, 2);
        const float Radius = Tuning(4, TEXT("SplinterRadius"), 120.f, 2) * (1 + BuildMagnitude(4, TEXT("Area")));
        FamilyAccent(4, Origin - FVector(0, 0, 70), Origin, Radius, FamilyColor(4), 0.3f);
        for (auto *Target : FindEnemies(Origin, Radius, EPlayerAttackSource::Samurai))
        {
            if (!IsValid(Target) || Target->IsDead() || !Controller->IsRunInProgress() || Controller->IsPlayerDead())
                continue;
            if (!Target->ApplyPlayerDamage(SplinterDamage, EPlayerAttackSource::Samurai))
                continue;
            FamilyAccent(4, Target->GetActorLocation(), Target->GetActorLocation(), FamilySpec(4).Radius,
                         FamilyColor(4), 0.25f, false, -1, 2);
            NotifyPartnerHit(EPlayerAttackSource::Samurai, Target);
            if (!Target->IsDead())
            {
                ApplyConfiguredStatus(4, Target, EEnemyStatusEffect::Bleed, EPlayerAttackSource::Samurai);
                RegisterFamilyHit(4, Target, SplinterDamage);
            }
        }
    }
}
void USurvivorAbilityComponent::GrantBuildPreview(FString FamilyId, int32 SelectedBranch)
{
#if !UE_BUILD_SHIPPING
    if (!Controller || !Upgrades || !Controller->IsRunInProgress())
        return;
    for (int32 i = 0; i < BuildFamilyCount; ++i)
    {
        const auto &S = BuildFamilies[i];
        if (!S.Available)
            continue;
        if (!FamilyId.Equals(S.Id, ESearchCase::IgnoreCase) && !FamilyId.Equals(TEXT("All"), ESearchCase::IgnoreCase))
            continue;
        auto Grant = [this](const FString &Path, int32 Level) {
            if (auto *U = LoadObject<UUpgradeDefinition>(nullptr, *Path))
                while (Upgrades->GetUpgradeLevel(U) < Level)
                    if (!Upgrades->AcquireUpgrade(U))
                        break;
        };
        const FString Root = FString(TEXT("/Game/HeavensDivide/Upgrades/")) + S.Owner + TEXT("/DA_Upgrade_") + S.Owner;
        Grant(Root + S.Id, 1);
        for (const TCHAR *Suffix : {TEXT("Power"), TEXT("Area"), TEXT("Haste")})
            Grant(Root + (i == 4 && FCString::Strcmp(Suffix, TEXT("Area")) == 0 ? FString(TEXT("WideArc"))
                                                                                : FString(S.Id) + Suffix),
                  2);
        for (int32 b = 0; b < 3; ++b)
            if (SelectedBranch == 0 || SelectedBranch == b + 1)
                Grant(Root + S.Branches[b], 1);
        Controller->ClientMessage(FString::Printf(TEXT("%s build ready: scaling rank 2, selected branches. Universal "
                                                       "Prepare is always available. This run only."),
                                                  S.Title));
    }
#endif
}
