#include "PlayerUpgradeComponent.h"
#include "AutoAttackComponent.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "CharacterStatsComponent.h"
#include "HealthComponent.h"
#include "HAL/IConsoleManager.h"
#include "MetaSkillTree.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "SharedPlayerStatsComponent.h"
#include "SurvivorPlayerController.h"
#include "SynergyMetaProgressionSubsystem.h"

static TAutoConsoleVariable<int32> CVarHDLogPlayerUpgradeStats(
    TEXT("hd.LogPlayerUpgradeStats"), 0,
    TEXT("Logs player upgrade/stat summary after upgrade stat rebuilds when enabled."));

void UPlayerUpgradeComponent::RebuildAllUpgradeModifiers()
{
	ApplyMetaSkillModifiers();
	UE_LOG(LogTemp, Log, TEXT("RebuildAllUpgradeModifiers: UpgradeCount=%d"), AcquiredUpgradeDefinitions.Num());

	for (const TPair<FName, TObjectPtr<UUpgradeDefinition>>& UpgradePair : AcquiredUpgradeDefinitions)
	{
		UUpgradeDefinition* Upgrade = UpgradePair.Value;
		if (!IsValidUpgradeDefinition(Upgrade))
		{
			continue;
		}

		const int32 Level = GetUpgradeLevel(Upgrade);
		if (Level > 0)
		{
			RebuildUpgradeModifiers(Upgrade, Level);
		}
	}

	LogPlayerUpgradeStats();
}

void UPlayerUpgradeComponent::LogPlayerUpgradeStats() const
{
	if (CVarHDLogPlayerUpgradeStats.GetValueOnGameThread() == 0)
	{
		return;
	}

	const ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(GetOwner());
	const UCharacterManagerComponent* CharacterManager =
	    SurvivorController ? SurvivorController->GetCharacterManager() : nullptr;
	const USharedPlayerStatsComponent* SharedStats =
	    SurvivorController ? SurvivorController->GetSharedPlayerStats() : nullptr;
	const UHealthComponent* PlayerHealth =
	    SurvivorController ? SurvivorController->GetPlayerHealthComponent() : nullptr;
	const ACharacterBase* Samurai = CharacterManager ? CharacterManager->GetSamurai() : nullptr;
	const ACharacterBase* Ninja = CharacterManager ? CharacterManager->GetNinja() : nullptr;
	const UAutoAttackComponent* SamuraiAttack =
	    Samurai ? Samurai->FindComponentByClass<UAutoAttackComponent>() : nullptr;
	const UAutoAttackComponent* NinjaAttack = Ninja ? Ninja->FindComponentByClass<UAutoAttackComponent>() : nullptr;
	const UCharacterStatsComponent* SamuraiStats = Samurai ? Samurai->GetCharacterStats() : nullptr;
	const UCharacterStatsComponent* NinjaStats = Ninja ? Ninja->GetCharacterStats() : nullptr;

	UE_LOG(LogTemp, Log, TEXT("=== PLAYER UPGRADE STATS ==="));
	UE_LOG(LogTemp, Log, TEXT("SHARED"));
	UE_LOG(LogTemp, Log, TEXT("Move Speed Multiplier: %.3f"),
	       SharedStats ? SharedStats->GetFinalMoveSpeedMultiplier() : 1.0f);
	UE_LOG(LogTemp, Log, TEXT("Max Health: Current %.2f / Final %.2f"),
	       PlayerHealth ? PlayerHealth->GetCurrentHealth() : 0.0f, PlayerHealth ? PlayerHealth->GetMaxHealth() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("Pickup Radius Multiplier: %.3f"),
	       SharedStats ? SharedStats->GetFinalPickupRadiusMultiplier() : 1.0f);
	UE_LOG(LogTemp, Log, TEXT("Global Damage Multiplier: %.3f"),
	       SharedStats ? SharedStats->GetFinalDamageMultiplier() : 1.0f);
	UE_LOG(LogTemp, Log, TEXT("SAMURAI"));
	UE_LOG(LogTemp, Log, TEXT("Damage: Base %.2f -> Final %.2f"),
	       SamuraiAttack ? SamuraiAttack->GetBaseAttackDamage() : 0.0f,
	       SamuraiAttack ? SamuraiAttack->GetEffectiveAttackDamage() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("Attack Interval: Base %.3f -> Final %.3f"),
	       SamuraiAttack ? SamuraiAttack->GetBaseAttackInterval() : 0.0f,
	       SamuraiAttack ? SamuraiAttack->GetEffectiveAttackInterval() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("Area Radius: Base %.2f -> Final %.2f"),
	       SamuraiAttack ? SamuraiAttack->GetBaseAttackRadius() : 0.0f,
	       SamuraiAttack ? SamuraiAttack->GetEffectiveAttackRadius() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("NINJA"));
	UE_LOG(LogTemp, Log, TEXT("Damage: Base %.2f -> Final %.2f"),
	       NinjaAttack ? NinjaAttack->GetBaseAttackDamage() : 0.0f,
	       NinjaAttack ? NinjaAttack->GetEffectiveAttackDamage() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("Attack Interval: Base %.3f -> Final %.3f"),
	       NinjaAttack ? NinjaAttack->GetBaseAttackInterval() : 0.0f,
	       NinjaAttack ? NinjaAttack->GetEffectiveAttackInterval() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("Projectile Count: Base 1 -> Final %d"),
	       NinjaAttack ? NinjaAttack->GetEffectiveProjectileCount() : 1);
	UE_LOG(LogTemp, Log, TEXT("Projectile Pierce Bonus: %d"),
	       NinjaAttack ? NinjaAttack->GetEffectiveProjectilePierceBonus() : 0);
	UE_LOG(LogTemp, Log, TEXT("Projectile Speed: Base %.2f -> Final %.2f"),
	       NinjaAttack ? NinjaAttack->GetBaseProjectileSpeed() : 0.0f,
	       NinjaAttack ? NinjaAttack->GetEffectiveProjectileSpeed() : 0.0f);
	UE_LOG(LogTemp, Log, TEXT("============================"));
}

