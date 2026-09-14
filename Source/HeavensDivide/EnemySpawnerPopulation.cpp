#include "EnemySpawner.h"

#include "EnemyBase.h"
#include "Curves/CurveFloat.h"

const FEnemyPressurePhase* AEnemySpawner::ResolveActivePressurePhase(int32* OutPhaseIndex, bool bLogWarnings) const
{
	if (OutPhaseIndex)
	{
		*OutPhaseIndex = INDEX_NONE;
	}

	const FEnemyPressurePhase* ResolvedPhase = nullptr;
	int32 ResolvedIndex = INDEX_NONE;
	int32 MatchCount = 0;
	for (int32 Index = 0; Index < PressurePhases.Num(); ++Index)
	{
		const FEnemyPressurePhase& Phase = PressurePhases[Index];
		const bool bValidRange = Phase.StartTimeSeconds >= 0.0f
			&& (Phase.EndTimeSeconds <= 0.0f || Phase.EndTimeSeconds > Phase.StartTimeSeconds);
		const bool bContainsTime = bValidRange
			&& RunTimeSeconds >= Phase.StartTimeSeconds
			&& (Phase.EndTimeSeconds <= 0.0f || RunTimeSeconds < Phase.EndTimeSeconds);
		if (bContainsTime)
		{
			++MatchCount;
			if (!ResolvedPhase)
			{
				ResolvedPhase = &Phase;
				ResolvedIndex = Index;
			}
		}
	}

	if (MatchCount == 1)
	{
		if (OutPhaseIndex)
		{
			*OutPhaseIndex = ResolvedIndex;
		}
		return ResolvedPhase;
	}

	const int32 CurrentSecond = FMath::FloorToInt(RunTimeSeconds);
	if (bLogWarnings && LastInvalidPhaseWarningSecond != CurrentSecond)
	{
		LastInvalidPhaseWarningSecond = CurrentSecond;
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner %s found %d pressure phases at RunTime=%.2f. Expected exactly one; normal spawning is paused safely."),
			*GetNameSafe(this), MatchCount, RunTimeSeconds);
	}
	return nullptr;
}

const FEnemySpawnEntry* AEnemySpawner::FindSpawnDefinition(TSubclassOf<AEnemyBase> EnemyClass) const
{
	for (const FEnemySpawnEntry& Entry : EnemySpawnEntries)
	{
		if (Entry.EnemyClass == EnemyClass)
		{
			return &Entry;
		}
	}
	return nullptr;
}

const FEnemyPopulationPhaseEntry* AEnemySpawner::ChoosePopulationDeficitEntry(const FEnemyPressurePhase& Phase) const
{
	float TotalPriority = 0.0f;
	for (const FEnemyPopulationPhaseEntry& Entry : Phase.EnemyPopulationEntries)
	{
		if (IsPopulationEntryEligible(Phase, Entry))
		{
			const int32 Deficit = Entry.DesiredPopulation - GetAliveCountForSpawnClass(Entry.EnemyClass);
			TotalPriority += static_cast<float>(Deficit) * Entry.RefillPriority;
		}
	}

	if (TotalPriority <= 0.0f)
	{
		return nullptr;
	}

	float Roll = FMath::FRandRange(0.0f, TotalPriority);
	for (const FEnemyPopulationPhaseEntry& Entry : Phase.EnemyPopulationEntries)
	{
		if (!IsPopulationEntryEligible(Phase, Entry))
		{
			continue;
		}
		Roll -= static_cast<float>(Entry.DesiredPopulation - GetAliveCountForSpawnClass(Entry.EnemyClass)) * Entry.RefillPriority;
		if (Roll <= 0.0f)
		{
			return &Entry;
		}
	}
	return nullptr;
}

const FEnemyPopulationPhaseEntry* AEnemySpawner::ChooseTimedThreatEntry(const FEnemyPressurePhase& Phase) const
{
	float TotalPriority = 0.0f;
	for (const FEnemyPopulationPhaseEntry& Entry : Phase.EnemyPopulationEntries)
	{
		if (IsTimedThreatEntryEligible(Phase, Entry))
		{
			TotalPriority += FMath::Max(0.0f, Entry.RefillPriority);
		}
	}

	if (TotalPriority <= 0.0f)
	{
		return nullptr;
	}

	float Roll = FMath::FRandRange(0.0f, TotalPriority);
	for (const FEnemyPopulationPhaseEntry& Entry : Phase.EnemyPopulationEntries)
	{
		if (!IsTimedThreatEntryEligible(Phase, Entry))
		{
			continue;
		}
		Roll -= FMath::Max(0.0f, Entry.RefillPriority);
		if (Roll <= 0.0f)
		{
			return &Entry;
		}
	}
	return nullptr;
}

