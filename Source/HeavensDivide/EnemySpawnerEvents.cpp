#include "EnemySpawner.h"
#include "EnemySpawnerDiagnostics.h"

#include "CharacterBase.h"
#include "EnemyBase.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"


static TAutoConsoleVariable<int32> CVarDebugPressureEvents(
	TEXT("hd.DebugPressureEvents"), 0,
	TEXT("Development only. Logs pressure-event starts/completions and draws their shared direction and arc."));

void AEnemySpawner::UpdateDirectionalBias()
{
	if (!bDirectionalBiasEnabled)
	{
		return;
	}
	if (RunTimeSeconds >= DirectionalBiasEndRunTime)
	{
		DirectionalBiasAngleDegrees = FMath::FRandRange(0.0f, 360.0f);
		const float MinDuration = FMath::Min(DirectionalBiasDurationMin, DirectionalBiasDurationMax);
		const float MaxDuration = FMath::Max(DirectionalBiasDurationMin, DirectionalBiasDurationMax);
		DirectionalBiasEndRunTime = RunTimeSeconds + FMath::FRandRange(FMath::Max(0.1f, MinDuration), FMath::Max(0.1f, MaxDuration));
	}
}

bool AEnemySpawner::IsEventSelectable(const FEnemyPressureEventDefinition& Event, const FEnemyPressurePhase& Phase, FString* OutReason, bool bIgnoreEventSchedule) const
{
	auto Fail = [OutReason](const TCHAR* Reason) { if (OutReason) { *OutReason = Reason; } return false; };
	if (!bIgnoreEventSchedule && !Phase.bEventsEnabled) return Fail(TEXT("phase events disabled"));
	if (!Event.bEnabled || Event.EventName.IsNone() || Event.Weight <= 0.0f) return Fail(TEXT("disabled/invalid"));
	if (!bIgnoreEventSchedule && (RunTimeSeconds < FMath::Max(120.0f, Event.MinimumRunTime)
		|| (Event.MaximumRunTime > 0.0f && RunTimeSeconds >= Event.MaximumRunTime))) return Fail(TEXT("outside time window"));
	if (!bIgnoreEventSchedule)
	{
		if (const float* EligibleTime = NextEligibleEventTimeByName.Find(Event.EventName); EligibleTime && RunTimeSeconds < *EligibleTime) return Fail(TEXT("event cooldown"));
	}
	if (Event.EnemyEntries.Num() == 0) return Fail(TEXT("no members"));

	int32 RequestedMembers = 0;
	for (const FEnemyPressureEventEnemyEntry& EventEntry : Event.EnemyEntries)
	{
		const FEnemySpawnEntry* Definition = FindSpawnDefinition(EventEntry.EnemyClass);
		const FEnemyPopulationPhaseEntry* Population = Phase.EnemyPopulationEntries.FindByPredicate([&EventEntry](const FEnemyPopulationPhaseEntry& Entry)
		{
			return Entry.EnemyClass == EventEntry.EnemyClass;
		});
		if (!Definition || !Population || !IsSpawnDefinitionUnlocked(*Definition)) return Fail(TEXT("required class unavailable"));
		if (!Event.bIgnoreThreatDeathCooldown && IsClassCooldownActive(EventEntry.EnemyClass)) return Fail(TEXT("required class cooldown"));
		const int32 Allowance = Event.bAllowTemporaryPopulationOverflow ? FMath::Max(0, EventEntry.ClassOverflowAllowance) : 0;
		if (Population->MaxPopulation <= 0 || GetAliveCountForSpawnClass(EventEntry.EnemyClass) + EventEntry.Count > Population->MaxPopulation + Allowance)
			return Fail(TEXT("class capacity"));
		RequestedMembers += FMath::Max(0, EventEntry.Count);
	}
	const int32 EventCap = FMath::Min(Phase.GlobalMaxAlive + (Event.bAllowTemporaryPopulationOverflow ? FMath::Max(0, Event.EventPopulationOverflowAllowance) : 0), AbsoluteHardAliveCap);
	int32 CurrentLiving = 0;
	for (const TPair<UClass*, int32>& Pair : AliveEnemyCountByClass) CurrentLiving += FMath::Max(0, Pair.Value);
	if (CurrentLiving + RequestedMembers > EventCap) return Fail(TEXT("global/event capacity"));
	if (OutReason) *OutReason = TEXT("selectable");
	return true;
}