int32 UPlayerUpgradeComponent::GetMasteryPoints(EUpgradeInvestmentOwner InvestmentOwner) const
{
	switch (InvestmentOwner)
	{
	case EUpgradeInvestmentOwner::Samurai:
		return SamuraiMasteryPoints;
	case EUpgradeInvestmentOwner::Ninja:
		return NinjaMasteryPoints;
	default:
		return 0;
	}
}

float UPlayerUpgradeComponent::GetSamuraiPowerMultiplier() const
{
	// Mastery tracks investment/unlocks only; damage is explicit on cards and permanent skills.
	return 1.0f;
}

float UPlayerUpgradeComponent::GetNinjaPowerMultiplier() const
{
	// Mastery tracks investment/unlocks only; damage is explicit on cards and permanent skills.
	return 1.0f;
}

void UPlayerUpgradeComponent::RebuildUpgradeModifiers(UUpgradeDefinition* Upgrade, int32 NewLevel)
{
	if (!IsValidUpgradeDefinition(Upgrade))
	{
		return;
	}

	ClearUpgradeModifiers(Upgrade);

	ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(GetOwner());
	UCharacterManagerComponent* CharacterManager =
	    SurvivorController ? SurvivorController->GetCharacterManager() : nullptr;
	USharedPlayerStatsComponent* SharedStats =
	    SurvivorController ? SurvivorController->GetSharedPlayerStats() : nullptr;
	const FName SourceId = MakeUpgradeModifierSourceId(Upgrade);

	UE_LOG(LogTemp, Log,
	       TEXT("RebuildUpgradeModifiers: Upgrade=%s Level=%d Modifiers=%d Controller=%s CharacterManager=%s "
	            "SharedStats=%s Samurai=%s Ninja=%s"),
	       *Upgrade->UpgradeId.ToString(), NewLevel, Upgrade->StatModifiers.Num(), *GetNameSafe(SurvivorController),
	       *GetNameSafe(CharacterManager), *GetNameSafe(SharedStats),
	       *GetNameSafe(CharacterManager ? CharacterManager->GetSamurai() : nullptr),
	       *GetNameSafe(CharacterManager ? CharacterManager->GetNinja() : nullptr));

	for (int32 Index = 0; Index < Upgrade->StatModifiers.Num(); ++Index)
	{
		const FUpgradeStatModifierDefinition& ModifierDefinition = Upgrade->StatModifiers[Index];
		const float* StoredMagnitude = AccumulatedUpgradeMagnitudes.Find(Upgrade->UpgradeId);
		const float ModifierValue = Upgrade->bUsesRolledRarity && StoredMagnitude
		                                ? *StoredMagnitude
		                                : ModifierDefinition.ValuePerLevel * NewLevel;

		if (ModifierDefinition.Target == EUpgradeStatTarget::SharedPlayer)
		{
			if (!SharedStats)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("Upgrade modifier skipped: Upgrade=%s Target=SharedPlayer SharedStats missing"),
				       *Upgrade->UpgradeId.ToString());
				continue;
			}

			FSharedPlayerStatModifier Modifier;
			Modifier.ModifierId = MakeUpgradeModifierId(Upgrade, Index);
			Modifier.SourceId = SourceId;
			Modifier.Stat = ModifierDefinition.SharedPlayerStat;
			Modifier.Operation = ModifierDefinition.Operation;
			Modifier.Value = ModifierValue;
			SharedStats->AddModifier(Modifier);
			UE_LOG(LogTemp, Log,
			       TEXT("Upgrade modifier applied: Upgrade=%s Target=SharedPlayer Stat=%d Operation=%d Value=%.3f"),
			       *Upgrade->UpgradeId.ToString(), static_cast<int32>(Modifier.Stat),
			       static_cast<int32>(Modifier.Operation), Modifier.Value);
			continue;
		}

		ACharacterBase* TargetCharacter = nullptr;
		if (CharacterManager)
		{
			TargetCharacter = ModifierDefinition.Target == EUpgradeStatTarget::Samurai
			                      ? Cast<ACharacterBase>(CharacterManager->GetSamurai())
			                      : Cast<ACharacterBase>(CharacterManager->GetNinja());
		}

		UCharacterStatsComponent* CharacterStats = TargetCharacter ? TargetCharacter->GetCharacterStats() : nullptr;
		if (!CharacterStats)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("Upgrade modifier skipped: Upgrade=%s Target=%d Character=%s CharacterStats missing"),
			       *Upgrade->UpgradeId.ToString(), static_cast<int32>(ModifierDefinition.Target),
			       *GetNameSafe(TargetCharacter));
			continue;
		}

		FCharacterStatModifier Modifier;
		Modifier.ModifierId = MakeUpgradeModifierId(Upgrade, Index);
		Modifier.SourceId = SourceId;
		Modifier.Stat = ModifierDefinition.CharacterStat;
		Modifier.Operation = ModifierDefinition.Operation;
		Modifier.Value = ModifierValue;
		CharacterStats->AddModifier(Modifier);
		UE_LOG(LogTemp, Log,
		       TEXT("Upgrade modifier applied: Upgrade=%s Target=%d Character=%s Stat=%d Operation=%d Value=%.3f "
		            "FinalStat=%.3f ModifierCount=%d"),
		       *Upgrade->UpgradeId.ToString(), static_cast<int32>(ModifierDefinition.Target),
		       *GetNameSafe(TargetCharacter), static_cast<int32>(Modifier.Stat), static_cast<int32>(Modifier.Operation),
		       Modifier.Value, CharacterStats->GetFinalStat(Modifier.Stat), CharacterStats->GetModifierCount());
	}
}

