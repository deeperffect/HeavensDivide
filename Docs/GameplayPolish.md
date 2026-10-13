# Gameplay polish — October 2026

## Ground

The current reference pass places complete olive/khaki painted slabs over the
Level 1 dirt and grass floor, avoiding the earlier surface mask cutting through
slab silhouettes. Artwork was created with built-in imagegen from the user's
screenshot reference. Sources and prompts are documented in
`Art/ReferenceGround/README.md`, `Prompts.md` and `RockStamps_Prompt.md`.
`RockStamps_Atlas.png` contains four transparent variants; `RockAtlasRegions.json`
provides their cropped UV rectangles and coverage. `RockScatterLayout.json`
records the generated placements, counts and geometric checks: 65 pieces in
20 clusters, comprising 23 large slabs and 42 companions. Large pieces span
1,025-1,444 cm; companions use a 420-700 cm range with at most three per cluster.
Each variant retains its crop proportions with 0.9-1.1 aspect variation.

`Tools/generate_ground_rock_scatter.py` reads `Tools/ground_rock_scatter.json`.
`Tools/apply_ground_rock_scatter.py` imports `T_RockStamps_Atlas`, `M_RockStamp`
and `MI_RockStamp_0` through `MI_RockStamp_3`, then saves one `GroundRockStamps`
actor with four static HISM components. Its flat masked planes sit at world
Z 1.3 cm over the floor at Z 1 cm, with no collision, navigation influence,
shadows or ticks. The surfaces remain lit and mipmapped; material tint
compensates for the existing warm lighting without modifying source pixels.

On `MI_Lvl1_Ground`, `PlacedRockStamps=1` replaces the repeated stone layer with
soil beneath the placed slabs. `0` restores the legacy tiled floor; hide the
slab actor too for a full comparison. Mask weights and quad coverage are not
visible stone coverage, because the atlas also contains transparent space.
The quad footprints cover 34.18% of the arena; estimated opaque rock coverage
is 19.90%.

`Tools/review_ground_rock_scatter.py` captures six views with normal texture
streaming, including low-angle and static telegraph readability probes.
Backups, reports and captures go to `Saved/GroundRockScatter`. These probes do
not constitute gameplay validation, and no measured FPS result is claimed.

Saved-level verification passed for all 65 slabs and six rendered views. The
native atlas material and per-component streaming scale retain 2048x2048 detail
with ordinary streaming enabled; attack-circle probes remain above the slabs.
Evidence: `Saved/GroundRockScatter/ValidationSummary.json` and `FinalClean/`.

The previous material-only pipeline is retained through
`Tools/apply_reference_ground.py` and `Tools/review_reference_ground.py`, with
evidence under `Saved/GroundReferencePass`. Floor rebuilds preserve
`PlacedRockStamps`.
Shared original textures, boss-arena materials and encounter-wall materials
retain their original references.

## Enemy pressure

### Pickup density - October 12 follow-up

Normal enemy XP rewards now combine into nearby resting pickups within **500 cm**,
up to **four enemy rewards per pickup**. Each death contributes its complete
authored integer XP immediately. For example, four 3-XP kills leave one 12-XP
pickup instead of four 3-XP pickups. Mixed rewards such as 3 + 5 + 12 + 2 become
one 22-XP pickup. A partial group remains collectible immediately; there is no
random XP drop roll or queued remainder. Each death makes at most one new orb,
even when its reward previously needed multiple 3-XP orbs. In a dense cluster of
uncollected kills, grouping reduces pickup count by up to **75%**. Claimed,
magnet-attracted and airborne chest orbs cannot receive new enemy rewards;
isolated kills still create a local pickup. Collection location and timing can
change with grouping, but collecting the complete reward set gives the same XP
and level progression. No frame-rate or full-run FPS result is claimed.

`ExperienceDropSubsystem` indexes resting drops in 500-cm cells, checks only
nearby cells on death, and uses no tick or scan of all actors. Enemy XP values,
Bloodbound no-XP rules and the level curve are retained. `ExperiencePerPickup`
remains for saved Blueprint compatibility; it no longer splits normal deaths.
Tune grouping under **Project Settings > Game > Experience Pickup Drops** or
`DefaultGame.ini`. Set `RewardsPerPickup=1` to disable combining, or adjust
`MergeRadius`.

