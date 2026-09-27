# Shared attack indicators

Ogre warnings, Goblin Bomb's circular blast, Samurai boss cleaves,
dashes, point-blank AOEs and ground circles, and Ninja floor traps share the
Samurai lane indicator's black base, animated red edge and timed red fill.
The original Samurai trial material retains its existing fill and colors.

All five indicator master materials, including the Samurai trial lane, now
include a late-charge outline accent. `ImpactWarningStart` defaults to 0.8
(final 20% of the charge); `ImpactWarningStrength` defaults to 3 (0 disables).
These parameters are grouped under **Impact Warning** in material instances.
The outline ripples slightly inward and pulses brighter with accelerating phase
derived from charge progress. Interior fill and gameplay footprint stay intact.
The charge accent is gated off at FillAmount=1. `ContinuousRipple=1` instead
drives the accent from time for constant auras; `OutlineOnly=1` removes the
interior entirely, including the black background. Other attacks default both to 0.

Apply after regenerating the base materials:
`Tools/apply_indicator_impact_warning.py` (preserves existing graph tuning).
Then apply `Tools/refine_indicator_ripples.py` for the triple-strength/ring update.
Render validation: `Tools/verify_indicator_impact_warning.py`.
Ring, time animation and green/blue fill rendering: `Tools/verify_indicator_ring_and_colors.py`.
Backups: `Saved/Backups/IndicatorImpactWarning`.

Ogre rectangles now use `ABossGroundTelegraph::InitializePersistentRectangle`
and the exact surface material used by the Samurai boss. The old projected
rectangle is hidden. Placement follows the Ogre's slam center and facing during
windup; world dimensions match damage dimensions even on scaled enemies.

The Gorilla's montage-less contact attack has a separate persistent
`ContactAuraDecal`, using the boss's circle decal material. Its radius follows
the scaled ContactDamageSphere. It is an unfilled ring with continuous ripples,
follows the Gorilla, and hides on death or gameplay suspension. Attack fill
and attack cleanup cannot reset the aura. `ContactAuraMaterial` is editable.
The Gorilla's body meshes reject decals and aura projection depth is reduced
to 8 cm, preventing the ground effect from painting its lower body.

Materials under `/Game/HeavensDivide/Materials`:

- `M_AttackIndicatorRectangle`: flat surface warnings, filling along their length.
- `M_AttackIndicatorRectangle_Decal`: retained projected variant (unused by Ogre).
- `M_AttackIndicatorCircle_Decal`: projected circular warnings, filling from the center outward.
- `M_AttackIndicatorCircle`: surface version for previews or future mesh warnings.

`FillAmount` runs from 0 to 1 using the existing windup/fuse timers. `FillColor`
and `BorderColor` control appearance. `LaneAspect` is width divided by length,
set from each attack's dimensions at runtime. Runtime red/TelegraphColor overrides
have been removed: edit FillColor on the assigned material or material instance.
BorderColor controls the rim separately. Radius, damage, fuse duration and attack timing are unchanged.
Rectangle warning meshes are planes to avoid visible side faces. Floor traps fit
the warning to their actual HazardArea box and reset fill when a new cycle starts.

Setup and saved Blueprint assignments: `Tools/configure_attack_indicators.py`.
Backups: `Saved/Backups/AttackIndicators`.
Rendered previews and verification: `Saved/AttackIndicators` via
`Tools/verify_attack_indicators.py`. Runtime tests:
`HeavensDivide.Combat.AttackIndicators`, `HeavensDivide.Enemies.GoblinBomb`.
Ogre/Gorilla regression test: `HeavensDivide.Combat.TankIndicators`.
Saved tank migration: `Tools/fix_tank_indicators.py`; backups in
`Saved/Backups/TankIndicatorFix`.

The saved Floating Skeleton currently has no attack telegraph properties. The
old rectangular warning material has no remaining asset references; the old
circle material is retained by the boss's unused legacy component only.