void AEnemySpawner::UpdateEventScheduler(const FEnemyPressurePhase* ActivePhase)
{
	if (!ActivePhase || ActiveEventIndex != INDEX_NONE || !ActivePhase->bEventsEnabled || RunTimeSeconds < 120.0f)
	{
		return;
	}
	if (NextGlobalEventTime <= 0.0f)
	{
		const float MinInterval = FMath::Min(ActivePhase->EventIntervalMin, ActivePhase->EventIntervalMax);
		const float MaxInterval = FMath::Max(ActivePhase->EventIntervalMin, ActivePhase->EventIntervalMax);
		NextGlobalEventTime = RunTimeSeconds + FMath::FRandRange(MinInterval, MaxInterval);
		return;
	}
	if (RunTimeSeconds < NextGlobalEventTime)
	{
		return;
	}

	TArray<int32, TInlineAllocator<8>> EligibleIndices;
	float TotalWeight = 0.0f;
	for (int32 Index = 0; Index < PressureEvents.Num(); ++Index)
	{
		if (IsEventSelectable(PressureEvents[Index], *ActivePhase))
		{
			EligibleIndices.Add(Index);
		}
	}
	const bool bHasAlternative = EligibleIndices.ContainsByPredicate([this](int32 Index) { return PressureEvents[Index].EventName != LastCompletedEventName; });
	for (int32 Index : EligibleIndices)
	{
		if (!bHasAlternative || PressureEvents[Index].EventName != LastCompletedEventName) TotalWeight += PressureEvents[Index].Weight;
	}
	if (TotalWeight <= 0.0f)
	{
		NextGlobalEventTime = RunTimeSeconds + 1.0f;
		return;
	}
	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (int32 Index : EligibleIndices)
	{
		if (bHasAlternative && PressureEvents[Index].EventName == LastCompletedEventName) continue;
		Roll -= PressureEvents[Index].Weight;
		if (Roll <= 0.0f) { StartPressureEvent(Index, *ActivePhase); return; }
	}
}

#if !UE_BUILD_SHIPPING
void AEnemySpawner::TriggerPressureEventByName(FName EventName)
{
	if (ActiveEventIndex != INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PressureEvent] FORCE %s rejected: event %s is already active."),
			*EventName.ToString(), *PressureEvents[ActiveEventIndex].EventName.ToString());
		return;
	}
	const int32 EventIndex = PressureEvents.IndexOfByPredicate([EventName](const FEnemyPressureEventDefinition& Event) { return Event.EventName == EventName; });
	const FEnemyPressurePhase* Phase = ResolveActivePressurePhase();
	if (!PressureEvents.IsValidIndex(EventIndex) || !Phase)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PressureEvent] FORCE %s rejected: unknown event or invalid phase."), *EventName.ToString());
		return;
	}
	FString Reason;
	if (!IsEventSelectable(PressureEvents[EventIndex], *Phase, &Reason, true))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PressureEvent] FORCE %s rejected: %s."), *EventName.ToString(), *Reason);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[PressureEvent] FORCE %s accepted; event/global schedule bypassed."), *EventName.ToString());
	StartPressureEvent(EventIndex, *Phase);
}
#endif

void AEnemySpawner::StartPressureEvent(int32 EventIndex, const FEnemyPressurePhase& Phase)
{
	if (!PressureEvents.IsValidIndex(EventIndex)) return;
	const FEnemyPressureEventDefinition& Event = PressureEvents[EventIndex];
	ActiveEventIndex = EventIndex;
	ActiveEventMemberIndex = ActiveEventSpawnedCount = ActiveEventFailedCount = 0;
	ActiveEventDirectionAngle = FMath::FRandRange(0.0f, 360.0f);
	ActiveEventStartRunTime = RunTimeSeconds;
	ActiveEventOverflowCap = FMath::Min(Phase.GlobalMaxAlive + (Event.bAllowTemporaryPopulationOverflow ? FMath::Max(0, Event.EventPopulationOverflowAllowance) : 0), AbsoluteHardAliveCap);
	ActiveEventMembers.Reset();
	for (const FEnemyPressureEventEnemyEntry& Entry : Event.EnemyEntries)
	{
		for (int32 Count = 0; Count < FMath::Max(0, Entry.Count); ++Count) ActiveEventMembers.Add(Entry.EnemyClass);
	}
	if (CVarDebugPressureEvents.GetValueOnGameThread() != 0 || CVarLogSpawnDirector.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[PressureEvent] START %s RunTime=%.1f Direction=%.1f RequestedMembers=%d CurrentAlive=%d EventCap=%d"),
			*Event.EventName.ToString(), RunTimeSeconds, ActiveEventDirectionAngle, ActiveEventMembers.Num(), GetAliveEnemyCount(), ActiveEventOverflowCap);
	}
