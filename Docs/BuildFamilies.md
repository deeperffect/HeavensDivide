# Twenty build families

Permanent passive progression is documented in [MetaSkillTree.md](MetaSkillTree.md).

For editor balance and Niagara setup, see [UpgradeTuning.md](UpgradeTuning.md). Runtime Data Assets now override the catalog defaults. [Per-upgrade balance keys](UpgradeTuningReference.md).

This roster contains **ten Samurai and ten Ninja families**. Each has one starter, three combinable behavior branches, three five-rank scalable cards and one partner synergy: **160 family cards total**, including upgraded existing cards. The controller pool contains 194 unique cards with the unrelated earlier upgrades retained.

## Start here

Start a fresh run, then use the Unreal console:

- `BuildPreview CrescentReaper` grants that starter, all three branches, rank 2 scaling, and its synergy.
- `BuildPreview CrescentReaper 1` grants only its first branch (plus the starter, scaling and synergy). Use 2 or 3 for another branch. This adds to the current run; it does not remove already acquired branches.
- Repeat the command with a partner family. Example: `BuildPreview LotusMines`.
- `BuildPreview All` is available for a saturation/stress test; an individual pair is better for assessing the combat feel.
- The older `AbilityShowcase` still previews its original four builds and setup assists.

These commands are disabled in shipping builds. They use normal run-local acquisition, preserve higher ranks, and repeat safely without adding mastery for already owned cards. No persistent meta unlocks are changed.

Suggested pairings:

| Samurai | Ninja | Playstyle |
| --- | --- | --- |
| Crescent Reaper | Lotus Mines | Cut through a crowd, then exploit prepared survivors with explosions and pulls. |
| War Banner | Crimson Needle | Mark a crowd, swap to Ninja and pick off weakened enemies. Native Ninja projectiles can consume intrinsic marks without needing Marked Blade. |
| Blood Moon | Raven Swarm | Spread Bleed, return to Ninja for persistent Poison and status-spreading follow-ups. |
| Spirit Lance | Thunder Wire | Build intersecting attack lanes and use the partner to pull enemies into follow-up damage. |
| Thousand Cuts | Phantom Ambush | Focus dangerous enemies, then swap to trigger heavy follow-ups and recharge refunds. |

## Rules

Only the active character starts and recharges automatic abilities. Already-cast attacks continue with their original damage attribution across swaps. Following fields/orbits follow their owner only while that character is active; they remain at the last position after a swap. Caltrops require movement, and Blade Wave continues to use the real melee attack/notify path.

Branches combine. Most require their starter and are fixed Rare cards. The four existing legendary evolutions (Razor Halo, Starfall, Black Web, Withering Garden) occupy the first branch slot of their respective families and retain the rank-2 Force/Reach requirement. Character offers reserve a starter, scaling card and branch/evolution when eligible; missing categories fall back to normal random eligible choices.

Force increases direct family damage; Reach increases its attack footprint, chain reach or acquisition range; Rhythm increases recharge speed. Blade Wave instead has Force, Wide Arc (its existing width + damage scaling), and Velocity (projectile travel speed). Force and Reach use Common/Rare/Epic values of 20/30/45% and 12/18/25%; Rhythm/Velocity uses 10/15/22%. Magnitudes add across ranks. Recharge = base cooldown / (1 + total Rhythm). Status damage continues to use the existing Bleed/Poison upgrades and mastery.

A synergy card lets its family prepare surviving enemies for six seconds. Actual partner attack/ability hits consume that preparation and execute the named follow-up. Same-character hits cannot trigger it. Preparation is transient, so it is not restored by swapping or merely standing nearby. Each family can trigger at most once per 0.35 seconds (one second for recharge refunds), with a two-second re-prime lockout on the victim. Reactions cannot recursively trigger reactions. Direct bonus damage retains the preparing family's source, scaling snapshot and damage restrictions.

The existing Venom Garden melee detonation remains baseline: its dedicated Plague Harvest card adds a separate partner-triggered Poison splash rather than making the baseline combo require another unlock.

