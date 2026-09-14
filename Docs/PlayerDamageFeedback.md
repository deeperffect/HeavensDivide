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
