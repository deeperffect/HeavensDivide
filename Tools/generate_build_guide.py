import json
from pathlib import Path
rows=json.loads(Path('Tools/build_family_catalog.json').read_text())
s='''# Twenty build families

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

'''
for owner in ['Samurai','Ninja']:
 s+='## '+owner+'\n\n'
 for r in [r for r in rows if r["owner"]==owner]:
  s+='### '+r['name']+' (`'+r['id']+'`)\n\n'+r['description']+'\n\n'
  if r['kind']!='Legacy':s+=f"Base: {r['damage']} direct damage per hit/pulse, {r['radius']} cm radius/width, {r['cooldown']}s recharge. Repeated attacks deal that damage each time unless a branch specifies a fraction.\n\n"
  for i,b in enumerate(r['branches']):s+=f"{i+1}. **{b['name']}** — {b['description']}\n"
  y=r['synergy'];s+='\n**Synergy: '+y['name']+'.** '+y['description']+f" Follow-up damage is {y['factor']*100:g}% of the preparing hit, "+('split across two echoes' if y['kind']=='Echo' else 'per affected target')+f"; reach {y['radius']} cm, scaling with Reach.\n\n"
s+='''## Files and maintenance

- `Tools/build_family_catalog.json`: authored roster, card text, branch IDs and base tuning.
- `Tools/generate_build_catalog.py`: regenerates `Source/HeavensDivide/BuildFamilyCatalog.h` from that roster.
- `Tools/configure_build_families.py`: creates/updates the 160 family cards and controller pool; backups are under `Saved/Backups/BuildFamilies`.
- `SurvivorBuildFamilies.cpp`: traveling, orbiting, zone, mine, lane, flurry and targeted attack behavior plus shared partner reactions, using the existing controller ability component and its 10 Hz scheduler.
- `SurvivorAbilityComponent`: expanded existing families and delayed work; `AutoAttackComponent`, `SamuraiBladeWave` and `AttackProjectileBase`: actual attack-hit integration.
- `BuildFamilyTests.cpp`: saved-asset structure/gating, independent branch paths, actual damage, same/other-character reaction checks, duplicate-payout protection, lane misses, retargeting and cleanup.

Gameplay work is bounded at 96 expanded casts, 512 prepared-enemy records, 128 queried enemies and 64 live visual accents per controller. No global per-frame enemy scans or persistent audio components were added. Damage continues through EnemyBase, preserving rewards, loot and death handling.

Visuals use the existing rings, connecting streaks and native Blade Wave effect. Card artwork reuses existing character illustrations; bespoke art and Niagara effects were not created for all twenty families. This is a playable systems/content pass, with final balance and visual polish still needing playtesting.
'''
Path('Docs/BuildFamilies.md').write_text(s,encoding='utf-8')

