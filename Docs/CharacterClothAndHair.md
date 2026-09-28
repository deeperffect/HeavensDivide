# Character cloth and ninja hair

The active Samurai mesh is `/Game/Assets/PlayerCharacters/Samurai/fdsafdsa`.
The active Ninja mesh is `/Game/Assets/PlayerCharacters/Ninja/NinjaCharacterV3`.
The generated cloth was removed from both meshes after the visual issues reported
in testing. Both material slots, imported geometry, skin weights, and skeletons
are preserved. Both sections now use ordinary skeletal animation. The Ninja hair
changes below remain active.

The original generated setup used inferred seam pins, geodesic distance masks,
and existing body collision. Its numerical stability test did not establish visual
quality, attachment correctness, or reliable body collision. Skin weighting can
affect the animated cloth attachment pose, but it was not established as the root
cause here. Re-author attachment masks and collision while watching the actual
animations before enabling cloth again. Material slot separation alone does not
define the cloth attachment constraints.

In `ABP_Ninja`, the AnimDynamics node immediately before the cached locomotion pose
is disabled. The final AnimDynamics node after the animation blends is the only
active ponytail simulation, covering `Ponytail1` through `Ponytail5`.
It uses component space, zero linear constraint travel, the original body inertia,
progressively looser angular limits, stronger damping, half-strength gravity, and
8/2 solver iterations. Additional world-motion forces and angular springs are
disabled. The simulation responds to the animated head movement without adding
large forces from sudden character turns.
Edit this final node to tune the hair. Keep the earlier duplicate disabled.
Linear damping is 0.85 and angular damping is 0.9; keep these values between 0
and 1. AnimDynamics uses fractional damping, and values above 1 produce invalid
transforms in the solver.

`ACharacterBase::SetCharacterMode` resets cloth and animation dynamics when leaving
Inactive mode, after the incoming character has been placed at the swap location.
This avoids carrying old secondary motion across an offscreen teleport.

Original meshes and `ABP_Ninja` are backed up in
`Saved/Backups/CharacterSimulation20260928`.

Setup utilities (run with the editor closed, using `UnrealEditor-Cmd.exe` and the
project path; include `-unattended -RenderOffscreen -AllowCommandletRendering`):

- `-run=CharacterSimulationSetup` recreates the old generated cloth; do not run it
  as a routine fix. It is retained as an authoring utility only.
- `-run=pythonscript -script=Tools/configure_ninja_hair_simulation.py` configures the ponytail.
- `Tools/finalize_character_simulation.py` requires existing generated cloth and
  is not applicable to the current reverted setup.
- `-run=CharacterSimulationSetup -Verify` steps both characters through idle,
  movement, rapid turns, a swap teleport, and slower frames; it checks for missing
  cloth output, non-finite/exploding cloth positions, and excessive hair stretching.

Reports and per-vertex cloth masks are written under `Saved/CharacterSimulation`.

Historical validation of the now-removed cloth: both characters completed 240 simulation steps covering movement,
rapid turns, a 100m+ swap teleport, and 15fps frames. The Samurai produced 22,320
cloth particle samples and the Ninja 25,680; neither cloth simulation produced
non-finite or exploding positions. The maximum measured ninja hair segment length
was 1.257 times its reference length under this stress test, within the 1.5 limit.
The same checks passed again after reloading the saved assets in a fresh Unreal process.
This is a numerical stability check; the final amount of sway can still be tuned
to taste in the editor.