void UPlayerUpgradeComponent::ClearUpgradeModifiers(UUpgradeDefinition* Upgrade)
{
	if (!Upgrade || Upgrade->UpgradeId.IsNone())
	{
		return;
	}

	ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(GetOwner());
	UCharacterManagerComponent* CharacterManager =
	    SurvivorController ? SurvivorController->GetCharacterManager() : nullptr;
	const FName SourceId = Upgrade->UpgradeId;

	if (CharacterManager)
	{
		if (ASamuraiCharacter* Samurai = CharacterManager->GetSamurai())
		{
			if (UCharacterStatsComponent* CharacterStats = Samurai->GetCharacterStats())
			{
				CharacterStats->ClearModifiersFromSource(SourceId);
			}
		}

		if (ANinjaCharacter* Ninja = CharacterManager->GetNinja())
		{
			if (UCharacterStatsComponent* CharacterStats = Ninja->GetCharacterStats())
			{
				CharacterStats->ClearModifiersFromSource(SourceId);
			}
		}
	}

	if (USharedPlayerStatsComponent* SharedStats =
	        SurvivorController ? SurvivorController->GetSharedPlayerStats() : nullptr)
	{
		SharedStats->ClearModifiersFromSource(SourceId);
	}
}

FName UPlayerUpgradeComponent::MakeUpgradeModifierSourceId(const UUpgradeDefinition* Upgrade) const
{
	return IsValidUpgradeDefinition(Upgrade) ? Upgrade->UpgradeId : NAME_None;
}