#if !UE_BUILD_SHIPPING
	if (CVarDebugPressureEvents.GetValueOnGameThread() != 0)
	{
		if (ACharacterBase* Player = GetActivePlayerCharacter())
		{
			const FVector Origin = Player->GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
			for (float Offset : { -Event.SpawnArcDegrees * 0.5f, 0.0f, Event.SpawnArcDegrees * 0.5f })
			{
				const float Radians = FMath::DegreesToRadians(ActiveEventDirectionAngle + Offset);
				DrawDebugLine(GetWorld(), Origin, Origin + FVector(FMath::Cos(Radians), FMath::Sin(Radians), 0.0f) * 1400.0f,
					Offset == 0.0f ? FColor::Red : FColor::Orange, false, 3.0f, 0, 5.0f);
			}
		}
	}
#endif
	EmitNextEventMember();
}

void AEnemySpawner::EmitNextEventMember()
{
	if (!PressureEvents.IsValidIndex(ActiveEventIndex) || !ActiveEventMembers.IsValidIndex(ActiveEventMemberIndex))
	{
		CompleteActiveEvent();
		return;
	}
	const FEnemyPressureEventDefinition& Event = PressureEvents[ActiveEventIndex];
	const TSubclassOf<AEnemyBase> EnemyClass = ActiveEventMembers[ActiveEventMemberIndex++];
	const FEnemyPressureEventEnemyEntry* EventEntry = Event.EnemyEntries.FindByPredicate([EnemyClass](const FEnemyPressureEventEnemyEntry& Entry) { return Entry.EnemyClass == EnemyClass; });
	if (SpawnEventEnemy(EnemyClass, EventEntry ? EventEntry->ClassOverflowAllowance : 0)) ++ActiveEventSpawnedCount; else ++ActiveEventFailedCount;
	if (ActiveEventMemberIndex >= ActiveEventMembers.Num())
	{
		CompleteActiveEvent();
	}
	else
	{
		const float NextDelay = ActiveEventMemberIndex == 1 && Event.DelayAfterFirstMember > 0.0f
			? Event.DelayAfterFirstMember : Event.DelayBetweenMembers;
		GetWorldTimerManager().SetTimer(EventMemberTimerHandle, this, &AEnemySpawner::EmitNextEventMember, FMath::Max(0.01f, NextDelay), false);
	}
}

void AEnemySpawner::CompleteActiveEvent()
{
	GetWorldTimerManager().ClearTimer(EventMemberTimerHandle);
	if (!PressureEvents.IsValidIndex(ActiveEventIndex)) { ActiveEventIndex = INDEX_NONE; ActiveEventMembers.Reset(); return; }
	const FEnemyPressureEventDefinition& Event = PressureEvents[ActiveEventIndex];
	const float MinCooldown = FMath::Min(Event.EventCooldownMin, Event.EventCooldownMax);
	const float MaxCooldown = FMath::Max(Event.EventCooldownMin, Event.EventCooldownMax);
	const float EventCooldown = FMath::FRandRange(MinCooldown, MaxCooldown);
	NextEligibleEventTimeByName.FindOrAdd(Event.EventName) = RunTimeSeconds + EventCooldown;
	LastCompletedEventName = Event.EventName;
	if (const FEnemyPressurePhase* Phase = ResolveActivePressurePhase(nullptr, false))
	{
		NextGlobalEventTime = RunTimeSeconds + FMath::FRandRange(FMath::Min(Phase->EventIntervalMin, Phase->EventIntervalMax), FMath::Max(Phase->EventIntervalMin, Phase->EventIntervalMax));
	}
	if (CVarDebugPressureEvents.GetValueOnGameThread() != 0 || CVarLogSpawnDirector.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[PressureEvent] COMPLETE %s Spawned=%d Failed=%d Duration=%.2f NextGlobalEventIn=%.2f EventCooldown=%.2f"),
			*Event.EventName.ToString(), ActiveEventSpawnedCount, ActiveEventFailedCount, RunTimeSeconds - ActiveEventStartRunTime,
			FMath::Max(0.0f, NextGlobalEventTime - RunTimeSeconds), EventCooldown);
	}
	ActiveEventIndex = INDEX_NONE;
	ActiveEventMembers.Reset();
}