Elite chest showers use **12 orbs instead of 40**, retaining the rolled reward.
For total XP X and N = min(X, 12), each orb gets floor(X/N), and exactly X mod N
orbs get one additional XP. Thus the sum is exactly X without rounding loss.
Chest tiers, jackpot multipliers, currencies, card rewards and reveal timing
retain their existing behavior.

Enemy healing base chances are **0.75% normal**, **10% elite**, **50% boss**
(each half its previous value). The shared successful-drop cooldown increases
from **20 to 30 seconds**. Low-health/pity multipliers, full-health exclusion
and the two-active-pickup cap remain; assistance can still reach its chance cap.
Healing urns first appear after **36 seconds**, then every **76-110 seconds**
(previously 18 and 38-55). Magnets first appear after **150 seconds**, then every
**200-280 seconds** (previously 75 and 100-140). These doubled schedules roughly
halve long-run spawn opportunities. Existing urn rewards remain guaranteed when
broken; only their appearance frequency changes. Blueprint and placed director
overrides are applied by `Tools/apply_pickup_reduction.py`, which updates the
reset baseline and verifies saved levels after reload. Evidence is under
`Saved/PickupReduction/`.

The Development Editor build and six selected automation tests passed: XP reward
grouping, XP motion, magnets, elite chests, encounter mechanics and boss spell
pressure. The grouping fixture verifies exact XP after every kill, mixed and
partial groups, 32 kills becoming eight pickups without losing any of 96 XP,
matching level progression, floor placement, character swaps and collecting
each reward only once. `Saved/PickupReduction/ValidationSummary.json` records
the results; this is native/editor validation rather than a packaged playtest.

- Marching grunts move at **2.5 × min(330, 235 + 8 × minutes) cm/s**, previously 235–330 cm/s. The 1.4-second warning is retained.
- Marches choose a random approach angle and retry up to 24 whole formations. Each member must pass the normal ground, navigation, collision and arena checks. Members are never compressed or clamped onto the boundary. Marchers disappear when their next movement step would leave the arena.
- Maintained enemies choose among eight directions, weighted toward less crowded sectors. Sector counts are cached for one second, with each selected spawn direction reserved in the cache; this avoids scanning the whole crowd for every spawn. Normal spawn distance, placement rules, population caps and threat schedules still apply. Distant eligible grunts can be recycled six per evaluation; nearby enemies are not teleported.
- Two thirds of pursuers can aim up to 0.65 seconds ahead of player movement when farther than 350 cm away. Prediction uses ordinary swept movement and the existing obstacle fallback, without an additional route collision query. Close attacks and committed charges retain their existing aiming rules.
- Ordinary melee autoattacks no longer draw an AoE charging circle. Elite slams, bombs, lanes and other special attacks retain warnings.

## Breakable wall trap

One attempt window is 2–3 minutes, the other 5–6 minutes. The initial random trigger is in the first 45 seconds of each window, leaving time for placement retries. Failed placement and other active objectives defer the event, including beyond the window, rather than discarding it. Ground checks retry past transient characters so enemies cannot hide the floor.

Fifty-one segments form a ring 1,600 cm from the player (previously 800 cm). Spawning the ring shows the paused smoke intro "Survive or break out.. if you can..". After it clears, a 1.5-second warning precedes a clockwise rock-rise wave: six Japanese Shrine pond/ground rock meshes alternate around the ring with varied height and yaw. The wave travels around the ring in 0.75 seconds; each rock rises over 0.22 seconds (0.97 seconds total). Rocks retain their authored materials and embed their bases in the floor. Each segment becomes blocking when its rise starts; occupied segments wait rather than rising through the player. The orange ring is hidden. `WallWaveSeconds` and `WallRiseSeconds` tune the reveal on `SurvivalFieldObjective`; `RockMeshes` lives on `EncounterWall`. The wall will not rise through the player. Each segment has **60 HP**, accepts both characters' attack damage, and gives no XP or kill reward. Destroying a segment immediately removes its collision. All remaining segments disappear after **20 seconds** of eligible gameplay, measured after the intro and including the warning. Player death or level travel cleans up the event. Menus and other objectives suspend it. The old floating wall instructions are hidden.

