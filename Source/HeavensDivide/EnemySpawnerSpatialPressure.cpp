#include "EnemySpawner.h"

#include "CharacterBase.h"
#include "EnemyBase.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Stats/Stats.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarDebugSpatialPressure(
	TEXT("hd.DebugSpatialPressure"), 0,
	TEXT("Draws movement, sectors, stale Grunts, and replacement choices for spatial pressure."));
static TAutoConsoleVariable<int32> CVarLogSpatialPressure(
	TEXT("hd.LogSpatialPressurePasses"), 0,
	TEXT("Logs every automatic spatial-pressure evaluation when set to 1."));
#endif

int32 AEnemySpawner::ChooseReplacementSector(const TArray<int32>& SectorCounts) const
{
	if (SectorCounts.IsEmpty()) return INDEX_NONE;
	if (!bPreferUnderrepresentedSectors) return FMath::RandRange(0, SectorCounts.Num() - 1);
	const float Randomness = FMath::Clamp(ReplacementSectorRandomness, 0.0f, 1.0f);
	float TotalWeight = 0.0f;
	TArray<float> Weights;
	Weights.Reserve(SectorCounts.Num());
	for (const int32 Count : SectorCounts)
	{
		const float Weight = FMath::Lerp(1.0f / (1.0f + FMath::Max(0, Count)), 1.0f, Randomness);
		Weights.Add(Weight);
		TotalWeight += Weight;
	}
	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (int32 Index = 0; Index < Weights.Num(); ++Index)
	{
		Roll -= Weights[Index];
		if (Roll <= 0.0f) return Index;
	}
	return Weights.Num() - 1;
}

AEnemyBase* AEnemySpawner::SpawnSpatialPressureReplacement(const TArray<int32>& SectorCounts, int32& OutSectorIndex)
{
	OutSectorIndex = INDEX_NONE;
	if (!SpatialPressureGruntClass || GetAliveEnemyCount() >= GetCurrentMaxAliveEnemies()) return nullptr;
	const FEnemyPressurePhase* Phase = ResolveActivePressurePhase();
	const FEnemySpawnEntry* Definition = FindSpawnDefinition(SpatialPressureGruntClass);
	const FEnemyPopulationPhaseEntry* Population = Phase ? Phase->EnemyPopulationEntries.FindByPredicate([this](const FEnemyPopulationPhaseEntry& Entry)
	{
		return Entry.EnemyClass == SpatialPressureGruntClass;
	}) : nullptr;
	if (!Phase || !Definition || Definition->PressureSpawnMode != EEnemyPressureSpawnMode::MaintainPopulation || !Population
		|| GetAliveCountForSpawnClass(SpatialPressureGruntClass) >= Population->MaxPopulation) return nullptr;

	ACharacterBase* Player = GetActivePlayerCharacter();
	OutSectorIndex = ChooseReplacementSector(SectorCounts);
	if (!Player || OutSectorIndex == INDEX_NONE) return nullptr;
	const int32 SectorCount = SectorCounts.Num();
	const float SectorWidth = 360.0f / static_cast<float>(SectorCount);
	const float SectorCenter = (static_cast<float>(OutSectorIndex) + 0.5f) * SectorWidth;
	FVector SpawnLocation;
	if (!FindSpawnLocation(Player->GetActorLocation(), SpatialPressureGruntClass, SpawnLocation, true, SectorCenter, SectorWidth,
		ReplacementSpawnDistanceMin, ReplacementSpawnDistanceMax)
		&& !FindSpawnLocation(Player->GetActorLocation(), SpatialPressureGruntClass, SpawnLocation, true, SectorCenter, SectorWidth,
			MinSpawnDistance, MaxSpawnDistance)) return nullptr;

	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	AEnemyBase* Enemy = GetWorld()->SpawnActor<AEnemyBase>(SpatialPressureGruntClass, SpawnLocation, FRotator::ZeroRotator, Parameters);
	if (!Enemy) return nullptr;
	Enemy->SpawnDefaultController();
	Enemy->ApplySpawnDifficultyScaling(GetHealthMultiplier(*Definition), GetDamageMultiplier());
	ApplyEnemySpawnModifierContexts(Enemy);
	if (Enemy->IsBloodbound()) OnEnemyBecameBloodbound.Broadcast(Enemy);
	Enemy->OnDestroyed.AddDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDestroyed);
	Enemy->OnEnemyDied.AddUniqueDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDied);
	SpawnedEnemies.Add(Enemy);
	IncrementAliveCountForSpawnedEnemy(Enemy, SpatialPressureGruntClass);
	TrackEnemySpawnMetadata(Enemy, false);
#if !UE_BUILD_SHIPPING
	if (CVarDebugSpatialPressure.GetValueOnGameThread() != 0)
	{
		DrawDebugSphere(GetWorld(), SpawnLocation, 65.0f, 16, FColor::Cyan, false, SpatialPressureEvaluationInterval, 0, 3.0f);
		DrawDebugLine(GetWorld(), Player->GetActorLocation() + FVector(0, 0, 60), SpawnLocation + FVector(0, 0, 60), FColor::Cyan, false, SpatialPressureEvaluationInterval, 0, 2.0f);
	}