FName UPlayerUpgradeComponent::MakeUpgradeModifierId(const UUpgradeDefinition* Upgrade, int32 ModifierIndex) const
{
	if (!IsValidUpgradeDefinition(Upgrade))
	{
		return NAME_None;
	}

	return FName(*FString::Printf(TEXT("%s.Modifier.%d"), *Upgrade->UpgradeId.ToString(), ModifierIndex));
}

float UPlayerUpgradeComponent::GetMetaSkillBonus(FName Effect) const
{
    const auto* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const auto* Meta = GI ? GI->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
    auto OwnsStance = [&](FName Stance)
    {
        return HasUpgradeId(Stance) && (BalancePreviewMetaProfile >= 0 || (Meta && Meta->IsStanceUnlocked(Stance)));
    };
    // Read unconverted effects directly: conversions must never call each other recursively.
    auto RawBonus = [&](FName RawEffect)
    {
        float Bonus = 0.f;
        if (BalancePreviewMetaProfile >= 0)
        {
            if (BalancePreviewMetaProfile > 0)
                for (const auto& Node : MetaSkillTree::Nodes())
                    if (Node.Effect == RawEffect && (Node.RequiredStance.IsNone() || OwnsStance(Node.RequiredStance)))
                        Bonus += Node.MaxRank * Node.PerRank;
            return Bonus;
        }
        if (!Meta) return 0.f;
        Bonus = Meta->GetSkillBonus(RawEffect);
        // Each hero owns a stance, including the off-screen Tag Team assistant.
        for (FName Stance : {FName(TEXT("BattleStance")),FName(TEXT("Iaijutsu")),FName(TEXT("BladeWave")),
             FName(TEXT("ReturningFang")),FName(TEXT("BarrageStance")),FName(TEXT("GreatShuriken"))})
            if (OwnsStance(Stance)) Bonus += Meta->GetStanceSkillBonus(RawEffect,Stance);
        return Bonus;
    };
    if (Effect == TEXT("IaijutsuCharge"))
        return OwnsStance(TEXT("Iaijutsu")) ? RawBonus(Effect)+RawBonus(TEXT("Attack"))+RawBonus(TEXT("SteelAttack")) : 0.f;
    if (Effect == TEXT("FangRange"))
        return OwnsStance(TEXT("ReturningFang")) ? RawBonus(Effect)+RawBonus(TEXT("Area")) : 0.f;
    if (Effect == TEXT("ShurikenSize"))
        return OwnsStance(TEXT("GreatShuriken")) ? RawBonus(Effect)+RawBonus(TEXT("Area")) : 0.f;
    if (Effect == TEXT("ShurikenTempo"))
        return OwnsStance(TEXT("GreatShuriken")) ? RawBonus(Effect)+RawBonus(TEXT("Attack"))+RawBonus(TEXT("ShadowAttack")) : 0.f;
    if (Effect == TEXT("BarrageArea"))
        return OwnsStance(TEXT("BarrageStance")) ? RawBonus(TEXT("Area")) : 0.f;
    return RawBonus(Effect);
}

