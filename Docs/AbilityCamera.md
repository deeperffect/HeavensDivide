# Joint combo camera

The three joint combos use camera framing for either active character. Tune **BP_PlayerCameraRig > Camera > Joint Combo**:

- **Enable Joint Camera**: enable the cinematic framing.
- **Joint Orbit Degrees**: strength of the charge arc. The saved default of 9 produces a restrained 3.15-degree arc.
- **Joint Zoom Amount**: charge dolly strength. The saved default of 0.16 produces an 8.8% push-in; perspective FOV stays fixed.

A smooth easing curve settles before the helper appears in smoke. Each combo definition's Helper Arrival Fraction sets this timing (35% of charge by default). Framing holds during release and returns once over 0.48 seconds from the actual camera pose. It follows the current player position during the return, including Eclipse's real dash endpoint. Release zoom-out, reversing orbit and extra impact kicks are removed. Joint projectile impacts do not add repeated shakes. Pause stops the sequence. Target changes, cancellation and teardown restore all camera properties. Joint framing takes priority over swap framing. The retired solo-ability camera controller has been removed.

See [Combo abilities](ComboAbilities.md) for animation/VFX/timing settings.
