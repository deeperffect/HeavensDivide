# Basic melee attacks

Standard `AMeleeEnemyBase` enemies use a timer attack with no montage or animation-notify entry point. Their existing locomotion/idle animation continues normally. `AMontageMeleeEnemyBase` now holds the previous special-enemy montage property, playback, hit-notify entry point and completion callback. `ATankMeleeEnemyBase` inherits that special branch. FinalBossBase, ranged enemies and player attack implementations were not changed.

## Sequence and editor settings

In a basic enemy Blueprint's Class Defaults, use **Combat > Melee Attack**:

| Property | Behavior |
| --- | --- |
| Attack Range | Existing distance to start an attack; preserved. |
| Attack Windup | 0.20 seconds before impact. |
| Attack Recovery | 0.25 seconds after impact before resuming behavior. |
| Attack Interval | Existing start-to-start cooldown; preserved at 1.5 seconds on the migrated basic mobs. This is the cooldown setting, not a new duplicate property. |
| Attack Damage | Existing damage, including existing difficulty scaling. |
| Attack Hit Radius | Existing radius, preserved at 150 on these mobs; expand advanced properties to edit. |
| Attack Hit Forward Offset | Existing forward offset, preserved at 90; expand advanced properties to edit. |
| Debug Attack Hit | Temporary sphere visualization when explicitly enabled, guarded by Unreal's debug-drawing build flag. |

Under **Combat > Melee Attack > Presentation**, tune **Use Procedural Attack Motion**, **Attack Lunge Distance** (30 cm), **Attack Lunge Out Duration** (0.06 seconds), and **Attack Lunge Return Duration** (0.12 seconds).

The enemy stops chase input and current movement, faces the player through the existing FaceTarget method, winds up, checks damage once, then recovers. Overlapping attack requests are ignored. Cooldown is measured from attack start. The movement stop threshold is capped just inside the basic attack range, fixing the existing 150-unit stop distance versus 140-unit attack range mismatch without altering either authored property. Special enemies retain their prior stop-distance behavior.

Impact uses an exact 3D sphere-versus-capsule overlap against the active player's capsule. The sphere sits in front of the enemy at Attack Hit Forward Offset; a forward-facing check rejects a player behind the enemy. This avoids querying every enemy in a crowd and does not depend on capsule overlap events. Damage still goes through SurvivorPlayerController.ApplyDamageToPlayer. Escaping the sphere before impact causes a miss. No decal is added.

The lunge changes only the skeletal mesh's relative location, so attached visuals follow while the actor/capsule/navigation position stays fixed. A 60 Hz timer runs only during the short lunge, interpolating from the captured baseline and restoring it exactly. If recovery is shortened, motion fits inside it. Death, target replacement/removal, destruction, gameplay suspension and behavior cancellation clear the relevant timers and restore the baseline before death presentation starts. EnemyDeathComponent and reward/drop handling are unchanged.

## Blueprint migration

Migrated and resaved:

- BP_EnemyGrunt
- BP_EnemyBear
- BP_EnemyCreature
- BP_EnemyFish
- BP_EnemyFloatingSkeleton
- BP_EnemyGoblinCrawler
- BP_TestDummy (zero damage and long cooldown preserved)

Floating Skeleton's saved setup at the start of this task was basic MeleeEnemyBase using the Grunt attack montage, rather than its older Tank/AoE setup. Migration follows that current saved setup.

BP_EnemyOgre retains its montage/slam attack; BP_EnemyGorilla retains contact damage; BP_EnemyGoblinBomb retains its custom fuse, wobble, flash and explosion. Their attack defaults were checked against the pre-migration snapshot. EnemyAttackHit notifies now target MontageMeleeEnemyBase only. The unused AM_EnemyGruntAttack asset, including its obsolete basic hit-notify timing, was removed after verifying zero asset referencers. Its backup is under Saved/Backups/BasicMelee. Shared animation sequences, animation Blueprints and special/player notify classes were retained.

## Files

- Modified: Source/HeavensDivide/MeleeEnemyBase.h/.cpp, EnemyBase.h/.cpp, TankMeleeEnemyBase.h, AnimNotify_EnemyAttackHit.cpp.
- Added: Source/HeavensDivide/MontageMeleeEnemyBase.h/.cpp and BasicMeleeEnemyTests.cpp.
- Added: Tools/migrate_basic_melee.py, Tools/basic_melee_original_defaults.json and Tools/verify_basic_melee.py.
- Modified the seven Blueprint assets listed above; removed AM_EnemyGruntAttack.uasset.

The migration utility uses the recorded original defaults to detect unexpected balance changes and reports unreferenced old assets for removal after Unreal exits. Original enemy assets were backed up before the native hierarchy change. The read-only verifier reloads/compiles enemy Blueprints and their assigned animation Blueprints and compares balance/special montage settings.

## Verification

UE Editor Win64 Development build succeeded. Automation tests HeavensDivide.Enemies.BasicMelee, HeavensDivide.Enemies.GoblinBomb and HeavensDivide.EnemyDeath.Lifecycle passed. Basic melee tests cover approach/stop/resume behavior, no montage/notify API, unpaused animation state, timer windup, hits/misses/behind-target rejection, recovery/cooldown, duplicate impact prevention, exact repeated lunge restoration, death at windup/impact, target removal, suspension and independent enemy timers. Goblin Bomb's native test fixture emits existing missing skeletal-mesh/socket warnings; no assertions failed.

These are headless logic/asset checks, not a manual navigation or visual playthrough. Use PIE to tune the lunge and attack reach against your current camera and enemy sizes.
