# Upgrade balance and VFX editing

## Steel Tempest example

In Unreal's Content Browser open:

`/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiSteelTempest`

Expand **Runtime Balance > Balance Parameters**:

- `Radius`: base attack radius in centimeters. Change 260 to 400 for a larger Tempest. Both target acquisition and actual damage use this value.
- `Damage`: base damage before character power and Force upgrades.
- `Cooldown`: base seconds between casts, before Rhythm scaling.
- `Count` / `Interval`: pulse count and spacing. With Count 1, only one base pulse occurs.
- `StatusStacks`: stacks applied when the relevant status-granting branch is owned. Setting it to zero disables that application's stacks.

Expand **Runtime VFX**:

1. Assign **Pulse System** to your Niagara asset.
2. Keep **Show Fallback With Niagara** disabled to replace the generated ring for that stage.
3. Set **Authored Radius** to your Niagara effect's radius at scale 1. With **Scale System To Radius** enabled, it scales automatically to the actual upgraded gameplay radius.
4. Adjust **Location Offset**, **Rotation Offset**, **Scale**, and lifetime controls as needed. These change visuals, not the damage area or cast timing.
5. Save the asset and start a fresh PIE run. No C++ rebuild is needed.

The default ring anchor is 70 cm below the enemy/player center. Location Offset is added to that anchor and remains applied while a field follows its owner.

## Where each type lives

| What to edit | Location |
| --- | --- |
| Base family damage, radius, cooldown, targeting, timing, pattern-specific settings | Starter's Runtime Balance map |
| Branch damage fractions, delays, angles, counts, splash/return settings | Branch card's Runtime Balance map |
| Synergy payout, reach, target count, preparation duration, re-prime delay, trigger throttle, pull/refund/echo settings | Synergy card under `/Game/HeavensDivide/Upgrades/Synergy/DA_BuildSynergy_*` |
| Force / Reach / Rhythm / Wide Arc / Velocity magnitudes | Scaling card's existing Rarity Magnitudes entries; Common, Rare and Epic are independent |
| Maximum ranks, rarity, prerequisites, exclusivity, artwork, card text | Existing Upgrade Data Asset fields |
| Tag Team setup attack damage, range, target counts, cone and pushback | `/Game/HeavensDivide/Upgrades/Synergy/DA_Synergy_TagTeam` Runtime Balance |
| Tag Team frequency, placement and visibility duration | InactiveCharacterAssist component on `BP_SurvivorPlayerController` |
| Earlier Double Cut / Fan of Blades / technique attack settings | AutoAttack component on the relevant character Blueprint |
| Bleed / Poison tick damage, duration, interval and legacy support fallbacks | EnemyStatusEffect component on the base enemy Blueprint |
| Earlier Hemotoxic Reaction, Virulent Strain, Handoff and Blade Cascade settings | Their existing Upgrade Data Asset fields |
| Mastery power per point | PlayerUpgrade component on `BP_SurvivorPlayerController` |

Blade Wave's width, damage multiplier, travel distance/speed, spawn offsets, collision thickness/height and authored VFX duration are now on its starter. Crossing Blades has frequency, wave count and side angle; Returning Blade has return damage multiplier; Splinter Wave has its damage fraction and radius. The migration copies existing Blueprint-authored wave values so it does not silently reset that attack. The older component values remain fallbacks when no asset override exists.

Only values actually read by an upgrade are seeded. Do not rename map keys: the key identifies the runtime setting. Unsupported arbitrary keys have no behavior. Pure behavior branches may have VFX but no numerical settings. Counts are rounded and bounded; scheduler intervals have a 0.1-second minimum. Internal work budgets, collision deduplication and recursion guards remain code safeguards.

## Branch inheritance

An acquired branch can override a matching starter balance key. Explicit branch-only keys affect only that branch's behavior. When multiple branches override the same key, branch 3 takes precedence over 2, then 1.

For branch visuals, enable **Override Family Visuals** on the branch card, then configure its presentation. Otherwise it inherits the starter's visuals. The same precedence applies if several acquired branches enable a visual override. This replaces the whole family's presentation while that branch is owned; it is not an extra effect layered on every hit.

Synergy presentation is independent: enable Override Family Visuals on the synergy card to customize its reaction. Otherwise it inherits the starter. A starter's own presentation always applies without needing that override checkbox.

## VFX slots

- **Pulse System:** rings, circles, explosions and fields. On native Blade Wave, it attaches to the traveling wave and replaces its earlier visuals when fallback-with-Niagara is disabled.
- **Line System:** connecting streaks, lanes and targeted strikes.
- **Warning System:** marked-position warnings, where that ability has a warning stage.
- **Impact System:** optional effect at each successful family hit. Empty means no additional impact actor.
- **Detonation System:** Venom Garden's Samurai detonation; falls back to Pulse System if empty.

A slot is used only when the ability emits that stage. Assigning a line system does not turn a circular ability into a line attack. Set **Minimum Spawn Interval** to throttle cosmetic spawning for busy abilities. **Sound**, **Sound Concurrency**, volume and pitch use short-lived fire-and-forget audio. A sound can play on every emitted stage; use concurrency and the spawn interval to control density.

You can also replace the fallback ring/line materials, their color/intensity parameter names, color, fade behavior and line thickness. Fallback geometry is shown when no Niagara is assigned unless you disable it. Visual lifetimes are bounded by actor cleanup; warnings finish when the warned attack resolves, and native wave effects end with their wave.

## Niagara parameter contract

The component sets these configurable names before activating the system:

| Default name | Niagara type | Value |
| --- | --- | --- |
| `User.Radius` | Float | Actual pulse radius or damaging lane width in centimeters, including Reach; zero for purely decorative connecting streaks |
| `User.Duration` | Float | Scheduled visual lifetime |
| `User.StartPosition` | Vector3 | World-space visual start |
| `User.EndPosition` | Vector3 | World-space line end, including End Offset |
| `User.Color` | Linear Color | Family color or the configured override |

Your Niagara graph must read these parameters for them to affect its emitters. If your graph sizes itself using User.Radius, **disable Scale System To Radius** to avoid applying radius scaling twice. If it uses a fixed authored size, keep automatic transform scaling enabled and set Authored Radius correctly. Parameter names can be changed or cleared.

Changing balance does not automatically rewrite manually authored card descriptions. Update those descriptions alongside significant tuning changes. Existing run-upgrade magnitudes and already-cast damage can be snapshotted; use a fresh run for a clean balance comparison.

## Maintenance

`Tools/configure_upgrade_tuning.py` seeds missing keys only, backs up assets under `Saved/Backups/UpgradeTuning`, and preserves existing balance/VFX edits. The catalog JSON and generated C++ table are now fallback defaults, not the authoritative runtime balance for seeded assets. `Tools/upgrade_tuning_defaults.json` records migration defaults.

Validation: Win64 Development Editor build succeeded. All eight automation suites passed, including HeavensDivide.Abilities.EditorTuning. The new test edits real loaded Data Assets and verifies radius acquisition/damage, damage amount, cooldown, branch echo delay/fraction, synergy payout/throttle, Niagara asset assignment and radius parameter, transform scaling, following offsets and branch visual inheritance. Migration saved 101 runtime assets; existing scaling cards remain editable through Rarity Magnitudes. Report: Saved/Automation/UpgradeTuning/index.json.
