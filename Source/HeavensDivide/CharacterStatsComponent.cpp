// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterStatsComponent.h"

UCharacterStatsComponent::UCharacterStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCharacterStatsComponent::AddModifier(const FCharacterStatModifier& Modifier)
{
	if (Modifier.ModifierId.IsNone())
	{
		return;
	}

	RemoveModifier(Modifier.ModifierId);
	Modifiers.Add(Modifier);
	OnStatsChanged.Broadcast();
}

bool UCharacterStatsComponent::RemoveModifier(FName ModifierId)
{
	if (ModifierId.IsNone())
	{
		return false;
	}

	const int32 RemovedCount = Modifiers.RemoveAll([ModifierId](const FCharacterStatModifier& Modifier)
	{
		return Modifier.ModifierId == ModifierId;
	});

	if (RemovedCount > 0)
	{
		OnStatsChanged.Broadcast();
		return true;
	}

	return false;
}

void UCharacterStatsComponent::ClearModifiersFromSource(FName SourceId)
{
	if (SourceId.IsNone())
	{
		return;
	}

	const int32 RemovedCount = Modifiers.RemoveAll([SourceId](const FCharacterStatModifier& Modifier)
	{
		return Modifier.SourceId == SourceId;
	});

	if (RemovedCount > 0)
	{
		OnStatsChanged.Broadcast();
	}
}

float UCharacterStatsComponent::GetFinalStat(ECharacterStatType Stat) const
{
	float FlatBonus = 0.0f;
	float AdditivePercentBonus = 0.0f;

	for (const FCharacterStatModifier& Modifier : Modifiers)
	{
		if (Modifier.Stat != Stat)
		{
			continue;
		}

		if (Modifier.Operation == EStatModifierOperation::AddFlat)
		{
			FlatBonus += Modifier.Value;
		}
		else
		{
			AdditivePercentBonus += Modifier.Value;
		}
	}

	return (GetBaseStat(Stat) + FlatBonus) * (1.0f + AdditivePercentBonus);
}

int32 UCharacterStatsComponent::GetModifierCount() const
{
	return Modifiers.Num();
}

float UCharacterStatsComponent::GetFinalDamageMultiplier() const
{
	return FMath::Max(0.0f, GetFinalStat(ECharacterStatType::DamageMultiplier));
}

float UCharacterStatsComponent::GetFinalAttackSpeedMultiplier() const
{
	return FMath::Max(0.01f, GetFinalStat(ECharacterStatType::AttackSpeedMultiplier));
}

float UCharacterStatsComponent::GetFinalAttackAreaMultiplier() const
{
	return FMath::Max(0.0f, GetFinalStat(ECharacterStatType::AttackAreaMultiplier));
}

int32 UCharacterStatsComponent::GetFinalProjectileCount() const
{
	return FMath::Max(1, 1 + FMath::RoundToInt(GetFinalStat(ECharacterStatType::ProjectileCountBonus)));
}

float UCharacterStatsComponent::GetFinalProjectileSpeedMultiplier() const
{
	return FMath::Max(0.0f, GetFinalStat(ECharacterStatType::ProjectileSpeedMultiplier));
}

float UCharacterStatsComponent::GetFinalHomingStrengthMultiplier() const
{
	return FMath::Max(0.0f, GetFinalStat(ECharacterStatType::HomingStrengthMultiplier));
}

float UCharacterStatsComponent::GetBaseStat(ECharacterStatType Stat) const
{
	switch (Stat)
	{
	case ECharacterStatType::ProjectileCountBonus:
		return 0.0f;
	case ECharacterStatType::DamageMultiplier:
	case ECharacterStatType::AttackSpeedMultiplier:
	case ECharacterStatType::AttackAreaMultiplier:
	case ECharacterStatType::ProjectileSpeedMultiplier:
	case ECharacterStatType::HomingStrengthMultiplier:
	default:
		return 1.0f;
	}
}
