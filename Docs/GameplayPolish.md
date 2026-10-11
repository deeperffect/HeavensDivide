# Gameplay polish — October 2026

## Ground

Level 1 uses more stone on its central route, loops, forecourts and rock patches. Grass islands remain. Stone samples bypass the material's tint, saturation, brightness and macro darkening; scene lighting and post processing still affect the lit surface. Original texture assets are unchanged.

Layout source: `Tools/level1_ground_layout.json`. Bake with `Tools/generate_level1_ground.py`; import and apply the targeted saved-asset changes through Unreal Python with `Tools/apply_gameplay_polish.py`. The migration saves timestamped asset backups under `Saved/GameplayPolish/Backups`.

## Enemy pressure

- Marching grunts move at **2.5 × min(330, 235 + 8 × minutes) cm/s**, previously 235–330 cm/s. The 1.4-second warning is retained.
- Marches choose a random approach angle and retry up to 24 whole formations. Each member must pass the normal ground, navigation, collision and arena checks. Members are never compressed or clamped onto the boundary. Marchers disappear when their next movement step would leave the arena.
- Maintained enemies choose among eight directions, weighted toward less crowded sectors. Sector counts are cached for one second, with each selected spawn direction reserved in the cache; this avoids scanning the whole crowd for every spawn. Normal spawn distance, placement rules, population caps and threat schedules still apply. Distant eligible grunts can be recycled six per evaluation; nearby enemies are not teleported.
- Two thirds of pursuers can aim up to 0.65 seconds ahead of player movement when farther than 350 cm away. Prediction uses ordinary swept movement and the existing obstacle fallback, without an additional route collision query. Close attacks and committed charges retain their existing aiming rules.
- Ordinary melee autoattacks no longer draw an AoE charging circle. Elite slams, bombs, lanes and other special attacks retain warnings.

## Breakable wall trap

One attempt window is 2–3 minutes, the other 5–6 minutes. The initial random trigger is in the first 45 seconds of each window, leaving time for placement retries. Failed placement and other active objectives defer the event, including beyond the window, rather than discarding it. Ground checks retry past transient characters so enemies cannot hide the floor.

Fifty-one segments form a ring 1,600 cm from the player (previously 800 cm). A 1.5-second warning precedes collision. The wall will not rise through the player. Each segment has **60 HP**, accepts both characters' attack damage, and gives no XP or kill reward. Destroying a segment immediately removes its collision. All remaining segments disappear after 10 seconds of eligible gameplay, measured from event spawn including the warning. Player death or level travel cleans up the event. Menus and other objectives suspend it.

The HP is an initial escape budget of about two seconds at 30 single-target DPS. It is fixed rather than scaled by run time. The warning also lets an attentive player escape before closure.

## Ember circle

A nearby circle appears around 1:15, with another opportunity every three minutes. It requires **20 seconds accumulated inside an 800 cm radius (four times the original area)**. Leaving pauses progress. Menus and other active objectives do not advance it. The opportunity expires after 105 seconds of eligible gameplay time.

Completion awards **XP equal to twice the current next-level requirement, plus 20 Soul Embers**. That is an XP cache, not a guarantee of exactly two levels. Completion commits before opening any resulting upgrade menus, so it cannot pay twice. Ember saving uses the existing pending-reward retry path. Balance Lab does not grant permanent Embers.

The green ring, world-space instructions/progress, and minimap marker identify the objective. An unfinished Ember circle does not suppress wall traps.

## Swap and simulation

Saved Ninja/Samurai settings: entrance montage rate 1.35×, departure rate 1.5×, freeze 0.62 seconds with 0.08 seconds of ease-in. Gameplay still waits for the complete accelerated entrance, including weapon draw timing.

Departures add character-specific bursts and a short cosmetic dash with up to four fading pose trails. The copy does not move or collide with the actual gameplay character. Its cloth simulation is disabled because the pose is evaluated manually.

The Ninja smoke material's missing shape reference was also repaired. Arrival and departure smoke now use soft procedural puffs with transparent edges. Samurai retains the red ink-slash burst. Walls use a separate mesh-UV stone material to avoid stretching the ground shader up vertical faces.

Upgrade menus suspend the gameplay meshes and cloth, preserving their previous states. Closing the menu resets hair dynamics and cloth before restoring their ticks. Cosmetic departures also stop progressing while the upgrade selection is open.

## Quick editor checks

1. Play Level 1. Eject with F8 and select its Encounter Director in the Outliner.
2. Under **Encounters → Testing**, use **Spawn Wall Trap (PIE)** or **Spawn Ember Circle (PIE)**. Return to the player with F8. Unsafe placement or an existing conflicting event prevents spawning.
3. Break walls with each stance; verify the gap is passable. Enter, leave and re-enter the circle; verify retained progress and a single reward.
4. Open several upgrade choices immediately after swapping Ninja, including while her ribbons are moving.
5. Circle a crowd and watch enemies approach through less populated sectors and cut across the route. Tune after a real run; the new behavior intentionally makes stationary damage output more valuable.

Automated regression reports and asset reports are written to `Saved/GameplayPolish`.

Verification: the editor build succeeded; 19 distinct regression checks passed, including rendered upgrade-menu/particle checks. The field-objective check was repeated successfully after the wall refinement. The saved Level 1 material, full 16-segment ring placement, Ember circle and both departure bursts were reviewed in rendered captures under `Saved/GameplayPolish/Previews`.

Follow-up validation: the enlarged circle (800 cm radius), 0.62-second swap freeze, deferred wall scheduling and cheaper crowd steering build successfully. All six selected field-objective/spawn/swap tests pass. The Level 1 runtime probe spawned 16 visible, sealed wall segments; updated wall/circle captures were inspected. Evidence: `Saved/GameplayPolishFollowup/Verification.json`. CPU work was reduced by removing prediction sweeps and caching sector counts; FPS on slower hardware has not been benchmarked.

## Kagutsu difficulty tuning

Kagutsu's spell interval is now 3.3 seconds in phase one and 2.5 seconds below half health (previously 5.2 and 4.1). This produces about 58% and 64% more casts. Falling ember, cursed-ground, summon and spiral blast radii are 60% larger; nova radius is 1,300 cm and eclipse radius is 900 cm. Lance lanes are 1,700 x 220 cm; eclipse lanes are 1,800 x 180 cm. The larger circles cover 2.56 times their previous area.

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

The wall trap now uses 51 segments to preserve a closed ring at a 1,600 cm radius. Segments keep 60 HP and the 1.5-second warning; the full event expires after 10 seconds. Apply saved map bounds with Tools/configure_encounter_object_bounds.py; map backups, reload checks and verification reports are under Saved/EncounterObjects.

Validation: Development Editor build and all seven selected gameplay/UI checks pass. The saved Level 1 probe produced 26 visible, sealed walls, eight valid urns across four arena corners, and no march ground warning. The boss arena retained 24 grounded warnings, two grounded guardians and seven ground-relative loot drops. Both pickup Blueprints expose visible markers, and rendered green/blue minimap markers were inspected. Evidence: Saved/EncounterObjects/Verification.json.

Wall/pressure validation: the Development Editor build and all four selected wall/spawn/fixed-health tests pass. The real Level 1 probe spawned 51 visible, sealed walls at the larger radius, then removed every segment after 10 gameplay seconds. Fresh-process reload verified all 21 enemy HP values and all 12 phases on both the spawner Blueprint and placed Level 1 spawner. Damage and XP rewards were preserved. Evidence: Saved/WallPressureFollowup/Verification.json.
