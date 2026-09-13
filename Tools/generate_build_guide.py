import json
from pathlib import Path
rows=json.loads(Path('Tools/build_family_catalog.json').read_text())
s='''# Build families

This roster contains **six Samurai and five Ninja families**. Each has one starter, three combinable behavior branches, three five-rank scalable cards: **77 family cards total**, including upgraded existing cards. The controller pool contains 112 unique cards with the unrelated earlier upgrades retained.

## Current roster

| Samurai (6) | Ninja (5) |
| --- | --- |
| Steel Tempest | Night Thread |
| Heavenfall | Venom Garden |
| Blade Wave | Phantom Ambush |
| Iron Orbit | Crimson Needle |
| War Banner | Raven Swarm |
| Blood Moon | |

Only these families are available through the upgrade pool and `BuildPreview`.

## Start here

Start a fresh run, then use the Unreal console:

- `BuildPreview IronOrbit` grants that starter, all three branches, rank 2 scaling. Universal Prepare is always available.
- `BuildPreview IronOrbit 1` grants only its first branch (plus the starter and scaling). Use 2 or 3 for another branch. This adds to the current run; it does not remove already acquired branches.
- Repeat the command with a partner family. Example: `BuildPreview PhantomAmbush`.
- `BuildPreview All` is available for a saturation/stress test; an individual pair is better for assessing the combat feel.
- The older `AbilityShowcase` still previews its original four builds and setup assists.

These commands are disabled in shipping builds. They use normal run-local acquisition, preserve higher ranks, and repeat safely without adding mastery for already owned cards. No persistent meta unlocks are changed.

Suggested pairings:

| Samurai | Ninja | Playstyle |
| --- | --- | --- |
| War Banner | Crimson Needle | Mark a crowd, swap to Ninja and pick off weakened enemies. Native Ninja projectiles can consume intrinsic marks without needing Marked Blade. |
| Blood Moon | Raven Swarm | Spread Bleed, return to Ninja for persistent Poison and status-spreading follow-ups. |

## Rules

Only the active character starts and recharges automatic abilities. Already-cast attacks continue with their original damage attribution across swaps. Following fields/orbits follow their owner only while that character is active; they remain at the last position after a swap. Blade Wave continues to use the real melee attack/notify path.

Branches combine. Most require their starter and are fixed Rare cards. The four existing legendary evolutions (Razor Halo, Starfall, Black Web, Withering Garden) occupy the first branch slot of their respective families and retain the rank-2 Force/Reach requirement. Character offers reserve a starter, scaling card and branch/evolution when eligible; missing categories fall back to normal random eligible choices.

Force increases direct family damage; Reach increases its attack footprint, chain reach or acquisition range; Rhythm increases recharge speed. Blade Wave instead has Force, Wide Arc (its existing width + damage scaling), and Velocity (projectile travel speed). Force and Reach use Common/Rare/Epic values of 20/30/45% and 12/18/25%; Rhythm/Velocity uses 10/15/22%. Magnitudes add across ranks. Recharge = base cooldown / (1 + total Rhythm). Status damage continues to use the existing Bleed/Poison upgrades and mastery.

Universal **Prepare** is available automatically, without a synergy card. A successful ability hit prepares a surviving enemy for **6 seconds**. Normal attacks and Tag Team setup attacks do not apply Prepare. Blade Wave is an ability even though a melee attack launches it; its wave hits can prepare enemies.

Each enemy holds one preparation. Another ability from the same character refreshes its duration and stores that hit's damage. An opposite-character ability does not overwrite an unexpired preparation. The preparing character's own hits cannot consume it, and swapping alone does nothing.

The inactive partner's **Tag Team assist hit** consumes Prepare once. Active-character hits, including hits after a swap, do not consume it. Consumption deals **60% of the preparing ability hit's damage** as bonus damage to the prepared enemy, attributed to the assisting character. It also spreads **one stack of each Bleed/Poison status already present on that enemy to other enemies within 300 cm**. Both spread if both are present; neither is created if absent. The original statuses remain. Spread uses each status's normal source restrictions and scaling, and still works when the assist kills the prepared enemy.

Bonus damage and status spread cannot prepare enemies or trigger another consumption. There is no family trigger cooldown or re-prime lockout: a later ability hit can prepare the enemy again. Lingering Intent and Unbroken Promise extend the duration; Answered Challenge and Converging Blades increase the bonus damage. Family Reach does not change the shared spread radius.

Tag Team prioritizes prepared enemies the assistant can consume, both for its initial target and its limited hit list. Normal targeting applies when none qualify; range, facing, hit limits and placement checks still apply. The universal duration, damage multiplier and spread radius are editable on the SurvivorAbility component under **Abilities > Prepare** on `BP_SurvivorPlayerController`.

Venom Garden retains its separate baseline melee detonation. Other existing synergies such as Tag Team and Hemotoxic Reaction retain their own unlocks and behavior.

'''
s+='''## Shared synergy upgrade: Grand Entrance

**Grand Entrance** (`GrandEntrance`) is a one-rank Epic synergy card available through normal Synergy offers, without a family prerequisite or discovery lock. After a successful player-initiated swap, the incoming character's next normal attack is enhanced:

- **Samurai:** a 360-degree slash with a **600 cm base radius**, scaling with basic attack area. All secondary targets take full normal attack damage; the primary target retains its normal technique modifiers.
- **Ninja:** **8 additional projectiles** in a **100-degree fan**, retaining normal projectile damage and acquired projectile modifiers. Extra projectiles combine with the normal volley and Blade Cascade.

The enhancement is spent at the attack's hit/projectile notify. Canceled attacks before that point retain it. It does not stack, and leaving the active character clears an unused enhancement. Assists, automatic abilities and Double Cut follow-ups cannot spend it. Acquiring the card does not immediately arm it: swap after acquisition. The enhanced normal attack does not itself apply universal Prepare.

Balance is editable on `/Game/HeavensDivide/Upgrades/Synergy/DA_Synergy_GrandEntrance` under Runtime Balance: `SamuraiRadius`, `NinjaBonusProjectiles`, and `NinjaFanAngle`. The source illustration and exact built-in imagegen prompt are recorded in `Art/UpgradeCards/GrandEntrance.json`.

'''
for owner in ['Samurai','Ninja']:
 s+='## '+owner+'\n\n'
 for r in [r for r in rows if r["owner"]==owner]:
  s+='### '+r['name']+' (`'+r['id']+'`)\n\n'+r['description']+'\n\n'
  if r['kind']!='Legacy':s+=f"Base: {r['damage']} direct damage per hit/pulse, {r['radius']} cm radius/width, {r['cooldown']}s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.\n\n"
  for i,b in enumerate(r['branches']):s+=f"{i+1}. **{b['name']}** : {b['description']}\n"
  s+='\n'
