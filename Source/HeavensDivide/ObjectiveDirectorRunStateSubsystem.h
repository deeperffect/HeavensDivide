#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ObjectiveDirectorRunStateSubsystem.generated.h"

class AObjectiveSpawnPoint;

UCLASS()
class HEAVENSDIVIDE_API UObjectiveDirectorRunStateSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	void InitializeRunDecisions();
	bool IsInitialized() const { return bInitialized; }
	bool HasExecuted(int32 MilestoneIndex) const;
	uint8 GetExecutedMilestoneMask() const { return ExecutedMilestoneMask; }
	int32 GetUsedPointCount() const;
	void MarkExecuted(int32 MilestoneIndex);
	float RollFraction();
	int32 RandRange(int32 Min, int32 Max);
	bool IsPointUsed(const AObjectiveSpawnPoint* Point) const;
	void MarkPointUsed(AObjectiveSpawnPoint* Point);
	void TrackSpawnedObjective(AActor* Objective);
	TArray<TWeakObjectPtr<AActor>> SpawnedObjectives;
private:
	bool bInitialized = false;
	uint8 ExecutedMilestoneMask = 0;
	FRandomStream RandomStream;
	TArray<TWeakObjectPtr<AObjectiveSpawnPoint>> UsedPoints;
};