const FEnemyPopulationPhaseEntry* AEnemySpawner::ChooseDirectorEntry(const FEnemyPressurePhase& Phase) const
{
	const FEnemyPopulationPhaseEntry* MaintainedEntry = ChoosePopulationDeficitEntry(Phase);
	const FEnemyPopulationPhaseEntry* ThreatEntry = ChooseTimedThreatEntry(Phase);
	if (ThreatEntry && (!MaintainedEntry || FMath::FRand() < FMath::Clamp(TimedThreatSpawnSlotChance, 0.0f, 1.0f)))
	{
		return ThreatEntry;
	}
	return MaintainedEntry;
}

float AEnemySpawner::GetHealthMultiplier(const FEnemySpawnEntry& SpawnEntry) const
{
	return 1.0f + FMath::Max(0.0f, SpawnEntry.HealthScalingPerMinute) * FMath::Max(0.0f, GetRunTimeMinutes());
}

float AEnemySpawner::GetDamageMultiplier() const
{
	return DamageMultiplierCurve ? FMath::Max(0.0f, DamageMultiplierCurve->GetFloatValue(RunTimeSeconds)) : EvaluateDefaultDamageMultiplier();
}

float AEnemySpawner::GetSpawnPressure() const
{
	return SpawnPressureCurve ? FMath::Max(0.0f, SpawnPressureCurve->GetFloatValue(RunTimeSeconds)) : EvaluateDefaultSpawnPressure();
}

float AEnemySpawner::GetCurrentSpawnInterval() const
{
	const FEnemyPressurePhase* Phase = ResolveActivePressurePhase(nullptr, false);
	if (!Phase || GetTotalDesiredPopulation(*Phase) <= 0)
	{
		return 0.0f;
	}

	const float Ratio = GetPopulationRatio(*Phase);
	const float PhaseInterval = Ratio < 0.5f
		? Phase->EmergencySpawnInterval
		: (Ratio < 0.8f ? Phase->AcceleratedSpawnInterval : Phase->NormalSpawnInterval);
	return FMath::Max(0.01f, PhaseInterval / FMath::Max(0.01f, GetSpawnPressureModifierProduct()));
}

int32 AEnemySpawner::GetCurrentMaxAliveEnemies() const
{
	const FEnemyPressurePhase* Phase = ResolveActivePressurePhase(nullptr, false);
	return Phase ? FMath::Clamp(Phase->GlobalMaxAlive, 0, FMath::Max(1, AbsoluteHardAliveCap)) : 0;
}

float AEnemySpawner::EvaluateDefaultDamageMultiplier() const
{
	const float Minutes = GetRunTimeMinutes();
	if (Minutes <= 5.0f)
	{
		return FMath::Lerp(1.0f, 1.25f, Minutes / 5.0f);
	}

	return FMath::Lerp(1.25f, 1.6f, FMath::Clamp((Minutes - 5.0f) / 5.0f, 0.0f, 1.0f));
}

float AEnemySpawner::EvaluateDefaultSpawnPressure() const
{
	const float Minutes = GetRunTimeMinutes();
	if (Minutes <= 5.0f)
	{
		return FMath::Lerp(1.0f, 2.15f, Minutes / 5.0f);
	}

	return FMath::Lerp(2.15f, 3.35f, FMath::Clamp((Minutes - 5.0f) / 5.0f, 0.0f, 1.0f));
}

bool AEnemySpawner::IsSpawnDefinitionUnlocked(const FEnemySpawnEntry& Entry) const
{
	const bool bTimeAllowed = RunTimeSeconds >= Entry.MinimumRunTime
		&& (Entry.MaximumRunTime <= 0.0f || RunTimeSeconds < Entry.MaximumRunTime);
	return Entry.bEnabled && Entry.EnemyClass && bTimeAllowed;
}

bool AEnemySpawner::IsClassCooldownActive(TSubclassOf<AEnemyBase> EnemyClass, float* OutRemainingSeconds) const
{
	const float* NextEligibleTime = EnemyClass ? NextEligibleSpawnTimeByClass.Find(EnemyClass.Get()) : nullptr;
	const float Remaining = NextEligibleTime ? FMath::Max(0.0f, *NextEligibleTime - RunTimeSeconds) : 0.0f;
	if (OutRemainingSeconds)
	{
		*OutRemainingSeconds = Remaining;
	}
	return Remaining > 0.0f;
}

