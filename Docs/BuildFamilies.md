# Build families

Combat now uses normal autoattacks and **Blade Wave**, which launches from Samurai's normal melee attacks. The saved upgrade pool contains **57 unique cards**, including 20 Ninja build cards. Normal-attack modifiers, status upgrades, Tag Team, Grand Entrance and the other shared upgrades remain available.

The ten automatic ability families and their 70 starter, branch, scaling and evolution cards are retired. They cannot appear in offers, be acquired through stale references, or run through the old automatic cast scheduler.

## Samurai stances and routes

Three Rare, one-rank stance cards appear in normal Samurai upgrade offers. Acquiring one excludes the other two for the current run. Stances do not automatically grant Bleed, explosions or Blade Wave. All effect upgrades remain mixable across stances.

| Stance | Area / reach | Attack speed | Damage |
| --- | --- | --- | --- |
| Blood Stance (`BloodStance`) | +35% | +30% | -30% |
| Execution Stance (`ExecutionStance`) | +35% | -35% | +80% |
| Blade Wave Stance (`WaveStance`) | -35% | +25% | +25% |

These are multiplicative factors applied after additive stat upgrades. A 35% attack-speed penalty remains a 0.65 multiplier even after buying more attack speed. Area scales melee reach, melee presentation, wave width, and these routes' transfer/explosion radii. Wave travel distance is unchanged. Ninja stats are unaffected.

### Blood: fast crowd coverage

Start with **Bleeding Edge**. Normal melee and Blade Wave hits apply Bleed; each stack contributes its base Bleed damage plus **10% of its applying hit's damage per tick**. The hit contribution is captured when applied and already includes attack damage scaling. Deep Cuts and Bleed meta bonuses scale the resulting damage. Intrinsic assists and status spreads retain their base-stack behavior.

- **Bloodletting (`Bloodletting`)**: +1 Bleed stack per direct melee/wave hit per rank, up to 3 ranks. Requires Bleeding Edge.
- **Blood Transfer (`BloodTransfer`)**: on a bleeding enemy's death, distribute **50% of its remaining Bleed damage** evenly among up to **5** nearest valid enemies within **300 cm**, scaling with Samurai area. Requires Bleeding Edge. Works on deaths from direct hits, status ticks or other damage.
- **Lingering Wounds (`LingeringWounds`)**: newly applied Bleed lasts 15/22/30% longer per Common/Rare/Epic rank, up to 5 ranks.
- Continue scaling with **Deep Cuts**, **Quickened Cuts** and the existing Samurai area upgrade.

Transfers retain the source's remaining duration and add no free base-stack damage. They carry a finite damage budget, so damage/mastery bonuses are not applied twice and refreshing duration does not multiply that budget. Further deaths can transfer half the remaining budget again. A lethal Bleed tick is spent before transfer. Poison and source-restricted enemies are unaffected by this transfer.

### Execution: heavy killing blows

**Overkill Burst (`OverkillBurst`)** is a separate, mixable Rare starter. Direct normal melee and Blade Wave kills explode for their excess damage within **220 cm**, scaling with Samurai area. A hit dealing 150 damage to a 40-HP enemy creates a 110-damage explosion. Exact kills with no overkill create no explosion.

Assists, Bleed, explosion kills and other proc damage cannot start an explosion. Each directly killed melee target can produce its own burst. Explosion damage respects enemy source restrictions and does not apply Bleed or Prepare.

- **Expanding Ruin (`BurstRadius`)**: +12/18/25% explosion radius per Common/Rare/Epic rank, up to 5 ranks. Requires Overkill Burst.
- Continue scaling with **Heavy Blade**, Samurai area and **Double Cut**. Double Cut's committed follow-up is another normal attack and can create its own overkill burst.

### Blade Wave: narrow, frequent attacks

Unlock **Blade Wave** separately. Blade Wave Stance increases its inherited attack damage and frequency while reducing both melee reach and wave width. Dedicated width upgrades can compensate gradually.

**Wave Volley (`WaveMultishot`)** adds one additional wave per committed swing per rank, up to three ranks. Waves retain their normal damage, width and returning behavior. Ordinary volleys use an 8-degree spacing. Crossing Blades adds its extra crossing waves on every third swing: with all three Wave Volley ranks, ordinary swings launch 4 waves and crossing swings launch 6.

