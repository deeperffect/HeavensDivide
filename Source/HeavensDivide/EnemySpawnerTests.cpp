#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "EnemySpawner.h"
#include "MeleeEnemyBase.h"
#include "RangedEnemyBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpawnerDirectorTest, "HeavensDivide.Enemies.SpawnDirector",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSpawnerDirectorTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    AEnemySpawner* Spawner = World->SpawnActor<AEnemySpawner>();
    TestFalse(TEXT("Spawner does not tick"), Spawner->PrimaryActorTick.bCanEverTick);

    FEnemySpawnEntry Grunt;
    Grunt.EnemyClass = AMeleeEnemyBase::StaticClass();
    Grunt.HealthScalingPerMinute = 0.05f;
    Grunt.MinRespawnDelayAfterDeath = Grunt.MaxRespawnDelayAfterDeath = 2.0f;
    FEnemySpawnEntry Threat;
    Threat.EnemyClass = ARangedEnemyBase::StaticClass();
    Threat.PressureSpawnMode = EEnemyPressureSpawnMode::TimedThreat;
    Threat.MinimumRunTime = 60.0f;
    Spawner->EnemySpawnEntries = {Grunt, Threat};

    FEnemyPopulationPhaseEntry GruntPopulation;
    GruntPopulation.EnemyClass = Grunt.EnemyClass;
    GruntPopulation.DesiredPopulation = GruntPopulation.MaxPopulation = 10;
    FEnemyPopulationPhaseEntry ThreatPopulation;
    ThreatPopulation.EnemyClass = Threat.EnemyClass;
    ThreatPopulation.MaxPopulation = 2;
    FEnemyPressurePhase Early;
    Early.EndTimeSeconds = 60.0f;
    Early.EnemyPopulationEntries = {GruntPopulation};
    FEnemyPressurePhase Later = Early;
    Later.StartTimeSeconds = 60.0f;
    Later.EndTimeSeconds = 0.0f;
    Later.bEventsEnabled = true;
    Later.EnemyPopulationEntries.Add(ThreatPopulation);
    Spawner->PressurePhases = {Early, Later};

    int32 PhaseIndex = INDEX_NONE;
    Spawner->RunTimeSeconds = 59.99f;
    TestTrue(TEXT("Early phase exists"), Spawner->ResolveActivePressurePhase(&PhaseIndex, false) != nullptr);
    TestEqual(TEXT("Early phase ends exclusively"), PhaseIndex, 0);
    TestNull(TEXT("Threat remains locked before its start"), Spawner->ChooseTimedThreatEntry(Later));
    Spawner->RunTimeSeconds = 60.0f;
    Spawner->ResolveActivePressurePhase(&PhaseIndex, false);
    TestEqual(TEXT("Next phase starts at boundary"), PhaseIndex, 1);
    TestNotNull(TEXT("Threat unlocks at boundary"), Spawner->ChooseTimedThreatEntry(Later));
    TestEqual(TEXT("Per-class health scales continuously"), Spawner->GetHealthMultiplier(Grunt), 1.05f);
    Spawner->PressurePhases[0].EndTimeSeconds = 61.0f;
    TestNull(TEXT("Overlapping phases pause selection"), Spawner->ResolveActivePressurePhase(nullptr, false));
    Spawner->PressurePhases[0].EndTimeSeconds = 59.0f;
    Spawner->RunTimeSeconds = 59.5f;
    TestNull(TEXT("Phase gaps pause selection"), Spawner->ResolveActivePressurePhase(nullptr, false));
    Spawner->PressurePhases[0] = Early;
    Spawner->RunTimeSeconds = 120.0f;

    Spawner->AliveEnemyCountByClass.Add(Grunt.EnemyClass.Get(), 0);
    TestEqual(TEXT("Large deficit uses emergency interval"), Spawner->GetCurrentSpawnInterval(), Later.EmergencySpawnInterval);
    Spawner->AliveEnemyCountByClass[Grunt.EnemyClass.Get()] = 5;
    TestEqual(TEXT("Half population uses accelerated interval"), Spawner->GetCurrentSpawnInterval(), Later.AcceleratedSpawnInterval);
    Spawner->AliveEnemyCountByClass[Grunt.EnemyClass.Get()] = 8;
    TestEqual(TEXT("Healthy population uses normal interval"), Spawner->GetCurrentSpawnInterval(), Later.NormalSpawnInterval);
    Spawner->AliveEnemyCountByClass[Grunt.EnemyClass.Get()] = 10;
    TestNull(TEXT("Maintained population never exceeds desired count"), Spawner->ChoosePopulationDeficitEntry(Later));
    Spawner->AliveEnemyCountByClass.Add(Threat.EnemyClass.Get(), 2);
    TestNull(TEXT("Threat respects phase cap"), Spawner->ChooseTimedThreatEntry(Later));
    Spawner->AliveEnemyCountByClass[Threat.EnemyClass.Get()] = 0;
    Spawner->NextEligibleSpawnTimeByClass.Add(Threat.EnemyClass.Get(), 121.0f);
    TestNull(TEXT("Threat replacement waits for cooldown"), Spawner->ChooseTimedThreatEntry(Later));
    Spawner->RunTimeSeconds = 121.0f;
    TestNotNull(TEXT("Cooldown expires at exact run-time boundary"), Spawner->ChooseTimedThreatEntry(Later));

    FEnemyPressureEventDefinition Event;
    Event.EventName = TEXT("TestEvent");
    FEnemyPressureEventEnemyEntry Member;
    Member.EnemyClass = Grunt.EnemyClass;
    Member.Count = 1;
    Member.ClassOverflowAllowance = 1;
    Event.EnemyEntries.Add(Member);
    TestTrue(TEXT("Event can use its explicit overflow allowance"), Spawner->IsEventSelectable(Event, Later));
    Event.bAllowTemporaryPopulationOverflow = false;
    TestFalse(TEXT("Event cannot exceed class cap without overflow"), Spawner->IsEventSelectable(Event, Later));
    Event.bAllowTemporaryPopulationOverflow = true;
    Spawner->AbsoluteHardAliveCap = 10;
    TestFalse(TEXT("Event cannot exceed absolute cap"), Spawner->IsEventSelectable(Event, Later));

    Spawner->AliveEnemyCountByClass.Empty();
    AEnemyBase* Enemy = World->SpawnActor<AMeleeEnemyBase>();
    Spawner->IncrementAliveCountForSpawnedEnemy(Enemy, Grunt.EnemyClass);
    TestEqual(TEXT("Spawn is counted once"), Spawner->GetAliveCountForSpawnClass(Grunt.EnemyClass), 1);
    Spawner->HandleSpawnedEnemyDied(Enemy);
    TestEqual(TEXT("Logical death frees population slot"), Spawner->GetAliveCountForSpawnClass(Grunt.EnemyClass), 0);
    TestEqual(TEXT("Death schedules authored cooldown"), Spawner->NextEligibleSpawnTimeByClass.FindChecked(Grunt.EnemyClass.Get()), 123.0f);
    Spawner->HandleSpawnedEnemyDestroyed(Enemy);
    TestEqual(TEXT("Later destruction does not double-decrement"), Spawner->GetAliveCountForSpawnClass(Grunt.EnemyClass), 0);

    Spawner->StartSpawning();
    TestTrue(TEXT("Spawning uses a timer"), World->GetTimerManager().IsTimerActive(Spawner->SpawnTimerHandle));
    Spawner->StopSpawning();
    TestFalse(TEXT("Stopping clears spawn timer"), World->GetTimerManager().IsTimerActive(Spawner->SpawnTimerHandle));
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
