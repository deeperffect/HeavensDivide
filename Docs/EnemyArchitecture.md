# Enemy code and Blueprint guide

See [Enemy spawner guide](EnemySpawnerArchitecture.md) for the director's code layout and tuning categories.

Enemy Blueprints supply tuning, meshes, animation classes, montages and effects. Native classes implement their behavior. Property names, class paths, component names and saved tuning are preserved by the cleanup.

## Enemy roster

| Blueprints | Native behavior |
| --- | --- |
| `BP_EnemyGrunt`, `BP_EnemyGoblinCrawler`, `BP_EnemyCreature`, `BP_EnemyFish`, `BP_EnemyFloatingSkeleton`, `BP_EnemyBear` | `MeleeEnemyBase`: chase, attack windup, directional hit, recovery and procedural lunge |
| `BP_EnemyDevilRanged` | `RangedEnemyBase`: attack montage with projectile notify |
| `BP_EnemyOgre`, `BP_EnemyGorilla` | `TankMeleeEnemyBase`: montage-driven slam or contact damage, as configured per Blueprint |
| `BP_EnemyGoblinBomb` | `GoblinBombEnemy`: committed fuse, charge presentation, explosion and normal death/reward processing |
| `BP_SamuraiBoss` | `FinalBossBase`: phase transitions, attack selection, dash and ground telegraphs |
| `BP_TwinSoulCrimson`, `BP_TwinSoulViolet` | Melee/ranged behavior with objective damage restrictions |
| `BP_TestDummy` | Melee base with its existing test tuning |

Boss, trial and dummy assets are included in cleanup validation; this does not add them to the regular spawn pool.

## Where to edit code

| File | Responsibility |
| --- | --- |
| `EnemyBase.cpp` | Lifecycle, targeting, damage, status entry points, bloodbound state, rewards, death and animation budgeting |
| `EnemyMovement.cpp` | Behavior/separation timers, chase steering, ground snapping, navigation fallback and facing |
| `EnemyIndicators.cpp` | Health bar, mark indicator and bleed/poison indicator layout |
| `EnemyLightweightMovementComponent.cpp` | Swept movement, wall sliding and pushback |
| `MeleeEnemyBase.cpp` | Basic melee attack phases and hit detection |
| `MontageMeleeEnemyBase.cpp` | Montage-driven melee and animation-notify hits |
| `TankMeleeEnemyBase.cpp` | Slam shapes, facing lock, telegraph fill and contact damage |
| `RangedEnemyBase.cpp` | Ranged attack scheduling and projectile spawning |
| `GoblinBombEnemy.cpp` | Bomb fuse, flashing/wobble and detonation |
| `FinalBossBase.cpp`, `BossGroundTelegraph.cpp` | Boss state machine and telegraphed attacks |
| `EnemyStatusEffectComponent.cpp`, `EnemyDeathComponent.cpp` | Timed status damage and death presentation |
| `EnemySpawner.cpp`, `EnemySpawnArea.cpp` | Population phases, pressure events, recycling, placement and tracking |

Blueprint settings are grouped under `Enemy`: Movement, Attack, UI, Components, Animation Budget, Damage, Death, Rewards and status-related categories. Melee and ranged attacks have separate subcategories. Boss and bomb-specific settings retain their existing dedicated categories. Spawner pressure phases remain the source of progression tuning.

## Runtime cleanup

- Removed 42 unconnected default events across all 14 enemy Blueprints and two native overrides that only forwarded to their parent.
- Removed unused private spawner helpers for legacy spawn budget, batch size and default health scaling. Serialized legacy settings remain for asset compatibility and retain their existing deprecation metadata.
- Removed an unused animation-profiling getter/cache; the live profiling switch remains.
- Movement sweeps reuse per-component hit-array capacity, including wall slides and committed moves. Queries consume their results before actor movement or the next sweep.
- Boss attack selection uses inline storage for its four candidates.

AI/separation intervals, attack cooldowns, animation-notify timing and random choices are unchanged. Movement, tank facing, boss dashes and active death/telegraph presentation still need their existing updates. Movement and death components already disable ticking when idle; the spawner and status effects already use timers. No measured frame-rate improvement is claimed.

## Validation

`HeavensDivide.Enemies.BlueprintAudit` compiles the 14 enemy actors and related animation, projectile, spawner, trial, gate and UI Blueprints (32 assets). It checks editable defaults on class defaults and native default subobjects before/after compilation.

For this cleanup, `Saved/EnemyCleanupBaseline.txt` records the pre-cleanup editable defaults. Subsequent audit runs compare against that snapshot. The audit's optional `-CleanupEnemyBlueprints` flag removes only unconnected default BeginPlay, ActorBeginOverlap and Tick events from enemy actors, backs up each asset under `Saved/Backups/EnemyCleanup`, and saves only after successful compilation and unchanged editable defaults.

Run the `HeavensDivide.Enemies`, `HeavensDivide.EnemyDeath`, `HeavensDivide.Combat`, `HeavensDivide.Abilities` and `HeavensDivide.ImpactFeedback` automation groups for regression coverage. Headless checks do not replace an in-editor visual/playthrough check of attacks and animation.

The cleanup build and all 15 tests in those groups passed. Logs are under `Saved/Logs/EnemyCleanupBuild.log` and `Saved/Logs/EnemyCleanupTests.log`.