Rock-wall follow-up: the Development Editor build and four focused regression checks pass (`FieldObjectives`, `ObjectiveIntro`, `Mechanics`, `RunChallenges`). The actual Level 1 rendered probe shows 7, 34 and then all 51 rocks as the wave progresses, using all six meshes. Indicator visibility, player overlap protection, immediate break-through and the full 20-second lifetime are covered by the fixtures. Evidence: `Saved/WallWave/ValidationSummary.json` and `Saved/WallWave/Previews/`.

The HP is an initial escape budget of about two seconds at 30 single-target DPS. It is fixed rather than scaled by run time. The warning also lets an attentive player escape before closure.

## Ember circle

A nearby circle appears around 1:15, with another opportunity every three minutes. Interact with its central shrine mesh to begin **20 seconds accumulated inside a 1,200 cm radius** (1.5 times its previous radius). Leaving drains progress at one second per second, clamped at zero. Menus and the activation intro neither advance nor drain it. The opportunity expires after 105 seconds of eligible gameplay time; activation resets that lifetime.

Completion awards **XP equal to twice the current next-level requirement, plus 20 Soul Embers**. That is an XP cache, not a guarantee of exactly two levels. Completion commits before opening any resulting upgrade menus, so it cannot pay twice. Ember saving uses the existing pending-reward retry path. Balance Lab does not grant permanent Embers.

The central mesh and minimap marker identify the unactivated objective. Its blue progress ring stays hidden until the player interacts with the central mesh. Yellow fills the ring clockwise as progress builds, with zero progress all blue and full progress all yellow; leaving makes the yellow portion retreat. There is no floating instruction, progress or completion text. An unactivated Ember circle does not suppress wall traps; activation claims the objective until completion or expiry.

The circle and Blood Shrine share a paused UI intro: smoky left-to-right text reveal, a two-second reading hold, then a left-to-right dissolve. The circle says "Survive inside for 20 seconds". Blood Shrine says "You've enraged the spirits. Kill 30 within 30 seconds"; only Bloodbound kills count, one per enemy. Its live `number/30` counter stays at the top center of the UI with the countdown below. Both challenges begin after the text disappears, preserving their full duration. See [Combat architecture](CombatArchitecture.md) for implementation and saved Blueprint setup.

Objective spawns also show a three-second popup and a directional arrow beside the player, using the existing `MarkersPointersV1` arrow art. These notices never pause gameplay. Arrows hide at nearby interactables and clear with completed/removed objectives. Active circles point you back when you leave; wall events point toward the ring while you are enclosed. Notices wait until any paused intro finishes before appearing.

## Swap and simulation

Saved Ninja/Samurai settings: entrance montage rate 1.35×, departure rate 1.5×, freeze 0.62 seconds with 0.08 seconds of ease-in. Gameplay still waits for the complete accelerated entrance, including weapon draw timing.

Departures add character-specific bursts and a short cosmetic dash with up to four fading pose trails. The copy does not move or collide with the actual gameplay character. Its cloth simulation is disabled because the pose is evaluated manually.

The Ninja smoke material's missing shape reference was also repaired. Arrival and departure smoke now use soft procedural puffs with transparent edges. Samurai retains the red ink-slash burst. Walls now use the six rocks' authored pond/ground materials; the legacy cube stone material remains available for other references.

Upgrade menus suspend the gameplay meshes and cloth, preserving their previous states. Closing the menu resets hair dynamics and cloth before restoring their ticks. Cosmetic departures also stop progressing while the upgrade selection is open.

## Quick editor checks

1. Play Level 1. Eject with F8 and select its Encounter Director in the Outliner.
2. Under **Encounters → Testing**, use **Spawn Wall Trap (PIE)** or **Spawn Ember Circle (PIE)**. Return to the player with F8. Unsafe placement or an existing conflicting event prevents spawning.
3. Break walls with each stance; verify the gap is passable. Interact with the circle's mesh, watch the paused intro, then leave and re-enter the circle; verify progress/yellow fill drains outside, grows inside, and pays a single reward.
4. Open several upgrade choices immediately after swapping Ninja, including while her ribbons are moving.
5. Circle a crowd and watch enemies approach through less populated sectors and cut across the route. Tune after a real run; the new behavior intentionally makes stationary damage output more valuable.

Automated regression reports and asset reports are written to `Saved/GameplayPolish`.