s+='''## Files and maintenance

- `Tools/build_family_catalog.json`: authored roster, card text, branch IDs and base tuning.
- `Tools/generate_build_catalog.py`: regenerates `Source/HeavensDivide/BuildFamilyCatalog.h` from that roster.
- `Tools/configure_build_families.py`: creates/updates the 77 family cards and controller pool; backups are under `Saved/Backups/BuildFamilies`.
- `SurvivorBuildFamilies.cpp`: orbiting, banner, expanding-ring, phantom, needle and swarm attack behavior plus universal Prepare, using the existing controller ability component and its 10 Hz scheduler.
- `SurvivorAbilityComponent`: expanded existing families and delayed work; `AutoAttackComponent`, `SamuraiBladeWave` and `AttackProjectileBase`: actual attack-hit integration.
- `BuildFamilyTests.cpp`: saved-asset structure/gating, independent branch paths, actual damage, universal Prepare, existing-status spread, assist-only consumption, duplicate-payout protection, line misses, Raven Swarm retargeting and cleanup.

Gameplay work is bounded at 96 expanded casts, 512 prepared-enemy records, 128 queried enemies and 64 live visual accents per controller. No global per-frame enemy scans or persistent audio components were added. Damage continues through EnemyBase, preserving rewards, loot and death handling.

Visuals use the existing rings, connecting streaks and native Blade Wave effect. All 77 family cards have dedicated generated illustrations assigned; see `Docs/UpgradeArtwork.md`. Combat VFX use the existing presentation system and per-upgrade settings. Final balance and visual polish still need playtesting.
'''
Path('Docs/BuildFamilies.md').write_text(s,encoding='utf-8')

