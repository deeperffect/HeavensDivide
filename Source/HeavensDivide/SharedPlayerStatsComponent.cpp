// Copyright Epic Games, Inc. All Rights Reserved.

#include "SharedPlayerStatsComponent.h"

USharedPlayerStatsComponent::USharedPlayerStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USharedPlayerStatsComponent::AddModifier(const FSharedPlayerStatModifier& Modifier)
{
	if (Modifier.ModifierId.IsNone())
	{
		return;
	}

	RemoveModifier(Modifier.ModifierId);
	Modifiers.Add(Modifier);
	OnStatsChanged.Broadcast();
}

bool USharedPlayerStatsComponent::RemoveModifier(FName ModifierId)
{
	if (ModifierId.IsNone())
	{
		return false;
	}

	const int32 RemovedCount = Modifiers.RemoveAll([ModifierId](const FSharedPlayerStatModifier& Modifier)
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

void USharedPlayerStatsComponent::ClearModifiersFromSource(FName SourceId)
{
	if (SourceId.IsNone())
	{
		return;
	}

	const int32 RemovedCount = Modifiers.RemoveAll([SourceId](const FSharedPlayerStatModifier& Modifier)
	{
		return Modifier.SourceId == SourceId;
	});

	if (RemovedCount > 0)
	{
		OnStatsChanged.Broadcast();
	}
}

float USharedPlayerStatsComponent::GetFinalStat(ESharedPlayerStatType Stat) const
{
	float FlatBonus = 0.0f;
	float AdditivePercentBonus = 0.0f;

	for (const FSharedPlayerStatModifier& Modifier : Modifiers)
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

float USharedPlayerStatsComponent::GetFinalMoveSpeedMultiplier() const
{
	return FMath::Max(0.0f, GetFinalStat(ESharedPlayerStatType::MoveSpeedMultiplier));
}

float USharedPlayerStatsComponent::GetFinalMaxHealthMultiplier() const
{
	return FMath::Max(0.0f, GetFinalStat(ESharedPlayerStatType::MaxHealthMultiplier));
}

float USharedPlayerStatsComponent::GetFinalPickupRadiusMultiplier() const
{
	return FMath::Max(0.0f, GetFinalStat(ESharedPlayerStatType::PickupRadiusMultiplier));
}

float USharedPlayerStatsComponent::GetFinalDamageMultiplier() const
{
	return FMath::Max(0.0f, GetFinalStat(ESharedPlayerStatType::DamageMultiplier));
}

float USharedPlayerStatsComponent::GetFinalAttackSpeedMultiplier() const
{
	return FMath::Max(0.01f, GetFinalStat(ESharedPlayerStatType::AttackSpeedMultiplier));
}

int32 USharedPlayerStatsComponent::GetFinalMaxDashCharges() const
{
	return FMath::Max(1, FMath::RoundToInt(GetFinalStat(ESharedPlayerStatType::MaxDashCharges)));
}

float USharedPlayerStatsComponent::GetBaseStat(ESharedPlayerStatType Stat) const
{
	if (Stat == ESharedPlayerStatType::MaxDashCharges)
	{
		return 1.0f;
	}

	return 1.0f;
}
