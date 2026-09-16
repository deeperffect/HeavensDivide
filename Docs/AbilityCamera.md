# Thousand Cuts camera

The camera automatically uses steadier framing while the active Ninja performs her combo ability, including root-motion movement. Small movements stay inside a world-space dead zone; larger movements use damped, speed-limited tracking. A maximum focus distance takes priority over smoothing on very long dashes. This is a world-space framing limit, not a guaranteed screen-space boundary at every resolution or camera angle.

Tune **BP_PlayerCameraRig → Camera → Thousand Cuts**:

- **Enable Thousand Cuts Camera**: toggle this behavior.
- **Ability Dead Zone**: stationary focus radius, default 160 cm.
- **Ability Max Focus Distance**: maximum character-to-focus distance, default 420 cm. Reduce if she gets too close to the screen edges.
- **Ability Follow Speed**: damping response, default 3. Lower values follow more slowly.
- **Ability Max Camera Speed**: normal tracking speed cap, default 800 cm/s. The maximum focus distance can override it.
- **Ability Zoom Out Multiplier**: default 1.25; scales spring-arm distance for perspective and width for orthographic cameras.
- **Ability Blend In / Out Duration**: default 0.25 / 0.65 seconds.

Tracking and transitions use real time so ability freeze does not stall them. After the ability, focus blends back to the character's current position and restores the original zoom and spring-arm lag. Changing targets or ending play cancels framing and restores settings. Samurai and normal movement keep their existing camera behavior.