#endif
	return Enemy;
}

void AEnemySpawner::DrawSpatialPressureDebug(const ACharacterBase* Player, const FVector& MovementDirection, const TArray<int32>& SectorCounts, const TArray<AEnemyBase*>& StaleEnemies) const
{
#if !UE_BUILD_SHIPPING
	if (!Player || CVarDebugSpatialPressure.GetValueOnGameThread() == 0) return;
	const FVector Origin = Player->GetActorLocation() + FVector(0, 0, 80);
	DrawDebugDirectionalArrow(GetWorld(), Origin, Origin + MovementDirection * 700.0f, 80.0f, FColor::Green, false, SpatialPressureEvaluationInterval, 0, 4.0f);
	const float Width = 2.0f * PI / static_cast<float>(FMath::Max(1, SectorCounts.Num()));
	for (int32 Index = 0; Index < SectorCounts.Num(); ++Index)
	{
		const float Angle = Index * Width;
		const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0);
		DrawDebugLine(GetWorld(), Origin, Origin + Direction * 1000.0f, FColor::Silver, false, SpatialPressureEvaluationInterval, 0, 1.0f);
		const float LabelAngle = Angle + Width * 0.5f;
		DrawDebugString(GetWorld(), Origin + FVector(FMath::Cos(LabelAngle), FMath::Sin(LabelAngle), 0) * 850.0f,
			FString::Printf(TEXT("%d: %d"), Index, SectorCounts[Index]), nullptr, FColor::White, SpatialPressureEvaluationInterval, false, 1.1f);
	}
	for (AEnemyBase* Enemy : StaleEnemies)
	{
		if (Enemy) DrawDebugSphere(GetWorld(), Enemy->GetActorLocation() + FVector(0, 0, 60), 75.0f, 12, FColor::Magenta, false, SpatialPressureEvaluationInterval, 0, 3.0f);
	}
#endif
}

void AEnemySpawner::EvaluateSpatialPressure(bool bAllowRecycling, bool bForceLog)
{
	if (bTrialSuspended || bRunTimeFrozen || !bSpawningEnabled || !bEnableSpatialPressureRecycling || !SpatialPressureGruntClass) return;
	ACharacterBase* Player = GetActivePlayerCharacter();
	const FEnemyPressurePhase* Phase = ResolveActivePressurePhase();
	if (!Player || !Phase) return;
	const FEnemyPopulationPhaseEntry* GruntPopulation = Phase->EnemyPopulationEntries.FindByPredicate([this](const FEnemyPopulationPhaseEntry& Entry)
	{
		return Entry.EnemyClass == SpatialPressureGruntClass;
	});
	if (!GruntPopulation || GruntPopulation->DesiredPopulation <= 0) return;
	PruneTrackedEnemies();
	const int32 GruntAlive = GetAliveCountForSpawnClass(SpatialPressureGruntClass);
	const bool bPopulationHealthy = static_cast<float>(GruntAlive) >= static_cast<float>(GruntPopulation->DesiredPopulation) * FMath::Clamp(MinimumGruntPopulationRatioForRecycling, 0.0f, 1.0f);
	FVector Velocity = Player->GetVelocity(); Velocity.Z = 0.0f;
	const float PlayerSpeed = Velocity.Size();
	const bool bMovingEnough = PlayerSpeed >= MinimumPlayerSpeedForDirectionalRecycling;
	const FVector MovementDirection = bMovingEnough ? Velocity / PlayerSpeed : FVector::ZeroVector;
	const FVector PlayerLocation = Player->GetActorLocation();
	const int32 SectorCount = FMath::Clamp(SpatialSectorCount, 4, 16);
	TArray<int32> SectorCounts; SectorCounts.Init(0, SectorCount);
	struct FStaleCandidate { AEnemyBase* Enemy; float Distance; float Dot; float Score; int32 Sector; };
	TArray<FStaleCandidate> Candidates;
	const float SectorWidth = 360.0f / static_cast<float>(SectorCount);
	for (const TPair<TObjectKey<AEnemyBase>, UClass*>& Pair : CountedEnemyClassByEnemy)
	{
		AEnemyBase* Enemy = Pair.Key.ResolveObjectPtr();
		if (!IsValid(Enemy) || Enemy->IsDead() || Pair.Value != SpatialPressureGruntClass.Get()) continue;
		FVector Offset = Enemy->GetActorLocation() - PlayerLocation; Offset.Z = 0.0f;
		const float Distance = Offset.Size();
		int32 EnemySector = INDEX_NONE;
		if (Distance > KINDA_SMALL_NUMBER)
		{
			const float Angle = FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X)) + 360.0f, 360.0f);
			EnemySector = FMath::Clamp(FMath::FloorToInt(Angle / SectorWidth), 0, SectorCount - 1);
			SectorCounts[EnemySector]++;
		}
		const float* SpawnTime = EnemySpawnRunTimeByEnemy.Find(Pair.Key);
		const float* ProtectedUntil = EventRecycleProtectionEndTimeByEnemy.Find(Pair.Key);
		const float Dot = Distance > KINDA_SMALL_NUMBER && bMovingEnough ? FVector::DotProduct(Offset / Distance, MovementDirection) : 1.0f;
		if (bMovingEnough && Distance >= StaleGruntMinimumDistance && Dot <= StaleGruntBehindDotThreshold
			&& SpawnTime && RunTimeSeconds - *SpawnTime >= MinimumSecondsAliveBeforeRecyclable
			&& (!ProtectedUntil || RunTimeSeconds >= *ProtectedUntil))
		{
			Candidates.Add({ Enemy, Distance, Dot, Distance / FMath::Max(1.0f, StaleGruntMinimumDistance) + (1.0f - Dot), EnemySector });
		}
	}
	Candidates.Sort([](const FStaleCandidate& A, const FStaleCandidate& B) { return A.Score > B.Score; });
