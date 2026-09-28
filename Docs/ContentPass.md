# Abilities, elite rewards, and survival maps

The September content pass adds real Niagara presentation to Overkill Burst,
Blood Transfer, Bleeding Edge, Venomous Kunai, Embedded Blades, Grand Entrance,
and Tag Team. Their Data Assets expose the assignments under **Runtime VFX**.
The status effects trigger on first application, and shared effect throttling and
the existing 64-accent cap prevent a particle burst per damage tick.

Blade Wave retains its ground slash, debris and slowdown. Double Cut retains its
360 slash. Ninja kunai, shuriken, return, split, fragment and clone upgrades retain
their existing visible weapon/clone implementations. Damage, speed, area, duration,
count, stance and resource modifiers inherit that presentation rather than spawning
unrelated effects. The audit includes every UpgradeDefinition in the upgrade folder.
The broken default enemy death-hit reference now points to an existing impact.
The missing Samurai active-ability effect is replaced with a golden radial burst
on each damage pulse. Ninja's missing kunai impact sound is also restored.

Vendor scale inputs (`User.Scale` and `User._Scale`) now receive the actual area
multiplier, without also multiplying component scale. Impact colors use the real
`User.Color 1` input. Smoke effects use separate tinted copies under
`/Game/HeavensDivide/VFX/Upgrades`; edit their `User.User_SmokeGradient` for color.
The original vendor assets are unchanged.

## Elite chest

Enemies marked **Drop Category = Elite** drop an `EliteRewardChest` instead of loose
XP. Gorilla is now marked Elite. Future elite Blueprints can opt into the same
behavior by setting that category and configuring XP Reward/Experience Pickup Class.
`Elite Reward Chest Class` optionally supplies a customized chest Blueprint.

The chest opens on proximity to the active character. Combat pauses during a 2.8s
close-up with an animated lid, gold loot glow, light and pickup chime. It restores
the previous camera and releases 40 arcing XP orbs. The original XP total is split
exactly, including remainders; orbs cannot collect until their flight finishes.
Repeat completion cannot duplicate rewards. Existing pause/upgrade menus block
opening. Destruction restores the camera and any pause owned by the chest.

Chest Blueprint defaults expose orb count, duration, collect radius, lid bone,
lid rotation, opening sound, mesh and Niagara component. The original vendor
treasure chest is reused. The orb count is a visual choice, not an XP multiplier.

## Map profiles

| Map | Identity | Rushes |
|---|---|---|
| `Lvl_B1_Lvl1` | Gentler mixed waves, 90% baseline population | Small Grunt advances and Crawler flanks |
| `Lvl_B1_Lvl1-2` | Faster pursuit and narrower flanking pressure | Crawler, Fish and Skeleton rushes |
| `Lvl_B1_Lvl1-3` | Lower population but heavier compositions | Bear walls, Creature escorts and Skeleton support |

Each has twelve one-minute phases, with the last open-ended. Every third phase
reduces population/refill pressure and disables new rushes. Rushes never bypass
the 160-enemy hard limit; they have individual cooldowns and a twelve-enemy
temporary overflow allowance. Each map has one placed spawner, and only that
actor's settings were changed. Terrain and objective actors are preserved.

Gorilla: 5,000 → 1,400 base HP, 100 → 120 XP, one alive, 90–115s replacement
cooldown. It starts at six minutes on maps 1/2 and five minutes on map 3.
Ogre: 500 → 260 HP and 7 → 12 XP, 40–55s replacement cooldown.
Devil: 55 HP/5 XP; Bomb: 35 HP/5 XP. Basic enemies and player ability damage/
upgrade values are preserved to retain their existing early kill breakpoints.
Threat HP growth is capped at 4% per elapsed minute.

This is an initial balance pass, not a claim of measured win-rate balance. In
playtesting, compare level reached at 3/6/9 minutes, time to kill a Gorilla, damage
taken during rushes, and whether the recovery phases actually let the player
clear space. The maps are separate selectable editor levels; this change does
not introduce a new campaign-travel sequence or menu.

## Cloth and verification

Only the generated `*_Clothes_*` assets/bindings were removed. Both new meshes,
their two material slots, skeletons and Ninja hair dynamics are preserved. See
`CharacterClothAndHair.md` for why the previous stability check was insufficient.

`Tools/configure_content_pass.py` applies the content settings and backs up each
edited asset under `Saved/Backups/ContentPass`. `Tools/verify_content_pass.py`
checks the saved assignments, map profiles, enemy values and cloth removal in a
fresh Unreal process. Reports are written to `Saved/ContentPass`.

Validation passed: editor C++ build; eight focused automation tests covering
elite rewards, Niagara assignments/scaling, both character upgrade builds,
Grand Entrance, combo VFX, spawn director and enemy death; fresh-process content
verification; 240-step Ninja hair stress check after cloth removal (maximum
segment-length ratio 1.257, no cloth instances). Art appearance and run difficulty
still need an in-game playtest; these automated checks do not measure either.


## Samurai VFX scale and enemy indicator correction

The new smoke effects authored `User._Scale = 0.2`, but the runtime previously
replaced that value with the area ratio. The active ability also applied its
art scale only to the component, which this Niagara graph ignores. Runtime now
multiplies the authored scalar by the art scale and area ratio, keeping the
component at unit scale. This removes the accidental fivefold enlargement.

For Samurai active VFX, edit `BP_Samurai > Combo Ability > VFX Scale` (X is the
uniform multiplier). For Overkill Burst, edit the upgrade Data Asset's
`Presentation > Scale` X. Changing `User._Scale` in either Niagara asset also
works. Damage radii remain unchanged. The new authored-scale options are enabled
only for the new smoke effects; other Niagara parameter conventions are retained.

Bomb and Gorilla now use the same unlit translucent ground-plane rendering as
Ogre, with `M_AttackIndicatorCircle`. Their former emissive decals added onto the
floor lighting, so matching colors alone could not match their appearance.
Existing FillColor/BorderColor values are preserved. Gorilla keeps its solid black
interior and strong continuous ripple, without the charge-warning brightness
boost. These circles are flat ground indicators, like Ogre's rectangle; they do
not conform to uneven terrain. Boss decal paths are unchanged.

Correction validation: editor build passed; six focused automation tests passed (including live mesh-particle scaling across 120 simulation steps for both Samurai effects). Rendered circle and rectangle fills matched exactly at RGB (166, 2, 35); alternate FillColor values, transparent aura interior, and continuous ripple animation passed. Reports: Saved/ScaleIndicatorFix.

Gorilla aura follow-up: OutlineOnly is now zero while FillAmount stays zero. This keeps the black interior permanently visible, with continuous outline animation and no charging fill.
