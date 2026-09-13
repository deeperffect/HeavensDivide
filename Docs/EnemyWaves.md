# Survival enemy waves

The current setup is authored in `Tools/enemy_wave_schedule.json` and applied by
`Tools/configure_enemy_waves.py`. It updates both `BP_EnemySpawner` Class Defaults
and the placed spawner in `/Game/Maps/Lvl_B1_Lvl1`, replacing their previous
different configurations. `configure_enemy_pressure_v1.py` is the legacy setup;
running it will overwrite this schedule.

## Basic waves

Each wave replenishes exactly one basic enemy type. Types are not randomly mixed.
Existing survivors remain when the next wave begins; killing or outrunning them
makes room under the shared living cap. Spawn positions retain the existing
directional pressure and arena validation. Wave counts are target living
populations, not simultaneous bursts or kill quotas.

| Run time | Basic enemy | Target alive | Global living cap |
| --- | --- | ---: | ---: |
| 0:00–1:00 | Grunt | 40 | 52 |
| 1:00–2:00 | Goblin Crawler | 48 | 61 |
| 2:00–3:00 | Creature | 56 | 70 |
| 3:00–4:00 | Fish | 64 | 79 |
| 4:00–5:00 | Bear | 72 | 88 |
| 5:00–6:00 | Floating Skeleton | 80 | 98 |
| 6:00–7:00 | Grunt | 88 | 106 |
| 7:00–8:00 | Goblin Crawler | 96 | 114 |
| 8:00–9:00 | Creature | 104 | 124 |
| 9:00–10:00 | Fish | 112 | 133 |
| 10:00–11:00 | Bear | 120 | 142 |
| 11:00 onward | Floating Skeleton | 128 | 150 |

The second rotation increases density and refill speed. The final wave is
open-ended, with health and existing damage scaling continuing over run time.
The absolute safety cap is 160. Normal refill uses two enemies per tick,
accelerated refill three, and emergency refill four; the normal interval falls
from 0.70 seconds to 0.425 seconds. Caps leave room for special enemies and
survivors from the previous wave.

| Basic enemy | Base HP | Speed (cm/s) | Intended feel |
| --- | ---: | ---: | --- |
| Grunt | 18 | 160 | Slow, forgiving opening fodder |
| Goblin Crawler | 12 | 255 | Fragile, fast pursuit |
| Creature | 24 | 200 | Sturdier, steady pursuit |
| Fish | 10 | 300 | Fastest and most fragile |
| Bear | 32 | 135 | Slowest, toughest basic mob |
| Floating Skeleton | 16 | 225 | Light, mobile fodder |

Basic HP scales by +3% of base HP per elapsed minute at spawn. Their attack logic,
damage, animations, and rewards are preserved. These are initial balance values;
the final feel still needs a playtest with upgrades and player movement.

## Special pressure

All four special types use `TimedThreat`, never maintained basic populations.
They have a 12% chance to take a refill slot while basic deficits exist. When
basic populations are full, eligible threats can fill their small caps. Their
cooldown begins on death and delays that class's replacement; it does not mean
a guaranteed spawn at an exact interval. Existing special HP, speed, and attack
behavior are preserved, including Gorilla's 5,000 base HP.

| Special | First eligible | Initial → final living cap | Replacement delay after death | HP growth/min |
| --- | --- | --- | --- | --- |
| Devil Ranged | 1:30 | 1 → 3 | 16–22 seconds | 6% |
| Goblin Bomb | 2:30 | 1 → 3 | 22–30 seconds | 4% |
| Ogre | 3:30 | 1 → 2 | 35–45 seconds | 8% |
| Gorilla | 5:30 | 1 → 2 | 60–80 seconds | 2% |

The old Grunt Surge, Skeleton Rush, and Heavy Push event lists are cleared so
they cannot introduce out-of-sequence basic mobs. Floating Skeleton is now a
basic maintained mob. Grunt spatial recycling remains guarded by the active
phase and therefore cannot introduce Grunts during another type's wave.

## Applying and verifying

Run `Tools/configure_enemy_waves.py` using Unreal's Python commandlet. Before
editing, it backs up the six basic Blueprints, spawner, and survival map under
`Saved/Backups/EnemyWaves/<timestamp>/`. Only those assets are saved.

Run `Tools/validate_enemy_waves.py` in a **fresh** commandlet afterward. It checks
the saved basic HP/speed, exact Blueprint and placed-spawner schedule, all ten
enemy types' reachability, wave boundary coverage, threat capacity, and refill
cadence. It writes `Saved/enemy_waves_validation.json` without saving assets.

Samurai Boss, Twin Soul Crimson, Twin Soul Violet, and Test Dummy are not part
of this setup.
