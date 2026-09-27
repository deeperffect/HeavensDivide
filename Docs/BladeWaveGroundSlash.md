# Blade Wave ground slash

`DA_Upgrade_SamuraiBladeWave` keeps control of the VFX through Presentation.
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
- Debris Lifetime: up to 5 seconds for remaining particles after emission stops.
- Debris Material: material applied to all three debris mesh renderers through
  `User.DebrisMaterial`. Default `M_BladeWaveDebris` has an editable StoneColor.
  A replacement material must support Niagara mesh particles. This is an explicit
  material assignment, not automatic sampling of the terrain material.

The collision stays above the ground with the slash. Slowdown affects the moving
damage volume and VFX together; wave range remains a cap. On return, the wave
restores its initial speed and starts a new oriented slash. Outbound debris remains
in place. Completion stops damage and releases particle tails with bounded cleanup.
External cancellation destroys the active visual with the wave.

The existing Wave VFX Authored Duration applies to legacy projectile components;
ground-slash presentation runs at its original particle speed. No BP_Spawner or
second ProjectileMovement component is needed. The vendor's demo speed (1000 cm/s)
is replaced by the attack's configured speed (currently 1400 cm/s).

Setup: `Tools/configure_ground_slash.py`. Backup:
`Saved/Backups/GroundSlashIntegration/DA_Upgrade_SamuraiBladeWave.uasset`.
Regression test: `HeavensDivide.Combat.GroundSlashMotion`.

The ground decal renderer uses `User.GroundTrailOrientation`, set before activation
from the slash heading and the original downward projection rotation. This aligns
the long decal axis along sideways and diagonal attacks while keeping deposited
marks in world space. `Tools/fix_ground_slash_trail.py` updates this binding without
changing upgrade tuning. Direction coverage: `HeavensDivide.Combat.GroundSlashTrailDirection`.
