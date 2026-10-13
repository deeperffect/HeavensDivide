# Veshra — The Pale Stinger

Editable source: [PaleStinger_Rigged.blend](PaleStinger_Rigged.blend).
This replaces the arena boss's humanoid appearance with the supplied scorpion.
The original UVs and base-color texture are retained.

## Source and rig

`Enemy_BossScorpion.fbx` and `Enemy_BossScorpion_base.jpg` are unchanged copies
of the user's Desktop/EnemyScorpion assets. `Scorpion_Source.blend` is the
normalized inspection scene. The rigged scene packs the original texture and
contains the skinned mesh, armature and four saved animation actions.

The skeleton has 38 bones: body/head, three tail bones, three articulated leg
chains per side with foot controls, and two articulated claw chains with separate
pincer fingers. Every vertex has normalized weights, with at most four influences.
Hard shell pieces use rigid weights; connected joints have smooth transitions.
There is no cloth simulation or runtime procedural IK.

| Clip | Duration | Behavior |
| --- | --- | --- |
| `PaleStinger_Idle` | 2.4 s | Looping body breathing and subtle claw/tail motion |
| `PaleStinger_Walk` | 0.9 s | Looping alternating tripod gait; lifted swing legs and supported feet |
| `PaleStinger_Cast` | 1.8 s | Body, head, claws and tail wobble; smooth entrance and exit |
| `PaleStinger_Death` | 2.8 s | Body collapse, folded legs/claws and lowered tail; final pose held |

In Blender, select the armature and choose an action in the Action Editor.
The mesh FBX contains the skeleton and skin; the four separate animation FBXs
contain baked bone transforms at 30 fps. Authoring height is 6 m; the game uses
mesh scale 1.30 and the arena's existing actor scale 1.06. This is twice
the initial scorpion implementation's linear dimensions (mesh scale 0.65).

## Unreal integration

Assets are under `/Game/Assets/EnemyCharacters/PaleStinger/`:

- `SK_PaleStinger`, its skeleton and three LODs, each with one material section.
- `M_PaleStinger` uses the original sRGB texture directly with an opaque lit shader.
- `ABP_PaleStinger` switches Idle/Walk using actual actor velocity.
- `Animations/A_PaleStinger_*` and Cast/Death `AM_PaleStinger_*` montages.

The existing `/Game/HeavensDivide/Blueprints/EnemyCharacters/Bosses/CinderOracle/BP_CinderOracle`
asset and native `CinderOracleBoss` class remain the encounter entry points so
portals and saved references continue to work. The displayed name is
**VESHRA - THE PALE STINGER**. The saved `Lvl_B1_Arena` instance also uses the new
appearance. Mesh yaw is -90 degrees to align the supplied model with actor forward.
The capsule is radius 300 / half-height 400 cm and the mesh origin sits at its base.
The permanent rim overlay is removed from both the saved mesh and runtime startup.
Attack pushback is disabled through the boss's native defaults, saved Blueprint,
arena instance and Balance Lab reset baseline. Brief damage hit flashes retain
their normal behavior.
HP, damage, spell patterns, cooldowns and guardians retain their previous values.

Boss defeat records victory and stops run gameplay immediately. The victory
screen's world freeze waits for the death montage, allowing the collapse to play.

## Rebuild and validation

From the project directory, regenerate the rig and FBXs with Blender 5.2:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe' --background --threads 4 --python Tools/Blender/ScorpionBoss/rig_and_animate.py
```

Then run `Tools/import_scorpion_boss.py` through Unreal Editor Python. It corrects
the FBX armature/root bind basis for the mesh and animation imports; direct manual
reimport without this correction can produce incorrect root height/scale.
Run `Tools/review_scorpion_boss.py` with `-RenderOffscreen` to compare eight imported
poses against the authored rig and test both montages. Its `-ScorpionImportFirst`
option imports first; `-ScorpionActivate` applies the verified result afterward.
`Tools/activate_scorpion_boss.py` backs up the boss Blueprint/map before saving,
then reloads them and checks that combat values and floor placement are preserved.
`Tools/resize_scorpion_boss.py` applies only the corrected 2x size and floor offset,
preserves combat settings, reloads the assets and exports the saved ability values.
See [the boss ability audit](../../../Docs/BossAbilityAudit.md) for all seven patterns.
Legacy encounter regeneration scripts can restore the former humanoid appearance;
rerun activation after using them.

`Tools/review_scorpion_arena.py` captures Idle, Cast and Death with the saved arena
lighting without saving preview changes. `-ScorpionSizeFollowup` first reapplies
the current boss settings and checks the Level 1 lighting connection.
Reports/previews are under
`Saved/ScorpionBoss/`. Native test: `HeavensDivide.Encounters.ScorpionBoss`.

Original SHA-256 hashes:

```text
Enemy_BossScorpion.fbx      604643568bb5c72bae6df846ff37f711d68fe78d02e932921868350467cb157c
Enemy_BossScorpion_base.jpg 592a0333e9bb764c30e7528c89974e6e995189e498ff1d16deb5d7ed078b19b9
```
