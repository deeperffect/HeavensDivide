# Blade Wave ground slash

`DA_Upgrade_SamuraiCrescentStance` keeps control of the VFX through Presentation.
Its Pulse System uses `/Game/HeavensDivide/VFX/NS_GroundSlash_BladeWave`, a copy
of the vendor's V2 system. The original vendor assets are preserved.

The working system also includes V1's four `Trail01`–`Trail04` layers, their four
`_origin` event sources, and `StarParticles` / `StarParticles002`. The copied event
handlers refer to the copied sources, not the original system's emitter IDs.
These use the original trail/fire materials, curves, and particle timing.

`SlashMeshTUT > Mesh Renderer > Meshes > Index 0 > Rotation` is set to Roll 90°,
laying the crescent horizontally. Adjust that renderer rotation to change the
mesh plane independently of the ground decals and airborne effects.
The mesh-space Pivot Offset cancels the mesh bounds center on Y/Z before this
rotation. That centers the crescent over the trail without changing its forward
placement, and follows particle scaling and attack direction. Project console
variable `r.LensFlareQuality=0` disables lens-flare post processing; bloom and the
authored light, trail, and spark emitters remain enabled.
The existing ground projection, debris material, slowdown, and return behavior
remain controlled by the upgrade presentation.

Regular Crescent waves show the crescent, glowing ground line, trails and sparks,
but no ground debris. All three debris layers appear only when the primary wave's
**Lingering Wake** (`CrescentField`) roll succeeds. Owning the upgrade alone does
not enable debris. Split waves inherit the parent's success or failure for both
fields and debris, without another roll. A returning wave keeps its original
result without rerolling, including after its one damage field has spawned. Its return animation reuses
the stationary outbound rocks rather than depositing a duplicate ground trail;
air debris retains its authored animation.

`SamuraiBladeWave.cpp` changes only the spawned Niagara component's `Debris*`
emitter enable flags. The shared Niagara asset and vendor assets retain all their
authored layers. The damaging field has no separate blue decal, rectangle or
eruption flash; its only persistent ground visual is the deposited Niagara debris. When outbound travel finishes,
a successful proc creates one strip covering the whole launch-to-endpoint path,
using the wave's width. Its 3-second damage duration (or 0.3-second eruption delay)
starts then. Returning waves do not create another strip; each successful split
inherits the proc and covers its own completed path.

Upgrade an existing working effect without resetting tuning with
`Tools/complete_ground_slash_effects.py`; backup is in
`Saved/Backups/GroundSlashFullEffects`. Test
`HeavensDivide.Combat.GroundSlashFullEffects` checks copied event connections,
mesh rotation, and actual particle emission from every visible added layer.

Presentation > Ground Slash exposes:

- Ground Slash Motion: enables terrain following and projectile slowdown.
- Slowdown Delay / Rate: vendor values 0.4 seconds and 3.5. This changes travel
  velocity, not Niagara simulation speed.
- Fit Slowdown To Range: compresses that curve when its natural stopping distance
  exceeds the configured wave range. Disable for the literal vendor timing;
  the attack's range cap still applies.
- Ground Trace Distance / Ground Offset: 150 cm trace, visual 10 cm above ground.
  Ground needs WorldStatic collision. Characters are not treated as terrain.
- Max Travel Time: 5 seconds per outbound/return phase as a cleanup limit.
- Debris Lifetime: particle-tail cleanup for waves without a field. Field debris
  instead lasts until that field expires or erupts, including duration upgrades.
- Debris Material: material applied to all three debris mesh renderers through
  `User.DebrisMaterial`. Default `M_BladeWaveDebris` has an editable StoneColor.
  A replacement material must support Niagara mesh particles. This is an explicit
  material assignment, not automatic sampling of the terrain material. These
  layers emit on gameplay waves only when that wave procs Lingering Wake.

The collision stays above the ground with the slash. Slowdown affects the moving
damage volume and VFX together; wave range remains a cap. On return, the wave
restores its initial speed and starts a new oriented slash. Outbound debris remains
in place. Outbound completion transfers the deposited particles to the field,
which owns cleanup at its last damage pulse or eruption. Without a field, normal
particle tails use bounded cleanup. External cancellation destroys the active
wave visual; previously completed field trails keep their own lifetime.

The existing Wave VFX Authored Duration applies to legacy projectile components;
ground-slash presentation runs at its original particle speed. No BP_Spawner or
second ProjectileMovement component is needed. The vendor's demo speed (1000 cm/s)
is replaced by the attack's configured speed (currently 1400 cm/s).

