// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerUpgradeComponent.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "CharacterStatsComponent.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "SharedPlayerStatsComponent.h"
#include "SurvivorPlayerController.h"
#include "UpgradeDefinition.h"

UPlayerUpgradeComponent::UPlayerUpgradeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UPlayerUpgradeComponent::GetUpgradeLevel(UUpgradeDefinition* Upgrade) const
{
	if (!IsValidUpgradeDefinition(Upgrade))
	{
		return 0;
	}

	if (const int32* FoundLevel = UpgradeLevels.Find(Upgrade->UpgradeId))
	{
		return *FoundLevel;
	}

	return 0;
}

bool UPlayerUpgradeComponent::CanAcquireUpgrade(UUpgradeDefinition* Upgrade) const
{
	return IsValidUpgradeDefinition(Upgrade) && GetUpgradeLevel(Upgrade) < Upgrade->MaxLevel;
}

bool UPlayerUpgradeComponent::AcquireUpgrade(UUpgradeDefinition* Upgrade)
{
	if (!CanAcquireUpgrade(Upgrade))
	{
		return false;
	}

	const int32 NewLevel = GetUpgradeLevel(Upgrade) + 1;
	UpgradeLevels.FindOrAdd(Upgrade->UpgradeId) = NewLevel;
	AcquiredUpgradeDefinitions.FindOrAdd(Upgrade->UpgradeId) = Upgrade;

	RebuildUpgradeModifiers(Upgrade, NewLevel);

	OnUpgradeAcquired.Broadcast(Upgrade, NewLevel);
	OnUpgradeLevelChanged.Broadcast(Upgrade, NewLevel);
	return true;
}

void UPlayerUpgradeComponent::RebuildAllUpgradeModifiers()
{
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
}

bool UPlayerUpgradeComponent::DebugAcquireUpgrade(UUpgradeDefinition* Upgrade)
{
	return AcquireUpgrade(Upgrade);
}

TArray<UUpgradeDefinition*> UPlayerUpgradeComponent::GetEligibleUpgrades(const TArray<UUpgradeDefinition*>& CandidateUpgrades) const
{
	TArray<UUpgradeDefinition*> EligibleUpgrades;
	for (UUpgradeDefinition* Upgrade : CandidateUpgrades)
	{
		if (CanAcquireUpgrade(Upgrade))
		{
			EligibleUpgrades.Add(Upgrade);
		}
	}

	return EligibleUpgrades;
}

bool UPlayerUpgradeComponent::IsValidUpgradeDefinition(const UUpgradeDefinition* Upgrade) const
{
	return Upgrade && !Upgrade->UpgradeId.IsNone() && Upgrade->MaxLevel > 0;
}

void UPlayerUpgradeComponent::RebuildUpgradeModifiers(UUpgradeDefinition* Upgrade, int32 NewLevel)
{
	if (!IsValidUpgradeDefinition(Upgrade))
	{
		return;
	}

	ClearUpgradeModifiers(Upgrade);

	ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(GetOwner());
	UCharacterManagerComponent* CharacterManager = SurvivorController ? SurvivorController->GetCharacterManager() : nullptr;
	USharedPlayerStatsComponent* SharedStats = SurvivorController ? SurvivorController->GetSharedPlayerStats() : nullptr;
	const FName SourceId = MakeUpgradeModifierSourceId(Upgrade);

	UE_LOG(LogTemp, Log, TEXT("RebuildUpgradeModifiers: Upgrade=%s Level=%d Modifiers=%d Controller=%s CharacterManager=%s SharedStats=%s Samurai=%s Ninja=%s"),
		*Upgrade->UpgradeId.ToString(),
		NewLevel,
		Upgrade->StatModifiers.Num(),
		*GetNameSafe(SurvivorController),
		*GetNameSafe(CharacterManager),
		*GetNameSafe(SharedStats),
		*GetNameSafe(CharacterManager ? CharacterManager->GetSamurai() : nullptr),
		*GetNameSafe(CharacterManager ? CharacterManager->GetNinja() : nullptr));

	for (int32 Index = 0; Index < Upgrade->StatModifiers.Num(); ++Index)
	{
		const FUpgradeStatModifierDefinition& ModifierDefinition = Upgrade->StatModifiers[Index];
		const float ModifierValue = ModifierDefinition.ValuePerLevel * NewLevel;

		if (ModifierDefinition.Target == EUpgradeStatTarget::SharedPlayer)
		{
			if (!SharedStats)
			{
				UE_LOG(LogTemp, Warning, TEXT("Upgrade modifier skipped: Upgrade=%s Target=SharedPlayer SharedStats missing"),
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
			UE_LOG(LogTemp, Log, TEXT("Upgrade modifier applied: Upgrade=%s Target=SharedPlayer Stat=%d Operation=%d Value=%.3f"),
				*Upgrade->UpgradeId.ToString(),
				static_cast<int32>(Modifier.Stat),
				static_cast<int32>(Modifier.Operation),
				Modifier.Value);
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
			UE_LOG(LogTemp, Warning, TEXT("Upgrade modifier skipped: Upgrade=%s Target=%d Character=%s CharacterStats missing"),
				*Upgrade->UpgradeId.ToString(),
				static_cast<int32>(ModifierDefinition.Target),
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
		UE_LOG(LogTemp, Log, TEXT("Upgrade modifier applied: Upgrade=%s Target=%d Character=%s Stat=%d Operation=%d Value=%.3f FinalStat=%.3f ModifierCount=%d"),
			*Upgrade->UpgradeId.ToString(),
			static_cast<int32>(ModifierDefinition.Target),
			*GetNameSafe(TargetCharacter),
			static_cast<int32>(Modifier.Stat),
			static_cast<int32>(Modifier.Operation),
			Modifier.Value,
			CharacterStats->GetFinalStat(Modifier.Stat),
			CharacterStats->GetModifierCount());
	}
}

void UPlayerUpgradeComponent::ClearUpgradeModifiers(UUpgradeDefinition* Upgrade)
{
	if (!IsValidUpgradeDefinition(Upgrade))
	{
		return;
	}

	ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(GetOwner());
	UCharacterManagerComponent* CharacterManager = SurvivorController ? SurvivorController->GetCharacterManager() : nullptr;
	const FName SourceId = MakeUpgradeModifierSourceId(Upgrade);

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

	if (USharedPlayerStatsComponent* SharedStats = SurvivorController ? SurvivorController->GetSharedPlayerStats() : nullptr)
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
