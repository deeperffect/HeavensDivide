#include "ObjectiveDirectorRunStateSubsystem.h"
#include "ObjectiveSpawnPoint.h"

void UObjectiveDirectorRunStateSubsystem::InitializeRunDecisions(float OptionalChance)
{
	if (bInitialized) return;
	RandomStream.Initialize(FMath::Rand());
	GuaranteedTrial = RandomStream.RandRange(0, 1) == 0 ? ERunCharacterTrialChoice::Samurai : ERunCharacterTrialChoice::Ninja;
	OptionalTrialRoll = RandomStream.FRand();
	bOptionalTrialWillSpawn = OptionalTrialRoll < FMath::Clamp(OptionalChance, 0.0f, 1.0f);
	bInitialized = true;
}

bool UObjectiveDirectorRunStateSubsystem::HasExecuted(int32 MilestoneIndex) const
{
	return MilestoneIndex >= 0 && MilestoneIndex < 8 && (ExecutedMilestoneMask & (1 << MilestoneIndex)) != 0;
}

int32 UObjectiveDirectorRunStateSubsystem::GetUsedPointCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AObjectiveSpawnPoint>& Entry : UsedPoints) if (Entry.IsValid()) ++Count;
	return Count;
}

void UObjectiveDirectorRunStateSubsystem::MarkExecuted(int32 MilestoneIndex)
{
	if (MilestoneIndex >= 0 && MilestoneIndex < 8) ExecutedMilestoneMask |= (1 << MilestoneIndex);
}

float UObjectiveDirectorRunStateSubsystem::RollFraction() { return RandomStream.FRand(); }
int32 UObjectiveDirectorRunStateSubsystem::RandRange(int32 Min, int32 Max) { return RandomStream.RandRange(Min, Max); }

bool UObjectiveDirectorRunStateSubsystem::IsPointUsed(const AObjectiveSpawnPoint* Point) const
{
	return UsedPoints.ContainsByPredicate([Point](const TWeakObjectPtr<AObjectiveSpawnPoint>& Entry){ return Entry.Get() == Point; });
}

void UObjectiveDirectorRunStateSubsystem::MarkPointUsed(AObjectiveSpawnPoint* Point)
{
	if (IsValid(Point)) UsedPoints.AddUnique(Point);
}

void UObjectiveDirectorRunStateSubsystem::TrackSpawnedObjective(AActor* Objective)
{
	SpawnedObjectives.RemoveAll([](const TWeakObjectPtr<AActor>& Entry){ return !Entry.IsValid(); });
	if (IsValid(Objective)) SpawnedObjectives.AddUnique(Objective);
}