**Quickened Cuts (`SamuraiTempo`)** is a mixable Samurai attack-speed card: +10/15/22% per Common/Rare/Epic rank, up to 5 ranks. It supports Blood or Blade Wave, or partly offsets Execution's slower cadence without removing its multiplier.

## Blade Wave (`BladeWave`)

Samurai's basic melee attacks launch traveling blade waves through the existing attack notify path. The original seven family cards remain, alongside the new Wave Volley card:

- **Blade Wave:** unlock the attack-triggered wave.
- **Returning Blade:** waves return for another hit pass.
- **Crossing Blades:** every third swing launches three crossing waves, plus acquired Wave Volley waves.
- **Splinter Wave:** the first enemy hit on each wave pass sheds a small 30%-damage Bleed burst.
- **Force (`BladeWavePower`):** increase wave damage.
- **Wide Arc (`WideArc`):** increase wave width and damage.
- **Velocity (`BladeWaveHaste`):** increase wave travel speed; does not change basic attack frequency.

Branches combine and require Blade Wave. The three scaling cards have five ranks. Force grants 20/30/45%, Wide Arc 12/18/25%, and Velocity 10/15/22% at Common/Rare/Epic rarity; magnitudes add across ranks.

## Ninja weapon routes

Three Rare stance cards replace or modify Ninja's normal attack. Choose one per run; its branch cards require that stance. **Embedded Blades** and its upgrades work with every route.

| Stance | Attack behavior | Scaling |
| --- | --- | --- |
| Returning Fang (`ReturningFang`) | One homing blade hits, returns, and immediately relaunches. Close targets shorten the cycle. | Attack speed becomes travel speed. Each extra projectile adds 25% damage instead. |
| Barrage (`BarrageStance`) | Normal volleys with +65% attack speed, +2 projectiles and -40% damage. | Attack speed, projectile count and damage. |
| Great Shuriken (`GreatShuriken`) | A large spinning blade pierces crowds and can hit each enemy every 0.25 seconds. +50% damage, -50% attack speed, -55% projectile speed. | Damage and attack frequency; each extra projectile adds 12% blade size instead. |

### Returning Fang

- **Cutting Return:** the returning blade also damages enemies along its path.
- **Bloodhound:** hitting a bleeding or poisoned target boosts the next throw's travel speed by 50%; does not grant either status.
- **Relentless Fang:** repeated hits against the same enemy build up to 75% bonus damage.
- **Final Pursuit:** a killing hit seeks one additional nearby enemy before returning.

Swapping away retrieves the blade without relaunching. The ordinary attack timer does not control this weapon.

### Barrage

- **Focused Volley:** tighten the projectile fan.
- **Forking Projectiles:** on impact, a projectile forks into two shots dealing 50% damage each. Forked shots cannot fork again. Requires Barrage; clone volleys also fork.
- **Alternating Fans:** alternate the volley angle to sweep a wider region.
- **Crescendo:** gain another temporary projectile every three attacks. Fire the capped volley, then restart the buildup. Dashing and swapping preserve progress. Five ranks; the cap starts at 5 and increases by 2 per additional rank (5/7/9/11/13). Edit `BaseCap`, `CapPerRank` and `AttacksPerProjectile` on its data asset.
- **Needle Rain:** every fourth volley fires twice as many main-fan projectiles.

### Great Shuriken

- **Serrated Edge:** successive hits from the same blade against an enemy deal increasing damage, up to +75%.
- **Grinding Halt:** briefly slow the blade when it reaches a tougher enemy, allowing more contact hits.
- **Growing Shuriken (`WideOrbit`):** grow while travelling, reaching double size after 1,000 cm. Both visual size and hit radius grow; the normal flight lifetime is unchanged. The internal ID is retained for compatibility.
- **Breaking Wheel:** scatter six smaller blades when the shuriken expires.

### Embedded Blades

Direct Ninja projectile hits embed a fragment **before damage resolves**, so even a one-hit kill scatters a blade toward nearby enemies. Each fragment deals 40% of its embedding hit's damage. Surviving enemies can store up to 12 fragments. Scattered fragments cannot embed more fragments.