Setup: `Tools/configure_ground_slash.py`. Backup:
`Saved/Backups/GroundSlashIntegration/DA_Upgrade_SamuraiBladeWave.uasset`.
Regression test: `HeavensDivide.Combat.GroundSlashMotion`. It checks actual
particle emission with failed and successful Lingering Wake procs, all three debris layers,
unchanged crescent/ground-line/trail/spark emission, return and split waves, and
inherited split-wave results, retained return results after field spawning, and
removal of debris from new waves after restoring a build without Lingering Wake.
The particle-emission suites (`GroundSlashMotion` and `GroundSlashFullEffects`)
require rendering; use `-RenderOffscreen`, not `-nullrhi`, for unattended runs.

The earlier debris separation passed the Development Editor build, `CrescentBuilds`,
`GroundSlashTrailDirection`, and both rendering-enabled particle suites. Evidence:
`Saved/Logs/BladeWaveDebrisGateFinalBuild.log`, `BladeWaveDebrisGateRegression.log`
(Crescent/trail direction), and `BladeWaveDebrisGateFinalVFXRegression.log`
(particle suites with rendering enabled).

The earlier correction to use each wave's field proc passed the Development Editor build
and rendering-enabled `GroundSlashMotion` / `CrescentBuilds` suites. Coverage forces
failed and successful rolls, opposite parent/split outcomes, and retained return
results after the field spawns. Logs: `Saved/Logs/BladeWaveDebrisProcBuild.log` and
`Saved/Logs/BladeWaveDebrisProcRegression.log`.

The completed-path field update passed the Development Editor build and both
rendering-enabled `CrescentBuilds` / `GroundSlashMotion` suites. Coverage checks
damage at launch, midpoint and endpoint; scaled width and end caps; exclusion of
untravelled ground; identical eruption coverage; independent split strips; no
duplicate return field; and debris matching each wave's proc result. The saved
Lingering Wake description was updated with other card assets preserved. Evidence:
`Saved/Logs/CrescentFieldPathBuild.log`, `CrescentFieldPathAssets.log` and
`CrescentFieldPathRegression.log`.

Split waves now inherit the parent's field result without rolling again. The
Development Editor build and rendering-enabled `CrescentBuilds` /
`GroundSlashMotion` suites passed. Tests force opposite current chances after
the parent launches, verify successful/failed inheritance for fields and all
three debris layers, and check return splits after the parent's field has
already spawned. Each successful child retains its own full-path strip.
Lingering Wake and Wake Chance descriptions were updated with tuning and other
upgrade assets preserved. Evidence: `Saved/Logs/CrescentSplitInheritanceBuild.log`,
`CrescentSplitInheritanceAssets.log` and `CrescentSplitInheritanceRegression.log`.

The ground decal renderer uses `User.GroundTrailOrientation`, set before activation
from the slash heading and the original downward projection rotation. This aligns
the long decal axis along sideways and diagonal attacks while keeping deposited
marks in world space. `Tools/fix_ground_slash_trail.py` updates this binding without
changing upgrade tuning. Direction coverage: `HeavensDivide.Combat.GroundSlashTrailDirection`.

`Tools/persist_crescent_field_debris.py` binds `User.GroundDebrisLifetime` to
the two stationary debris emitters in the working Niagara system, with backups
under `Saved/Backups/CrescentFieldDebris`. Runtime budgets their particle lifetime
for travel plus the field, then the field removes them at the exact end of its
damage. Niagara keeps simulating at normal speed; airborne debris keeps its
authored lifetime. All upgrade tuning and vendor assets are preserved.

The debris-only field update passed the Development Editor build and rendered
`CrescentBuilds`, `GroundSlashMotion`, and `GroundSlashFullEffects` suites. Coverage
checks the original deposited particle counts and visible mesh scales just before
field expiry at base and maximum duration, cleanup on expiry/eruption, no separate
indicator or eruption flash, inherited split procs, and preserved wave layers.
The working Niagara asset was backed up under
`Saved/Backups/CrescentFieldDebris/20261003_222644`; card and vendor hashes were
preserved. Logs: `Saved/Logs/CrescentFieldDebrisFinalBuild.log`,
`CrescentFieldDebrisFinalAssets.log`, and `CrescentFieldDebrisRegression.log`.
Niagara authoring requires `-AllowCommandletRendering -RenderOffscreen`.
