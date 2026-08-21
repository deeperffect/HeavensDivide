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
	struct FLevelXPRequirement
	{
		int32 Level;
		int32 XPToNextLevel;
	};

	static constexpr FLevelXPRequirement LevelXPRequirements[] =
	{
		{ 1, 10 },
		{ 2, 16 },
		{ 3, 24 },
		{ 4, 36 },
		{ 5, 50 },
		{ 10, 168 },
		{ 15, 360 },
		{ 20, 628 },
		{ 25, 970 },
		{ 30, 1388 },
		{ 40, 2448 },
		{ 50, 3808 },
	};

	const int32 SafeLevel = FMath::Max(1, CurrentLevel);
	if (SafeLevel <= LevelXPRequirements[0].Level)
	{
		return LevelXPRequirements[0].XPToNextLevel;
	}

	for (int32 Index = 1; Index < UE_ARRAY_COUNT(LevelXPRequirements); ++Index)
	{
		const FLevelXPRequirement& PreviousRequirement = LevelXPRequirements[Index - 1];
		const FLevelXPRequirement& NextRequirement = LevelXPRequirements[Index];
		if (SafeLevel <= NextRequirement.Level)
		{
			const float Alpha = static_cast<float>(SafeLevel - PreviousRequirement.Level) / static_cast<float>(NextRequirement.Level - PreviousRequirement.Level);
			return FMath::Max(1, FMath::RoundToInt(FMath::Lerp(static_cast<float>(PreviousRequirement.XPToNextLevel), static_cast<float>(NextRequirement.XPToNextLevel), Alpha)));
		}
	}

	const FLevelXPRequirement& LastRequirement = LevelXPRequirements[UE_ARRAY_COUNT(LevelXPRequirements) - 1];
	const FLevelXPRequirement& PreviousRequirement = LevelXPRequirements[UE_ARRAY_COUNT(LevelXPRequirements) - 2];
	const float XPPerLevel = static_cast<float>(LastRequirement.XPToNextLevel - PreviousRequirement.XPToNextLevel) / static_cast<float>(LastRequirement.Level - PreviousRequirement.Level);
	return FMath::Max(1, FMath::RoundToInt(LastRequirement.XPToNextLevel + (XPPerLevel * (SafeLevel - LastRequirement.Level))));
}

void UExperienceComponent::BroadcastXPChanged()
{
	OnXPChanged.Broadcast(CurrentXP, CalculateXPToNextLevel(), GetXPPercent());
}
