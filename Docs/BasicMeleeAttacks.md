# Basic melee attacks

Standard `AMeleeEnemyBase` enemies use a timer attack with no montage or animation-notify entry point. Their existing locomotion/idle animation continues normally. `AMontageMeleeEnemyBase` now holds the previous special-enemy montage property, playback, hit-notify entry point and completion callback. `ATankMeleeEnemyBase` inherits that special branch. FinalBossBase, ranged enemies and player attack implementations were not changed.

## Sequence and editor settings

In a basic enemy Blueprint's Class Defaults, use **Enemy > Attack > Melee**:

| Property | Behavior |
| --- | --- |
| Attack Range | Existing distance to start an attack; preserved. |
| Attack Windup | Time spent rocking back before impact; varies by species below. |
| Attack Recovery | Time after impact before resuming movement. |
| Attack Interval | Start-to-start cooldown, separately tuned from the delay before impact. |
| Attack Damage | Existing damage, including existing difficulty scaling. |
| Attack Hit Radius | Existing radius, preserved at 150 on these mobs; expand advanced properties to edit. |
| Attack Hit Forward Offset | Existing forward offset, preserved at 90; expand advanced properties to edit. |
| Debug Attack Hit | Temporary sphere visualization when explicitly enabled, guarded by Unreal's debug-drawing build flag. |

Under **Enemy > Attack > Melee > Presentation**, tune **Use Procedural Attack Motion**, **Attack Wobble Angle** (12 degrees), **Attack Lunge Distance** (30 cm), **Attack Lunge Out Duration** (0.06 seconds), and **Attack Lunge Return Duration** (0.12 seconds). Out Duration controls the forward swing at the end of windup; Out + Return Duration controls the settling motion, capped by recovery.

### Movement and attack variation

| Enemy | Chase speed (cm/s) | Windup (s) | Attack interval (s) | Recovery (s) |
| --- | ---: | ---: | ---: | ---: |
| Bear | 135 | 0.18 | 0.85 | 0.20 |
| Grunt | 160 | 0.28 | 1.00 | 0.22 |
| Creature | 200 | 0.42 | 1.25 | 0.25 |
| Floating Skeleton | 225 | 0.55 | 1.45 | 0.28 |
| Goblin Crawler | 255 | 0.70 | 1.70 | 0.30 |
| Fish | 300 | 0.85 | 1.90 | 0.32 |
| Twin Soul Crimson (trial) | 300 | 0.85 | 1.90 | 0.32 |

Faster chasers take longer to land their first hit after reaching the player and repeat attacks more slowly. Slower chasers wind up and repeat attacks faster. Each new attack has its own full windup. Fish's speed now matches its authored wave schedule (300); the other movement speeds are preserved. Damage, health, hit areas, spawn populations and the test dummy's zero damage/999-second interval are preserved.

The saved Blueprint values come from `Tools/basic_melee_profiles.json`. Apply them with `Tools/configure_basic_melee_profiles.py`; it backs up the affected assets under `Saved/Backups/MeleeWobble` before saving. Run `Tools/verify_basic_melee.py` in a fresh Unreal Python commandlet to verify persistence, the movement/timing tradeoff, absence of montage playback, and the special-enemy defaults.

The enemy stops chase input and current movement, faces the player through the existing FaceTarget method, winds up, checks damage once, then recovers. Overlapping attack requests are ignored. Cooldown is measured from attack start. The movement stop threshold is capped just inside the basic attack range, fixing the existing 150-unit stop distance versus 140-unit attack range mismatch without altering either authored property. Special enemies retain their prior stop-distance behavior.

Impact uses an exact 3D sphere-versus-capsule overlap against the active player's capsule. The sphere sits in front of the enemy at Attack Hit Forward Offset; a forward-facing check rejects a player behind the enemy. This avoids querying every enemy in a crowd and does not depend on capsule overlap events. Damage still goes through SurvivorPlayerController.ApplyDamageToPlayer. Escaping the sphere before impact causes a miss. The existing ground attack cue uses the same windup duration and signals impact on the damage frame.

The wobble changes the skeletal mesh's relative location and rotation, so attached visuals follow while the actor/capsule/navigation position stays fixed. It rocks back and sideways during windup, swings forward to its peak on the damage frame, then briefly rebounds and settles. Rotation uses the actor's axes to support differently oriented imported meshes. A 60 Hz timer runs only during the attack, deriving motion from the captured baseline and restoring both position and rotation without drift. If recovery is shortened, motion fits inside it. Death, target replacement/removal, destruction, gameplay suspension and behavior cancellation clear the relevant timers and restore the baseline before death presentation starts. EnemyDeathComponent and reward/drop handling are unchanged.

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

The 2026-10-10 wobble update passed the UE 5.8 Editor Win64 Development build and five automation tests: `HeavensDivide.Enemies.BasicMelee`, `HeavensDivide.Enemies.GoblinBomb`, `HeavensDivide.EnemyDeath.Lifecycle`, `HeavensDivide.Combat.TankIndicators`, and `HeavensDivide.Encounters.Mechanics`. New checks cover visible windup rotation, peak motion at impact, authored mesh orientation/scale, rotation restoration, shortened recovery, and actual Bear/Crawler Blueprint timing. A separate commandlet reloaded the saved profiles, compiled the basic Blueprints, checked for montage playback nodes, and compared all 26 enemies against the pre-change audit. Results are in `Saved/MeleeWobbleTests` and `Saved/MeleeWobbleAssets.log`.

UE Editor Win64 Development build succeeded. Automation tests HeavensDivide.Enemies.BasicMelee, HeavensDivide.Enemies.GoblinBomb and HeavensDivide.EnemyDeath.Lifecycle passed. Basic melee tests cover approach/stop/resume behavior, no montage/notify API, unpaused animation state, timer windup, hits/misses/behind-target rejection, recovery/cooldown, duplicate impact prevention, exact repeated lunge restoration, death at windup/impact, target removal, suspension and independent enemy timers. Goblin Bomb's native test fixture emits existing missing skeletal-mesh/socket warnings; no assertions failed.

These are headless logic/asset checks, not a manual navigation or visual playthrough. Use PIE to tune the lunge and attack reach against your current camera and enemy sizes.