- **Fragment Damage:** +20/30/45% fragment damage per Common/Rare/Epic rank, up to 5 ranks.
- **Fragment Load:** embed one additional fragment per hit per rank, up to 3 ranks.
- **Fragment Reach:** +15/22/30% scatter targeting range per Common/Rare/Epic rank, up to 5 ranks; base range 700 cm.

The old Ninja cards are retired, except **Shadow Step**, **Multiple Strikes** and **Afterimage Frenzy**, which retain Shadow Clone and its upgrades. **Venomous Kunai** is restored as a standalone card: Ninja attacks apply Poison, including all three weapons and clone attacks. The other old poison upgrades remain retired. The removed cards cover the old projectile bonus, bounce/pierce/split, Fan of Blades, Blade Cascade, execution branches and poison scaling upgrades. Hemotoxic Reaction and Accelerated Venom are also retired alongside the retired poison scaling upgrades. Shared Marked Blade remains available. Ninja trial rewards now offer current Ninja cards for every stance. None of the new Ninja attacks applies universal Prepare.

## Prepare and Tag Team

Blade Wave hits prepare surviving enemies for **6 seconds**. Normal attack hits and assists do not apply Prepare. Further wave hits refresh the mark and stored hit damage.

The inactive Ninja's **Tag Team assist** consumes Samurai's preparation once, dealing **60% of the preparing hit's damage** as bonus damage to that enemy. It spreads one stack of each Bleed/Poison status already present to other enemies within **300 cm**, respecting status source restrictions. Absent statuses are never created. This still works when the assist kills the prepared enemy.

Active-character attacks, including attacks after swapping, cannot consume Prepare. Tag Team prioritizes prepared targets within its normal targeting limits. Bonus damage and status spread cannot recursively consume Prepare. Preparation and Reaction meta bonuses continue to affect duration and damage. Shared Prepare settings remain on the SurvivorAbility component.

## Shared synergy upgrade: Grand Entrance

**Grand Entrance** (`GrandEntrance`) is a one-rank Epic synergy card available through normal Synergy offers, without a family prerequisite or discovery lock. After a successful player-initiated swap, the incoming character's next normal attack is enhanced:

- **Samurai:** a 360-degree slash with a **600 cm base radius**, scaling with basic attack area. All secondary targets take full normal attack damage; the primary target retains its normal technique modifiers.
- **Ninja:** **8 additional projectiles** in a **100-degree fan**, retaining normal projectile damage and acquired projectile modifiers. Extra projectiles combine with the normal volley.

For Returning Fang, Grand Entrance's extra projectiles become bonus damage on the next throw. For Great Shuriken, they become extra size on the next blade. Barrage retains the enhanced normal volley.

The enhancement is spent at the attack's hit/projectile notify. Canceled attacks before that point retain it. It does not stack, and leaving the active character clears an unused enhancement. Assists, automatic abilities and Double Cut follow-ups cannot spend it. Acquiring the card does not immediately arm it: swap after acquisition. The enhanced normal attack does not itself apply universal Prepare.

Balance is editable on `/Game/HeavensDivide/Upgrades/Synergy/DA_Synergy_GrandEntrance` under Runtime Balance: `SamuraiRadius`, `NinjaBonusProjectiles`, and `NinjaFanAngle`. The source illustration and exact built-in imagegen prompt are recorded in `Art/UpgradeCards/GrandEntrance.json`.

## Preview and maintenance

In a non-shipping build, `BuildPreview BladeWave` grants its starter, all branches and rank-2 scaling. `BuildPreview BladeWave 1` selects only its first branch; use 2 or 3 for the others. `BuildPreview All` and the legacy `AbilityShowcase` command now preview only Blade Wave. These commands affect the current run only.

- `Tools/build_family_catalog.json`: Blade Wave is the only available family. Retired entries retain their stable internal indices for compatibility and stale-ID rejection.
- `Tools/generate_build_catalog.py`: regenerates the runtime catalog.
- `Tools/configure_build_families.py`: authors only available families.
- `Tools/remove_automatic_ability_upgrades.py`: removes retired saved assets with backups; `-ValidateAttackRoster` verifies the saved pool without editing.
- `Art/UpgradeCards/manifest.json`: the seven retained family illustrations. Grand Entrance keeps its separate art manifest.

Normal-attack execution and Blade Wave still use `AutoAttackComponent` and `SamuraiBladeWave`. `SurvivorAbilityComponent` retains assist, Prepare, presentation and Blade Wave hit support; automatic casts are disabled.

