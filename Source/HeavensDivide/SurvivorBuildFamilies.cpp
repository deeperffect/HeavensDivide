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
void USurvivorAbilityComponent::BladeWaveImpact(AEnemyBase *Enemy, float Damage, bool bSplinter)
{
    if (!Controller || !Upgrades)
        return;
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
            if (!Target->IsDead())
            {
                ApplyConfiguredStatus(4, Target, EEnemyStatusEffect::Bleed, EPlayerAttackSource::Samurai);
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
        Controller->ClientMessage(FString::Printf(TEXT("%s build ready: scaling rank 2, selected branches. This run only."),
                                                  S.Title));
    }
#endif
}