#if !UE_BUILD_SHIPPING
	if (CVarDebugSpatialPressure.GetValueOnGameThread() != 0)
	{
		TArray<AEnemyBase*> DebugStale;
		DebugStale.Reserve(Candidates.Num());
		for (const FStaleCandidate& Candidate : Candidates) DebugStale.Add(Candidate.Enemy);
		DrawSpatialPressureDebug(Player, MovementDirection, SectorCounts, DebugStale);
	}
	const bool bLogPass = bForceLog || CVarLogSpatialPressure.GetValueOnGameThread() != 0;
#else
	constexpr bool bLogPass = false;
#endif
	const bool bRecycleAllowed = bAllowRecycling && bMovingEnough && bPopulationHealthy;
	const int32 RecycleTarget = bRecycleAllowed ? FMath::Min(FMath::Max(0, MaxGruntsRecycledPerEvaluation), Candidates.Num()) : 0;
	TArray<FString> RecycleLines;
	TArray<int32> ReplacementSectors;
	int32 Recycled = 0, Replaced = 0;
	for (int32 Index = 0; Index < RecycleTarget; ++Index)
	{
		const FStaleCandidate& Candidate = Candidates[Index];
		if (bLogPass) RecycleLines.Add(FString::Printf(TEXT("%s Distance=%.0f Dot=%.2f"), *GetNameSafe(Candidate.Enemy), Candidate.Distance, Candidate.Dot));
		if (!RecycleLivingEnemy(Candidate.Enemy)) continue;
		++Recycled;
		if (SectorCounts.IsValidIndex(Candidate.Sector)) SectorCounts[Candidate.Sector] = FMath::Max(0, SectorCounts[Candidate.Sector] - 1);
		int32 Sector = INDEX_NONE;
		if (SpawnSpatialPressureReplacement(SectorCounts, Sector))
		{
			++Replaced; if (bLogPass) ReplacementSectors.Add(Sector); if (SectorCounts.IsValidIndex(Sector)) ++SectorCounts[Sector];
		}
	}
#if !UE_BUILD_SHIPPING
	if (bForceLog || CVarLogSpatialPressure.GetValueOnGameThread() != 0)
	{
		FString Counts, Replacements;
		for (int32 Index = 0; Index < SectorCounts.Num(); ++Index) Counts += FString::Printf(TEXT("%s%d=%d"), Index ? TEXT(" ") : TEXT(""), Index, SectorCounts[Index]);
		for (int32 Index = 0; Index < ReplacementSectors.Num(); ++Index) Replacements += FString::Printf(TEXT("%s%d"), Index ? TEXT(",") : TEXT(""), ReplacementSectors[Index]);
		UE_LOG(LogTemp, Log, TEXT("SpatialPressure: Enabled=true PlayerSpeed=%.0f GruntAlive=%d GruntDesired=%d RecycleAllowed=%s SectorCounts=[%s] StaleCandidates=%d RecyclingThisPass=%d Replaced=%d ReplacementSectors=[%s]"),
			PlayerSpeed, GruntAlive, GruntPopulation->DesiredPopulation, bRecycleAllowed ? TEXT("true") : TEXT("false"), *Counts, Candidates.Num(), Recycled, Replaced, *Replacements);
		for (const FString& Line : RecycleLines) UE_LOG(LogTemp, Log, TEXT("  Recycle: %s"), *Line);
	}
#endif
}

void AEnemySpawner::HandleSpatialPressureTimerElapsed()
{
	EvaluateSpatialPressure(true, false);
}

#if !UE_BUILD_SHIPPING
void AEnemySpawner::LogSpatialPressureStatus() { EvaluateSpatialPressure(false, true); }
void AEnemySpawner::ForceSpatialPressurePass() { EvaluateSpatialPressure(true, true); }
#endif