## Samurai

### Steel Tempest (`SteelTempest`)

An automatic close-range steel ring.

1. **Razor Halo** — Echo the ring after 0.3 seconds for 65% damage.
2. **Storm Eye** — The ring also applies Bleed.
3. **Advancing Storm** — Send a second half-strength ring into the nearest crowd.

**Synergy: Storm Conductor.** Steel Tempest prepares surviving enemies for 6 seconds; the partner's hit chains bonus strikes through nearby enemies. Trigger with Ninja attacks or abilities. Follow-up damage is 60% of the preparing hit, per affected target; reach 300 cm, scaling with Reach.

### Heavenfall (`Heavenfall`)

Strike the densest nearby group after a warning; apply Bleed.

1. **Starfall** — Strike up to three separate groups.
2. **Aftershock** — Each strike echoes at half damage after 0.4 seconds.
3. **Seeking Star** — The warning follows its selected enemy until impact.

**Synergy: Fallen Constellation.** Heavenfall prepares surviving enemies for 6 seconds; the partner's hit calls down two delayed follow-up strikes. Trigger with Ninja attacks or abilities. Follow-up damage is 80% of the preparing hit, split across two echoes; reach 210 cm, scaling with Reach.

### Blade Wave (`BladeWave`)

Basic melee attacks launch the existing traveling blade wave.

1. **Returning Blade** — Waves return for another hit pass.
2. **Crossing Blades** — Every third swing launches three crossing waves.
3. **Splinter Wave** — The first enemy hit on each wave pass sheds a small 30%-damage Bleed burst.

**Synergy: Venom Edge.** Blade Wave prepares surviving enemies for 6 seconds; the partner's hit triggers a status-spreading burst. Trigger with Ninja attacks or abilities. Follow-up damage is 60% of the preparing hit, per affected target; reach 240 cm, scaling with Reach.

### Crescent Reaper (`CrescentReaper`)

A crescent travels through the crowd, hitting each enemy once per pass.

Base: 28 direct damage per hit/pulse, 120 cm radius/width, 4.5s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.

1. **Reaping Return** — The crescent retraces its path for a second hit pass.
2. **Twin Crescents** — Send an additional crescent angled 30 degrees to the side.
3. **Crimson Crescent** — Each hit applies Bleed.

**Synergy: Reaper's Invitation.** Crescent Reaper prepares surviving enemies for 6 seconds; the partner's hit pulls nearby enemies toward the victim and damages them. Trigger with Ninja attacks or abilities. Follow-up damage is 60% of the preparing hit, per affected target; reach 300 cm, scaling with Reach.

### Iron Orbit (`IronOrbit`)

Three blades circle Samurai for four seconds, cutting enemies along their paths.

Base: 7 direct damage per hit/pulse, 85 cm radius/width, 6s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.

1. **Counter Orbit** — Add a blade orbiting in the opposite direction.
2. **Satellite Blades** — Add a second outer orbit.
3. **Orbit Shrapnel** — Orbiting blades apply Bleed.

**Synergy: Orbit Relay.** Iron Orbit prepares surviving enemies for 6 seconds; the partner's hit deals a bonus strike and reduces this family's remaining cooldown by one second. Trigger with Ninja attacks or abilities. Follow-up damage is 50% of the preparing hit, per affected target; reach 180 cm, scaling with Reach.

### Fault Line (`FaultLine`)

Three delayed eruptions tear a line through the crowd.

Base: 30 direct damage per hit/pulse, 170 cm radius/width, 5s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.

1. **Cross Fault** — Add a perpendicular fault line.
2. **Rolling Aftershock** — Eruptions repeat at half damage.
3. **Rupture Lift** — Eruptions push enemies away from their centers.

**Synergy: Fault Detonator.** Fault Line prepares surviving enemies for 6 seconds; the partner's hit calls down two delayed follow-up strikes. Trigger with Ninja attacks or abilities. Follow-up damage is 75% of the preparing hit, split across two echoes; reach 250 cm, scaling with Reach.

