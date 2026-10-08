# Character cloth and ninja hair

## Cloth pass 2 — 2026-10-08

Cloth is enabled on the separately selected material sections of both active meshes:

| Character | Mesh | Material slot | Panels | Simulation vertices | Fixed vertices |
| --- | --- | --- | ---: | ---: | ---: |
| Samurai | /Game/Assets/PlayerCharacters/Samurai/fdsafdsa | SamuraiClothes | 4 | 409 | 20 |
| Ninja | /Game/Assets/PlayerCharacters/Ninja/NinjaCharacterV3 | CharacterClothes | 3 | 265 | 19 |

The original render geometry, material-slot selections, skeletons, and imported skin
weights are preserved. The additional vertices and attachment-weight corrections
exist only inside the cloth simulation mesh.

The previous pass used very large character collision capsules, guessed attachment
pins on some panels, and imported weights that attached some loose ribbon tips to
unrelated leg bones. This pass uses actual shared garment seams, fixed attachment
bands, a smooth transition toward free ends, and animation targets derived from each
panel's attachment. Every disconnected panel must have a real seam; authoring fails
instead of guessing a top edge.

Long ribbons need room to hang under gravity. A very short max-distance sphere around
the attachment-driven animation target held them sideways in bent poses. The new
max-distance masks grow along the garment, capped at 120 cm for Samurai and 60 mesh
cm for Ninja (whose component scale is 2). These are allowable travel bounds, not
intended sway amplitudes. Stiff geodesic tethers preserve garment length; damping,
limited inherited motion, and stronger animation drive near the seams control sway.

Collision uses dedicated bone-aligned capsules in PA_SamuraiCloth and PA_NinjaCloth
beside the corresponding skeletal meshes. The gameplay/ragdoll physics assets are
unchanged. Swept collision is disabled: it caused severe dragging and stretching on
Samurai's rapid poses. The solver uses substeps instead. Self-collision and environment
collision are not enabled by this pass.

| Solver setting | Samurai | Ninja |
| --- | ---: | ---: |
| Iterations / maximum | 20 / 40 | 12 / 24 |
| Substeps | 6 | 4 |
| Global / local damping | 0.30 / 0.15 | 0.30 / 0.15 |
| Gravity scale | 1.0 | 1.0 |

SamuraiClothes now inherits SamuraiCharacterV4_Mat instead of using default imported
textures. Both cloth materials support clothing shaders and two-sided rendering.
The material-slot names and selected polygons remain unchanged.

## Validation and tuning

The editor module builds successfully. The saved assets are tested in an offscreen
Unreal game world for 420 steps per character: idle, running, rapid turns, a 100 m+
swap teleport, 15 fps frames, and three real attack montages (including a double-speed
attack). Checks require bound cloth with the expected particle count, finite positions,
fixed pins, bounded travel, limited edge stretching, capsule collision, and intact
Ninja hair. Front/back/side captures compare the same poses with skinning and cloth.

Reports, weight masks, and rendered comparisons are under Saved/CharacterSimulation/Pass2.
The geometric audit compares the saved mesh exports against the Before directory;
protected_assets.json records the unchanged gameplay physics and ABP_Ninja hashes.
This is a scripted simulation and visual review, not a full manual gameplay session
or a guarantee against every possible cloth/body intersection.

For tuning in the Skeletal Mesh Editor, edit the masks named
Pass2_FixedSeam_To_FreeEdge_cm and Pass2_Attachment_AnimationDrive. Keep the seam band
at zero distance. Adjust the dedicated cloth physics asset rather than the character's
gameplay physics asset. Recheck attacks and swaps after changes.

Commandlet options (editor closed; UnrealEditor-Cmd.exe followed by the project path;
include -unattended -RenderOffscreen -AllowCommandletRendering -noraytracing):

- -run=CharacterSimulationSetup -Inspect exports mesh, bone, and collision data.
- -run=CharacterSimulationSetup -Verify checks both saved simulations.
- -run=CharacterSimulationSetup -Review also renders pose comparisons.
- -run=CharacterSimulationSetup -Apply authors this pass on a clean baseline. It refuses
  to overwrite existing cloth or dedicated physics assets. Do not run it as routine tuning.
- -run=pythonscript -script=Tools/configure_character_cloth_materials.py finishes the
  cloth material setup. Use forward slashes in absolute Python script paths.

The earlier finalize_character_simulation.py and configure_ninja_hair_simulation.py
utilities also touch the hair setup; they are not required for cloth tuning.

Pre-pass backups are in Saved/Backups/CharacterClothPass2_20261008. Original meshes,
gameplay physics, and ABP_Ninja are copied with their Content paths; changed materials
are backed up under Materials. The discarded first iteration is kept in the Pass2
report directory for diagnosis. Older backups in CharacterSimulation20260928 remain.

## Ninja hair and swap reset

In ABP_Ninja, the AnimDynamics node immediately before the cached locomotion pose is
disabled. The final AnimDynamics node after the animation blends remains the only
active ponytail simulation, covering Ponytail1 through Ponytail5. This pass does not
modify ABP_Ninja or its hair settings.

The hair uses component space, zero linear constraint travel, progressively looser
angular limits, half-strength gravity, and 8/2 solver iterations. Extra world-motion
forces and angular springs remain disabled. Linear damping is 0.85 and angular damping
is 0.9; these fractional damping values must remain between 0 and 1.

ACharacterBase::SetCharacterMode resets cloth and animation dynamics when leaving
Inactive mode, after the incoming character has been placed at its swap location.
This existing behavior remains in place and is included in the cloth regression.
