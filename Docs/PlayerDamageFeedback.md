# Player damage feedback

Actual damage to shared player health triggers a red screen-edge flash. It fades over 0.45 seconds and leaves the center transparent. Repeated hits restart the fade without stacking opacity. Healing, zero damage and invulnerable/rejected hits do not trigger it. Lethal damage does.

Tune the effect in `WBP_PlayerHUD` Class Defaults → `Player HUD | Damage Feedback`:

- **Enable Damage Vignette:** turn the effect on/off.
- **Damage Vignette Duration:** fade time in seconds (default 0.45).
- **Damage Vignette Opacity:** peak opacity per edge gradient (default 0.45; corners overlap).
- **Damage Vignette Edge Size:** fraction of width/height covered by each edge fade (default 0.20).
- **Damage Vignette Color:** red by default.

`PlayerDamageFeedback.cpp` paints two gradients through the existing HUD. It uses the HUD's existing tick only while fading, adds no timers or input-blocking widgets, and needs no material/texture asset. `PlayerHUDWidget` binds/unbinds the shared health component's `OnDamaged` event with the rest of its health listeners.

`HeavensDivide.ImpactFeedback.PlayerDamage` covers health-loss triggers, fade/refresh, healing, rejected and lethal damage, disabling the effect, and cleanup.

## Persistent low-health warning

At 30% shared player health or lower, the HUD fades in a red pulsing vignette around all four screen edges. There is no warning text or badge below the timer. The pulse repeats every 1.1 seconds; at 15% or lower it becomes stronger and repeats every 0.7 seconds. Soft gradient stops fade the edges into a fully transparent center. No blur, camera motion, extra actors, or post-process pass is added. The pulse combines with the existing damage flash using the stronger opacity, rather than stacking both. Default low-health edge opacity is 0.32, bounded at 0.4.

Healing above the threshold fades the warning out over 0.2 seconds. Death, victory, HUD teardown, and changing health sources clear it. Pause/upgrade menus hide the persistent warning and freeze its animation. Tune `Enable Low Health Warning`, `Low Health Threshold`, and `Low Health Edge Opacity` in `WBP_PlayerHUD` Class Defaults → `Player HUD | Low Health`. The critical threshold is half the configured low-health threshold.

Text-free vignette follow-up (2026-10-16): Development Editor build and the three focused player-damage, combat-number/menu-state and rendered-HUD checks pass. Pulse peaks/troughs and the clear center were checked; 720p and 1080p previews show no badge below the timer. Report and previews: `Saved/LowHealthVignette`.

## Animated enemy damage numbers

Accepted direct player damage displays a cream number above the enemy at the hit's world position, with a fast pop, upward drift, and fade over 0.46 seconds. Bleed, poison, and other damage-over-time ticks display no numbers. Critical hits start immediately at 1.65x scale, peak at 1.8x, and use larger gold numbers with `!`, a tiny lateral kick, a brief light flash and six spark strokes; they finish after 0.62 seconds. Existing critical hit VFX remain active.

The critical flag is passed from actual Blood melee/echo, Returning Fang, and Barrage rolls through the damage pipeline. Large damage is never inferred to be critical. Displayed values are actual HP removed, including the final hit; rejected, invulnerable, zero, and already-dead hits do not display numbers. Healing urns retain their existing shatter feedback.

`CombatNumberPresentation` stores at most 96 plain records per HUD and draws through the existing Slate paint pass. There are no per-hit actors, UMG widgets, Niagara instances, world traces, or timers. Very rapid direct hits on the same target and of the same style merge within 35 ms, without extending lifetime. Normal and critical damage never merge. Status damage is rejected before projection/queueing. Offscreen hits are rejected, expired entries are removed, and critical hits take priority over ordinary hits when the buffer is full. Staggering is deterministic and does not consume the gameplay RNG. Text uses fixed font sizes with a render transform to avoid generating a new font size every frame.

Disable numbers in `WBP_PlayerHUD` Class Defaults → `Player HUD | Damage Numbers | Show Damage Numbers`.

Validation: `Tools/Run-CombatFeedbackAutomation.ps1 -Render` exercises the real damage-to-HUD path, critical forwarding, accepted/rejected/lethal damage, hidden status ticks, direct-hit aggregation, a 5,000-hit saturation case, effect expiration, low-health recovery/death/disable, and rendered previews of the shipped HUD at 720p and 1080p. Screenshots are written to `Saved/CombatFeedback/Previews`.

Verified 2026-10-16: Editor Development build succeeded; all 16 combat/feedback automation tests passed (one existing warning-bearing test, no failures). Reviewed the 720p critical pop, 1080p float/fade, and healed HUD previews. Report: `Saved/CombatFeedback/Automation/index.json`. The previews exercise the shipped HUD and shared number painter against a neutral background; this was not a full-map GPU/FPS benchmark.