## Samurai build authoring

- `Tools/samurai_build_upgrades.json`: stance values, seven new mixable cards, prerequisites and scaling.
- `Tools/configure_samurai_builds.py`: authors those ten cards and updates the saved controller pool; use `-ValidateSamuraiBuilds` for read-only validation.
- New cards reuse matching Samurai illustrations from the existing CardArt2 set.
- `SamuraiBuildUpgrades.cpp`: finite Bleed transfer and direct-hit overkill explosions. Both use bounded nearby queries and existing combat ring visuals.
- `HeavensDivide.Combat.SamuraiBuilds`: verifies saved cards, stance exclusivity/scaling/restoration, Bleed transfer budgets, lethal ticks, real melee explosions and actual wave spawns.

These are starting balance values and need playtesting against the current enemy waves.

## Ninja testing and authoring

During a run in a non-shipping build, switch to Ninja and enter one of these console commands:

- `NinjaBuildPreview Fang`
- `NinjaBuildPreview Barrage`
- `NinjaBuildPreview Shuriken`
- `NinjaBuildPreview Clear`

Each route preview grants its stance, its branch cards and the Embedded Blades package at one rank. Switching previews replaces the new Ninja build cards while preserving other acquired upgrades. Clear removes the new Ninja cards. These commands affect only the current run.

`Tools/ninja_build_upgrades.json` defines the 20 cards. `Tools/configure_ninja_builds.py` creates missing cards and updates the controller pool while preserving existing card tuning; `-ValidateNinjaBuilds` validates without writing. Cards reuse existing Ninja illustrations. Fang and scattered kunai reuse the original Ninja projectile Blueprint visuals, trails and impact sound. The giant shuriken retains its spinning mesh visual.

`NinjaBuildComponent` implements the weapons and fragments. `HeavensDivide.Combat.NinjaBuilds` exercises the saved Ninja Blueprint, real volley spawning, timer-independent Fang returns, repeated shuriken hits, growth with distance, lethal-hit fragments, stance exclusivity and preview switching.

## Legacy Samurai cleanup

The old Cleaver, Duelist and Deathblow technique cards are removed. Samurai trial rewards now offer eligible upgrades from the current Samurai build pool. The old technique effects cannot activate from stale run data.

The 22 retained Samurai cards are the ten route cards; Bleeding Edge, Deep Cuts, Heavy Blade, Area and Double Cut; and the seven original Blade Wave cards. Shared synergies and Ninja/global upgrades remain available. Existing tuned card assets are preserved without rewriting their values.

`Tools/remove_legacy_samurai_techniques.py` removes the three legacy assets and verifies retained card file hashes. Backups and hashes are under `Saved/Backups/LegacySamuraiCleanup`. Their shared artwork is retained because current build cards reuse it.

`Tools/remove_legacy_ninja_upgrades.py` removes the 12 retired cards and changes Wide Orbit into Growing Shuriken, preserving all other saved card tuning and Shadow Clone artwork. Backups are under `Saved/Backups/LegacyNinjaCleanup`; `-ValidateNinjaCleanup` checks the saved roster without writing. Growth distance and maximum size multiplier are editable on the Growing Shuriken card under Runtime Balance.

## Ninja clone attacks and weapon speed

Shadow Clones use the Ninja's current weapon: Barrage/normal projectiles, Great Shuriken, or an independent Returning Fang that returns to the clone. Multiple Strikes increases the attack quota. A Fang round trip counts as one attack and immediately relaunches while attacks remain; Afterimage Frenzy increases its travel speed. Barrage counters belong to each clone. Clone attacks do not spend Grand Entrance or advance the player's attack counters.

Open `/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaGreatShuriken` and edit **Runtime Balance > Balance Parameters > TravelSpeed** (cm/s, default 900). This directly controls player and clone shuriken speed, independently of normal kunai speed. Grinding Halt can still briefly slow a blade. The stance's old projectile-speed stat modifier no longer determines giant shuriken travel speed.

`Tools/restore_ninja_poison.py` restores the original poison starter from the cleanup backup and re-adds it to the saved pool, preserving other card tuning.

`Tools/update_barrage_upgrades.py` replaces Crossfire (both side and rear shots) with Forking Projectiles and updates Crescendo. Other saved card tuning is preserved.