### War Banner (`WarBanner`)

Plant a banner in a crowd. Four pulses damage and mark survivors for Ninja.

Base: 10 direct damage per hit/pulse, 300 cm radius/width, 7s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.

1. **Marching Banner** — Plant at Samurai and follow him while active.
2. **Rallying Banner** — The banner also pulses at a second nearby enemy position.
3. **Banner of Thorns** — Banner pulses also apply Bleed.

**Synergy: Rallying Shadows.** War Banner prepares surviving enemies for 6 seconds; the partner's hit deals a bonus strike and reduces this family's remaining cooldown by one second. Trigger with Ninja attacks or abilities. Follow-up damage is 100% of the preparing hit, per affected target; reach 260 cm, scaling with Reach.

### Thousand Cuts (`ThousandCuts`)

Focus six rapid cuts on one enemy; its death ends the flurry.

Base: 9 direct damage per hit/pulse, 75 cm radius/width, 4s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.

1. **Passing Sentence** — Retarget a nearby enemy if the victim dies.
2. **Shared Sentence** — Each cut splashes nearby enemies for half damage.
3. **Open Wounds** — Cuts apply Bleed.

**Synergy: Execution Window.** Thousand Cuts prepares surviving enemies for 6 seconds; the partner's hit triggers a heavy single-target follow-up. Trigger with Ninja attacks or abilities. Follow-up damage is 200% of the preparing hit, per affected target; reach 150 cm, scaling with Reach.

### Spirit Lance (`SpiritLance`)

Pierce a long, narrow line through enemies.

Base: 38 direct damage per hit/pulse, 70 cm radius/width, 4.5s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.

1. **Crossed Lances** — Fire two additional lances at side angles.
2. **Rebounding Lance** — Repeat the line after a short delay at half damage.
3. **Spectral Brand** — Mark enemies hit by the lance.

**Synergy: Shadow Spear.** Spirit Lance prepares surviving enemies for 6 seconds; the partner's hit chains bonus strikes through nearby enemies. Trigger with Ninja attacks or abilities. Follow-up damage is 65% of the preparing hit, per affected target; reach 440 cm, scaling with Reach.

### Blood Moon (`BloodMoon`)

An expanding hollow ring sweeps outward, applying Bleed.

Base: 18 direct damage per hit/pulse, 140 cm radius/width, 5s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.

1. **Second Moon** — The ring contracts after expanding, enabling a second pass.
2. **Red Tide** — Add a second expanding ring centered on the targeted crowd.
3. **Moon Recoil** — The wave pushes enemies outward.

**Synergy: Eclipse Harvest.** Blood Moon prepares surviving enemies for 6 seconds; the partner's hit triggers a status-spreading burst. Trigger with Ninja attacks or abilities. Follow-up damage is 100% of the preparing hit, per affected target; reach 320 cm, scaling with Reach.

## Ninja

### Night Thread (`NightThread`)

Chain between three enemies, prioritizing Bleed and poisoning bleeding targets.

1. **Black Web** — Leave a delayed half-damage burst at each hit.
2. **Venom Thread** — Every thread hit applies Poison, even without Bleed.
3. **Cross Stitch** — Connecting threads also cut enemies between their endpoints for 40% damage.

**Synergy: Sever the Thread.** Night Thread prepares surviving enemies for 6 seconds; the partner's hit chains bonus strikes through nearby enemies. Trigger with Samurai attacks or abilities. Follow-up damage is 70% of the preparing hit, per affected target; reach 360 cm, scaling with Reach.

### Venom Garden (`VenomGarden`)

Plant a wide field for eight slow pulses and Poison. Samurai melee detonates its remaining damage.

1. **Withering Garden** — Eleven pulses and a final burst; included in Samurai detonation.
2. **Wandering Garden** — Plant the field at Ninja; it follows him while he is active, then stays where he left it.
3. **Briar Garden** — Pulses push enemies outward, making the field a space-clearing tool.

