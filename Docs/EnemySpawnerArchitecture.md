# Enemy spawner guide

`BP_EnemySpawner` retains its saved roster, population phases, pressure events, placement rules and scaling. The spawner is timer-driven and does not tick.

## Code organization

| File | Responsibility |
| --- | --- |
| `EnemySpawner.cpp` | Lifecycle, timers, actor spawning, tracking, modifiers, death/destruction and high-level status logs |
| `EnemySpawnerPopulation.cpp` | Phase resolution, maintained-population selection, timed-threat selection, caps, refill rates and scaling |
| `EnemySpawnerPlacement.cpp` | Candidate generation, floor traces, navigation projection, arena bounds and collision validation |
| `EnemySpawnerEvents.cpp` | Directional bias, event eligibility/scheduling, member emission and event cooldowns |
| `EnemySpawnerSpatialPressure.cpp` | Stale-grunt detection, sector weighting, recycling and associated diagnostics |
| `EnemySpawnerDebug.cpp` | Console commands, stress spawning and temporary autoattack suppression; excluded from Shipping |
| `EnemySpawnerDiagnostics.h` | Shared console-variable declarations |
| `EnemySpawner.h` | Reflected configuration, public API and runtime state |

## Blueprint settings

Settings are grouped under `Spawner`: Roster, Pressure, Events, Placement, Run Scaling, Modifiers, Debug and Legacy. Pressure includes population batches, timed threats, directional bias and spatial recycling.

Roster entries define class availability, per-minute health scaling and replacement cooldowns. Phase entries define desired populations, class caps and refill priorities. Event entries define their own members, intervals and explicit overflow allowances.

`SpawnWeight`, `SpawnCost` and `MaxAliveOfThisType` are unused by the current pressure director. They are marked as deprecated, advanced Legacy fields alongside the previously deprecated interval/budget settings. Serialized values remain for asset compatibility. Adjust phase population/priority/caps instead.

## Preserved behavior

Phase ranges keep their exclusive end times; missing or overlapping phases pause routine spawning. Population ratios still choose the existing normal/accelerated/emergency refill rates. Timed threats retain their independent chance and death-based class cooldowns. Event schedules, member order, overflow caps, random draws, recycling selection and spawn validation are unchanged.

Event-member arrays retain capacity between events, and event selection uses inline storage for up to eight candidate indices. Spatial recycling builds debug-only arrays and formatted strings only when their diagnostics are enabled. This reduces avoidable allocations without changing gameplay decisions; no measured frame-rate gain is claimed.

The three disconnected default events in `BP_EnemySpawner` can be removed with the audit's explicit `-CleanupSpawnerBlueprint` flag. It backs up the asset, compiles it and verifies editable defaults before saving.

## Validation

`HeavensDivide.Enemies.SpawnDirector` covers phase transitions and invalid ranges, population refill rates, threat unlocks/caps/cooldowns, event overflow and absolute caps, death/destruction counting, health scaling and spawn-timer start/stop.

`HeavensDivide.Enemies.BlueprintAudit` compiles the spawner and related enemy assets and compares editable defaults against `Saved/EnemyCleanupBaseline.txt`. Logs for this pass are `Saved/Logs/SpawnerCleanupBuild.log`, `SpawnerCleanupTests.log` and `SpawnerCleanupReload.log`.