bool AEnemySpawner::IsPopulationEntryEligible(const FEnemyPressurePhase& Phase, const FEnemyPopulationPhaseEntry& PopulationEntry) const
{
	if (!PopulationEntry.EnemyClass || PopulationEntry.DesiredPopulation <= 0
		|| PopulationEntry.MaxPopulation <= 0 || PopulationEntry.RefillPriority <= 0.0f)
	{
		return false;
	}

	const FEnemySpawnEntry* SpawnDefinition = FindSpawnDefinition(PopulationEntry.EnemyClass);
	if (!SpawnDefinition || SpawnDefinition->PressureSpawnMode != EEnemyPressureSpawnMode::MaintainPopulation
		|| !IsSpawnDefinitionUnlocked(*SpawnDefinition) || IsClassCooldownActive(PopulationEntry.EnemyClass))
	{
		return false;
	}

	const int32 AliveOfType = GetAliveCountForSpawnClass(PopulationEntry.EnemyClass);
	return AliveOfType < PopulationEntry.DesiredPopulation && AliveOfType < PopulationEntry.MaxPopulation;
}

bool AEnemySpawner::IsTimedThreatEntryEligible(const FEnemyPressurePhase& Phase, const FEnemyPopulationPhaseEntry& PopulationEntry) const
{
	if (!PopulationEntry.EnemyClass || PopulationEntry.MaxPopulation <= 0 || PopulationEntry.RefillPriority <= 0.0f)
	{
		return false;
	}

	const FEnemySpawnEntry* SpawnDefinition = FindSpawnDefinition(PopulationEntry.EnemyClass);
	if (!SpawnDefinition || SpawnDefinition->PressureSpawnMode != EEnemyPressureSpawnMode::TimedThreat
		|| !IsSpawnDefinitionUnlocked(*SpawnDefinition) || IsClassCooldownActive(PopulationEntry.EnemyClass))
	{
		return false;
	}

	return GetAliveCountForSpawnClass(PopulationEntry.EnemyClass) < PopulationEntry.MaxPopulation;
}

int32 AEnemySpawner::GetTotalDesiredPopulation(const FEnemyPressurePhase& Phase) const
{
	int32 TotalDesired = 0;
	for (const FEnemyPopulationPhaseEntry& Entry : Phase.EnemyPopulationEntries)
	{
		const FEnemySpawnEntry* SpawnDefinition = FindSpawnDefinition(Entry.EnemyClass);
		if (Entry.EnemyClass && Entry.DesiredPopulation > 0 && Entry.MaxPopulation > 0
			&& SpawnDefinition && SpawnDefinition->PressureSpawnMode == EEnemyPressureSpawnMode::MaintainPopulation
			&& IsSpawnDefinitionUnlocked(*SpawnDefinition))
		{
			TotalDesired += FMath::Min(Entry.DesiredPopulation, Entry.MaxPopulation);
		}
	}
	return TotalDesired;
}

float AEnemySpawner::GetPopulationRatio(const FEnemyPressurePhase& Phase) const
{
	const int32 TotalDesired = GetTotalDesiredPopulation(Phase);
	if (TotalDesired <= 0)
	{
		return 1.0f;
	}
	int32 LivingCount = 0;
	for (const FEnemyPopulationPhaseEntry& Entry : Phase.EnemyPopulationEntries)
	{
		const FEnemySpawnEntry* SpawnDefinition = FindSpawnDefinition(Entry.EnemyClass);
		if (SpawnDefinition && SpawnDefinition->PressureSpawnMode == EEnemyPressureSpawnMode::MaintainPopulation
			&& IsSpawnDefinitionUnlocked(*SpawnDefinition))
		{
			LivingCount += GetAliveCountForSpawnClass(Entry.EnemyClass);
		}
	}
	return static_cast<float>(LivingCount) / static_cast<float>(TotalDesired);
}

int32 AEnemySpawner::GetCurrentPopulationBatchSize(const FEnemyPressurePhase& Phase) const
{
	const float Ratio = GetPopulationRatio(Phase);
	return FMath::Max(1, Ratio < 0.5f ? EmergencyMaxBatchSize : (Ratio < 0.8f ? AcceleratedMaxBatchSize : NormalMaxBatchSize));
}