**Synergy: Plague Harvest.** Venom Garden prepares surviving enemies for 6 seconds; the partner's hit triggers a status-spreading burst. Trigger with Samurai attacks or abilities. Follow-up damage is 400% of the preparing hit, per affected target; reach 300 cm, scaling with Reach.

### Lotus Mines (`LotusMines`)

Scatter three mines near a crowd. After arming, each bursts when an enemy approaches.

Base: 30 direct damage per hit/pulse, 180 cm radius/width, 6s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.

1. **Double Petals** — Scatter two additional mines.
2. **Patient Lotus** — A triggered mine rearms once for a second explosion.
3. **Venom Petals** — Explosions apply Poison.

**Synergy: Lotus Execution.** Lotus Mines prepares surviving enemies for 6 seconds; the partner's hit calls down two delayed follow-up strikes. Trigger with Samurai attacks or abilities. Follow-up damage is 80% of the preparing hit, split across two echoes; reach 260 cm, scaling with Reach.

### Shadow Shuriken (`ShadowShuriken`)

A shuriken ricochets through five distinct enemies.

Base: 16 direct damage per hit/pulse, 380 cm radius/width, 4s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.

1. **Returning Star** — After the last bounce, revisit the first target at half damage.
2. **Splitting Star** — Each bounce splashes nearby enemies for 30% damage.
3. **Toxic Star** — Bounces apply Poison.

**Synergy: Steel Ricochet.** Shadow Shuriken prepares surviving enemies for 6 seconds; the partner's hit chains bonus strikes through nearby enemies. Trigger with Samurai attacks or abilities. Follow-up damage is 90% of the preparing hit, per affected target; reach 420 cm, scaling with Reach.

### Smoke Lattice (`SmokeLattice`)

Lay down a Poison lattice that repeatedly pushes enemies out of its center.

Base: 6 direct damage per hit/pulse, 290 cm radius/width, 6.5s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.

1. **Drifting Smoke** — The lattice follows active Ninja before being left behind on swap.
2. **Double Veil** — Create a second lattice at another nearby position.
3. **Choking Finale** — The last pulse deals triple damage.

**Synergy: Hidden Blade.** Smoke Lattice prepares surviving enemies for 6 seconds; the partner's hit triggers a heavy single-target follow-up. Trigger with Samurai attacks or abilities. Follow-up damage is 300% of the preparing hit, per affected target; reach 200 cm, scaling with Reach.

### Thunder Wire (`ThunderWire`)

Stretch a damaging wire through a nearby enemy for three pulses.

Base: 14 direct damage per hit/pulse, 65 cm radius/width, 5s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.

1. **Cross Wire** — Lay a second wire perpendicular to the first.
2. **Live Wire** — Wire endpoints also emit half-damage bursts.
3. **Venom Cable** — Wire hits apply Poison.

**Synergy: Grounded Steel.** Thunder Wire prepares surviving enemies for 6 seconds; the partner's hit pulls nearby enemies toward the victim and damages them. Trigger with Samurai attacks or abilities. Follow-up damage is 100% of the preparing hit, per affected target; reach 360 cm, scaling with Reach.

### Phantom Ambush (`PhantomAmbush`)

Two delayed phantom slashes converge on the targeted crowd.

Base: 25 direct damage per hit/pulse, 280 cm radius/width, 5s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.

1. **Third Shadow** — Add a third phantom from the opposite side.
2. **Lingering Shadow** — Each phantom repeats its slash at half damage.
3. **Assassin's Ink** — Phantom slashes apply Poison.

**Synergy: Phantom Handoff.** Phantom Ambush prepares surviving enemies for 6 seconds; the partner's hit deals a bonus strike and reduces this family's remaining cooldown by one second. Trigger with Samurai attacks or abilities. Follow-up damage is 75% of the preparing hit, per affected target; reach 200 cm, scaling with Reach.

### Caltrop Trail (`CaltropTrail`)