Verification: the editor build succeeded; 19 distinct regression checks passed, including rendered upgrade-menu/particle checks. The field-objective check was repeated successfully after the wall refinement. The saved Level 1 material, full 16-segment ring placement, Ember circle and both departure bursts were reviewed in rendered captures under `Saved/GameplayPolish/Previews`.

Follow-up validation: the enlarged circle (800 cm radius), 0.62-second swap freeze, deferred wall scheduling and cheaper crowd steering build successfully. All six selected field-objective/spawn/swap tests pass. The Level 1 runtime probe spawned 16 visible, sealed wall segments; updated wall/circle captures were inspected. Evidence: `Saved/GameplayPolishFollowup/Verification.json`. CPU work was reduced by removing prediction sweeps and caching sector counts; FPS on slower hardware has not been benchmarked.

## Veshra difficulty tuning (formerly Kagutsu)

Veshra's spell interval is now 3.3 seconds in phase one and 2.5 seconds below half health (previously 5.2 and 4.1). This produces about 58% and 64% more casts. Falling ember, cursed-ground, summon and spiral blast radii are 60% larger; nova radius is 1,300 cm and eclipse radius is 900 cm. Lance lanes are 1,700 x 220 cm; eclipse lanes are 1,800 x 180 cm. The larger circles cover 2.56 times their previous area.

Damage, HP, warning durations, active durations, safe ring centers and hazard/summon counts retain their previous values. The saved Blueprint and placed boss in Lvl_B1_Arena both use the new tuning. Source values are in Tools/boss_pressure_tuning.json; apply with Unreal Python through Tools/apply_boss_challenge.py. The encounter generator and editor reset baseline follow these values. Backups and before/after damage checks are under Saved/BossChallenge.

Boss tuning validation: saved arena reload and damage preservation checks passed. Existing encounter mechanics, attack indicator and fixed-health automation tests all pass (Saved/BossChallenge/Verification.json).

## Bear material repair

Enemy_Bear_Mat passed vertex-only PreSkinnedPosition directly to its pixel palette shader, causing an SM6 compile failure and fallback to the default material. A VertexInterpolator now transfers that position correctly. The original patchwork palette, textures and dissolve remain intact; the shared War Drummer material benefits too. The generation and verification scripts follow the repaired connection. Saved-asset checks and two animated rendered views pass, with no bear shader fallback errors (Saved/BearMaterial/Verification.json).

## Boss arena grounding

The arena Plane used BlockAll but ignored the custom EnemyGround trace channel, so floor projection failed for boss warnings and summoned enemies. Tools/fix_arena_ground.py restores the floor response and aligns the placed boss to its scaled capsule height. CombatGround::FindSurface now ignores characters and volumes when projecting indicators, summons and loot. XP drops use floor Z + 18 cm; chest bases use +3 cm; healing drops use their existing configured floor-relative hover. This work runs on spawning and dropping, rather than on every movement tick.

Arena grounding validation: Development Editor build passes. All six relevant regressions pass; the mechanics fixture covers character-blocked floor projection, enemy feet, floor-relative XP and initialized summons. The actual saved arena probe verified 24 ground warnings, two grounded guardians and seven ground-relative loot drops. Floor collision persists after reload. Evidence: Saved/ArenaGroundFix/Verification.json, MapProbeFinal.log and AutomationFinal/index.json.

## Arena objects and minimap follow-up

Successful blood shrines immediately hide and disable collision. Their existing delayed reward request runs before the actor is destroyed, and completed markers disappear immediately. Healing pickups now have green minimap markers; magnets have blue markers. Both unregister on removal. Grunt marches no longer display a ground warning lane.

EnemySpawnArea is the shared placement boundary; MinimapBounds is the fallback in maps without one. Level 1 minimap bounds now match its combat spawn area. The boss arena now has spawn and minimap bounds inset 100 cm from its floor edges. Urns use normal encounter ground, navigation and collision validation. Shrines, circles and wall rings must fit inside the area. Scattered XP, healing drops and chest rewards clamp inside the edge; boss summons and spell footprints also respect bounds. Bounds work runs when spawning or dropping rather than in enemy movement ticks.

The wall trap now uses 51 segments to preserve a closed ring at a 1,600 cm radius. Segments keep 60 HP and the 1.5-second warning; the full event expires after 20 seconds. Apply saved map bounds with Tools/configure_encounter_object_bounds.py; map backups, reload checks and verification reports are under Saved/EncounterObjects.

