// Copyright Epic Games, Inc. All Rights Reserved.

#include "ExperienceComponent.h"

UExperienceComponent::UExperienceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UExperienceComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentXP = FMath::Max(0, CurrentXP);
	CurrentLevel = FMath::Max(1, CurrentLevel);
	BaseXPRequirement = FMath::Max(1, BaseXPRequirement);
	XPRequirementGrowth = FMath::Max(0, XPRequirementGrowth);
	BroadcastXPChanged();
}

void UExperienceComponent::AddXP(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	CurrentXP += Amount;

	int32 XPToNextLevel = CalculateXPToNextLevel();
	while (CurrentXP >= XPToNextLevel)
	{
		CurrentXP -= XPToNextLevel;
		++CurrentLevel;
		UE_LOG(LogTemp, Log, TEXT("LEVEL UP: New Level = %d"), CurrentLevel);
		OnLevelUp.Broadcast(CurrentLevel);
		XPToNextLevel = CalculateXPToNextLevel();
	}

	UE_LOG(LogTemp, Log, TEXT("Player XP: %d / %d"), CurrentXP, XPToNextLevel);
	BroadcastXPChanged();
}

int32 UExperienceComponent::GetCurrentXP() const
{
	return CurrentXP;
}

int32 UExperienceComponent::GetCurrentLevel() const
{
	return CurrentLevel;
}

int32 UExperienceComponent::GetXPToNextLevel() const
{
	return CalculateXPToNextLevel();
}

float UExperienceComponent::GetXPPercent() const
{
	const int32 XPToNextLevel = CalculateXPToNextLevel();
	return XPToNextLevel > 0 ? static_cast<float>(CurrentXP) / static_cast<float>(XPToNextLevel) : 0.0f;
}

int32 UExperienceComponent::CalculateXPToNextLevel() const
{
	return FMath::Max(1, BaseXPRequirement + ((FMath::Max(1, CurrentLevel) - 1) * XPRequirementGrowth));
}

void UExperienceComponent::BroadcastXPChanged()
{
	OnXPChanged.Broadcast(CurrentXP, CalculateXPToNextLevel(), GetXPPercent());
}