void UPlayerUpgradeComponent::ApplyMetaSkillModifiers()
{
	auto* PC = Cast<ASurvivorPlayerController>(GetOwner());
	auto* Party = PC ? PC->GetCharacterManager() : nullptr;
	auto* Shared = PC ? PC->GetSharedPlayerStats() : nullptr;
	const auto* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const auto* Meta = GI ? GI->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
	if (!PC || !Party)
		return;
	for (const FMetaSkillNode& N : MetaSkillTree::Nodes())
	{
		const FName Source(*FString::Printf(TEXT("MetaSkill.%s"), *N.Id.ToString()));
        if (N.Effect == TEXT("Attack") || N.Effect == TEXT("SteelAttack") || N.Effect == TEXT("ShadowAttack") || N.Effect == TEXT("Area"))
        {
            // Clear pre-conversion sources as well, including live balance-editor rebuilds.
            if (Shared) Shared->ClearModifiersFromSource(Source);
            if (auto* Hero=Party->GetSamurai()) Hero->GetCharacterStats()->ClearModifiersFromSource(Source);
            if (auto* Hero=Party->GetNinja()) Hero->GetCharacterStats()->ClearModifiersFromSource(Source);
            continue;
        }
		if (N.bPassiveEffect || N.Effect == TEXT("Swap") || N.Effect == TEXT("Bleed") || N.Effect == TEXT("Poison") ||
		    N.Effect == TEXT("Reroll") || N.Effect == TEXT("Banish"))
			continue;
		const bool bApplies=N.RequiredStance.IsNone() || (HasUpgradeId(N.RequiredStance) &&
            (BalancePreviewMetaProfile >= 0 || (Meta && Meta->IsStanceUnlocked(N.RequiredStance))));
		const float Value = !bApplies ? 0.f : BalancePreviewMetaProfile >= 0
            ? (BalancePreviewMetaProfile > 0 ? N.MaxRank * N.PerRank : 0.f)
            : (Meta ? Meta->GetSkillRank(N.Id) * N.PerRank : 0.f);
		if (N.Target == EUpgradeStatTarget::SharedPlayer)
		{
			if (!Shared)
				continue;
			// AddModifier replaces by ID: rebuilds neither stack nor briefly remove health.
			FSharedPlayerStatModifier M;
			M.SourceId = M.ModifierId = Source;
			M.Stat = N.SharedStat;
			M.Value = Value;
			Shared->AddModifier(M);
		}
		else
		{
			ACharacterBase* Character = N.Target == EUpgradeStatTarget::Samurai
			                                ? static_cast<ACharacterBase*>(Party->GetSamurai())
			                                : static_cast<ACharacterBase*>(Party->GetNinja());
			auto* Stats = Character ? Character->GetCharacterStats() : nullptr;
			if (!Stats)
				continue;
			FCharacterStatModifier M;
			M.SourceId = M.ModifierId = Source;
			M.Stat = N.CharacterStat;
			M.Value = Value;
			Stats->AddModifier(M);
		}
	}
    auto SetConverted = [](ACharacterBase* Character, const TCHAR* Source, ECharacterStatType Stat, float Value)
    {
        if (auto* Stats = Character ? Character->GetCharacterStats() : nullptr)
        {
            FCharacterStatModifier M;
            M.SourceId = M.ModifierId = Source;
            M.Stat = Stat; M.Value = Value;
            Stats->AddModifier(M);
        }
    };
    auto OwnsStance = [&](FName Stance)
    {
        return HasUpgradeId(Stance) && (BalancePreviewMetaProfile >= 0 || (Meta && Meta->IsStanceUnlocked(Stance)));
    };
    const float SharedCadence = GetMetaSkillBonus(TEXT("Attack"));
    const float SteelCadence = SharedCadence + GetMetaSkillBonus(TEXT("SteelAttack"));
    const float ShadowCadence = SharedCadence + GetMetaSkillBonus(TEXT("ShadowAttack"));
    SetConverted(Party->GetSamurai(),TEXT("MetaSkill.ConvertedSteelCadence"),ECharacterStatType::AttackSpeedMultiplier,
        OwnsStance(TEXT("BattleStance")) || OwnsStance(TEXT("BladeWave")) ? SteelCadence : 0.f);
    SetConverted(Party->GetNinja(),TEXT("MetaSkill.ConvertedShadowCadence"),ECharacterStatType::AttackSpeedMultiplier,
        OwnsStance(TEXT("BarrageStance")) ? ShadowCadence : 0.f);
    SetConverted(Party->GetNinja(),TEXT("MetaSkill.ConvertedFangCadence"),ECharacterStatType::ProjectileSpeedMultiplier,
        OwnsStance(TEXT("ReturningFang")) ? ShadowCadence : 0.f);
    SetConverted(Party->GetSamurai(),TEXT("MetaSkill.ConvertedSteelArea"),ECharacterStatType::AttackAreaMultiplier,
        GetMetaSkillBonus(TEXT("Area")));

}