Validation: Development Editor build and all seven selected gameplay/UI checks pass. The saved Level 1 probe produced 26 visible, sealed walls, eight valid urns across four arena corners, and no march ground warning. The boss arena retained 24 grounded warnings, two grounded guardians and seven ground-relative loot drops. Both pickup Blueprints expose visible markers, and rendered green/blue minimap markers were inspected. Evidence: Saved/EncounterObjects/Verification.json.

Earlier wall/pressure validation: the Development Editor build and all four selected wall/spawn/fixed-health tests passed. The real Level 1 probe spawned 51 visible, sealed walls at the larger radius, then removed every segment after the previous 10-second lifetime. Fresh-process reload verified all 21 enemy HP values and all 12 phases on both the spawner Blueprint and placed Level 1 spawner. Damage and XP rewards were preserved. Evidence: Saved/WallPressureFollowup/Verification.json. The later loadout revision extends the lifetime to 20 seconds; `HeavensDivide.Encounters.FieldObjectives` verifies survival beyond 10 seconds and complete segment removal after 20 (`Saved/LoadoutRevision/Automation/index.json`).

## Scorpion boss: Veshra — The Pale Stinger

The supplied scorpion replaces the arena boss's humanoid mesh and animations.
Its 38-bone rig articulates all six legs, both claws/pincer fingers, body/head and
tail. Every vertex is weighted, with rigid armor and smooth connected joints.
The original texture/UVs remain intact. The mesh has three LODs and one material
section per LOD; it uses bone animation without cloth or runtime IK.

The new clips are Idle (2.4 seconds), tripod Walk (0.9 seconds), casting wobble
(1.8 seconds) and collapsing Death (2.8 seconds). Actual lightweight-movement
velocity drives locomotion; full-body montages drive casting and death. Victory
still records and stops combat immediately, but the victory screen waits for the
death montage before freezing the world. Bosses without a death montage retain
the immediate presentation.

`BP_CinderOracle` remains the asset referenced by gates and encounter code, with
the displayed name **VESHRA - THE PALE STINGER**. Its saved Blueprint and arena
instance use `SK_PaleStinger` / `ABP_PaleStinger`. The capsule fits the broader body
(300 cm radius, 400 cm half-height); mesh base and capsule base meet the real arena
floor. The existing arena actor scale is preserved. Combat numbers, including
6,000 HP, spell damage, cooldowns, AoE sizes and summon settings, are unchanged.
The Balance Lab label and reset baseline name/montage references follow the new
appearance.

Validation: Development Editor build and all five selected tests pass:
`Encounters.ScorpionBoss`, `Encounters.Mechanics`, `Combat.TagTeamRegression`,
`ImpactFeedback.SwapFreeze`, and `Progression.RunChallenges`. Eight rendered
Unreal poses match authored bone heights within 0.264 cm, both montages pass
through the animation graph, and the saved arena reload verifies floor alignment
and unchanged combat values. Rig checks cover all vertex weights, loop seams,
finite poses and floor clearance. Evidence: `Saved/ScorpionBoss/`.
See [the editable source and regeneration guide](../Art/Enemies/ScorpionBoss/README.md).

The current size doubles the scorpion's initial linear dimensions (mesh scale 0.65
to 1.30) and its collision capsule, while preserving actor scale and floor alignment.
Its permanent rim overlay is cleared in the saved assets and native startup;
attack pushback is disabled in native defaults, Blueprint, arena instance and the
Balance Lab reset baseline. Existing short hit flashes remain. The boss regression
checks immunity for Samurai, Ninja and Other attack sources, alongside ordinary
enemy pushback coverage. Follow-up evidence is under `Saved/ScorpionBoss/SizeFollowup`.
The earlier follow-up Editor build, ScorpionBoss, SamuraiPushback and
TagTeamRegression tests passed. The corrected 2x size was then verified by saved
Blueprint/map reload, a fresh Editor build, ScorpionBoss, Encounters.Mechanics
and TagTeamRegression tests, and three rendered arena poses. Current evidence is
under `Saved/ScorpionBoss/SizeCorrection`; see [the ability audit](BossAbilityAudit.md)
for the saved spell values, guardian behavior and timing findings.