Moving leaves armed caltrops behind you. Enemies stepping on them take damage and Poison.

Base: 12 direct damage per hit/pulse, 130 cm radius/width, 1.3s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.

1. **Forked Trail** — Leave two caltrop patches to either side.
2. **Persistent Spikes** — Each patch rearms once after triggering.
3. **Barbed Exit** — Triggered patches push enemies away.

**Synergy: Spiked Opening.** Caltrop Trail prepares surviving enemies for 6 seconds; the partner's hit triggers a status-spreading burst. Trigger with Samurai attacks or abilities. Follow-up damage is 160% of the preparing hit, per affected target; reach 230 cm, scaling with Reach.

### Crimson Needle (`CrimsonNeedle`)

Target the lowest-health enemy with a focused needle; bleeding targets take 50% extra damage.

Base: 40 direct damage per hit/pulse, 90 cm radius/width, 4s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.

1. **Twin Needles** — Strike the second-lowest-health target too.
2. **Needle Fan** — The impact splashes nearby enemies for half damage.
3. **Septic Needle** — Needles apply Poison.

**Synergy: Crimson Verdict.** Crimson Needle prepares surviving enemies for 6 seconds; the partner's hit triggers a heavy single-target follow-up. Trigger with Samurai attacks or abilities. Follow-up damage is 125% of the preparing hit, per affected target; reach 120 cm, scaling with Reach.

### Raven Swarm (`RavenSwarm`)

A raven swarm follows an enemy for six pecks, applying Poison.

Base: 8 direct damage per hit/pulse, 100 cm radius/width, 5.5s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.

1. **Carrion Flight** — Retarget a nearby victim when the current one dies.
2. **Murder of Crows** — Pecks splash nearby enemies for half damage.
3. **Last Cry** — The final peck emits a triple-damage burst.

**Synergy: Carrion Feast.** Raven Swarm prepares surviving enemies for 6 seconds; the partner's hit triggers a status-spreading burst. Trigger with Samurai attacks or abilities. Follow-up damage is 200% of the preparing hit, per affected target; reach 280 cm, scaling with Reach.

## Files and maintenance

- `Tools/build_family_catalog.json`: authored roster, card text, branch IDs and base tuning.
- `Tools/generate_build_catalog.py`: regenerates `Source/HeavensDivide/BuildFamilyCatalog.h` from that roster.
- `Tools/configure_build_families.py`: creates/updates the 160 family cards and controller pool; backups are under `Saved/Backups/BuildFamilies`.
- `SurvivorBuildFamilies.cpp`: traveling, orbiting, zone, mine, lane, flurry and targeted attack behavior plus shared partner reactions, using the existing controller ability component and its 10 Hz scheduler.
- `SurvivorAbilityComponent`: expanded existing families and delayed work; `AutoAttackComponent`, `SamuraiBladeWave` and `AttackProjectileBase`: actual attack-hit integration.
- `BuildFamilyTests.cpp`: saved-asset structure/gating, independent branch paths, actual damage, same/other-character reaction checks, duplicate-payout protection, lane misses, retargeting and cleanup.

Gameplay work is bounded at 96 expanded casts, 512 prepared-enemy records, 128 queried enemies and 64 live visual accents per controller. No global per-frame enemy scans or persistent audio components were added. Damage continues through EnemyBase, preserving rewards, loot and death handling.

Visuals use the existing rings, connecting streaks and native Blade Wave effect. Card artwork reuses existing character illustrations; bespoke art and Niagara effects were not created for all twenty families. This is a playable systems/content pass, with final balance and visual polish still needing playtesting.

Validation completed: Win64 Development Editor build succeeded. All seven automation suites passed: BuildFamilies, Expansion, SamuraiPushback, BasicMelee, GoblinBomb, EnemyDeath.Lifecycle and ImpactFeedback.CombatAndSettings. Asset creation verified all 160 family cards and the 194-entry deduplicated controller pool. Test report: Saved/Automation/BuildFamilies/index.json.
