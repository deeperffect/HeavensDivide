# Impact feedback

Configure attack feedback in these locations:

| Attack | Editor location |
| --- | --- |
| Samurai melee (including Double Cut) | BP_Samurai → Auto Attack Component → Auto Attack / Impact → Impact Feedback |
| Ninja kunai (including pierce, bounce and split children) | BP_NinjaProjectile → Class Defaults → Projectile / Impact → Impact Feedback |
| Blade Wave | BP_SamuraiWaveProjectile → Class Defaults → Blade Wave / Impact → Impact Feedback |
| New skill | Add an Impact Feedback Data variable and call Play Impact Feedback after accepted damage |

`FImpactFeedbackData` holds Niagara, sound, Niagara scale and rotation offset, surface-normal orientation, shake enable, shake class and strength. A missing asset is safe. Niagara is pooled with auto release. Feedback never applies damage.

The starter assignments use `NS_Hit_Basic_Once` at 0.65 scale for Samurai and 0.3 for kunai. Ninja uses `MS_Ninja_Impact` and disables shake. Samurai retains any existing `ImpactSound` as its once-per-swing fallback; when a new feedback sound is assigned it takes precedence and plays at each accepted contact. The setup script only assigns `MS_Samurai_Impact` if neither sound slot was configured.

`BP_CS_SamuraiImpact` derives from the native `SamuraiImpactCameraShake`: 0.07 seconds, small translation, almost no rotation and no FOV change. Location Amplitude Multiplier is 10 to make the movement visible at the rig's 1600 cm distance; the original multiplier of 1 produced only subpixel movement. Rotation Amplitude Multiplier is explicitly 1 because Unreal's Perlin constructor defaults it to 0. Edit its root shake pattern to tune duration, blending and amplitudes. Other heavy skills can assign their own CameraShakeBase subclass and authored scale.

`PerformAttackTrace` uses the existing consumed-notify guard. It accumulates successful damage across primary and secondary targets, plays VFX/audio per accepted target with shake disabled, then requests one shake after the loop. Repeated notifies, misses and rejected damage cannot shake. Each real Double Cut follow-up is a separate swing.

Projectile feedback uses a valid swept impact point/normal; overlap-only and melee hits ask the enemy for the nearest point on its capsule. Contacts are captured before damage so lethal reactions do not move the spawn point. Enemy damage, `HealthComponent.OnDamaged`, status effects and death events remain the generic reaction path. No attack-specific Niagara properties were added to enemies. Inspection did not find an existing enemy-owned hit Niagara spawn to remove.

All gameplay shakes should use `Play Gameplay Camera Shake` on the feedback library or SurvivorPlayerController. It multiplies authored scale by the cached settings object value, skips zero, and keeps at most one active gameplay shake. Equal/weaker impacts cannot restart it within 0.05 seconds; stronger impacts can replace it. Settings changes also rescale the currently active shake immediately. Direct calls to Unreal's StartCameraShake bypass this policy, so new skills should use the wrapper.

Camera Shake Intensity is a continuous 0–100% slider in the existing Settings panel. It defaults to 100% and saves immediately, matching Auto Targeting. The config property is `CameraShakeIntensity` on `HeavensDivideGameUserSettings`, persisted in the platform's `GameUserSettings.ini` under `[/Script/HeavensDivide.HeavensDivideGameUserSettings]`. No SaveGame or meta-progression format changes are needed.

Enemy hit flash is independent of attack feedback. Every EnemyBase subclass defaults to `M_EnemyHitFlash`, a white unlit translucent overlay lasting 0.08 seconds after nonlethal direct damage. Configure enable, duration, color, opacity and material under **Enemy / Hit Flash** in enemy Blueprint Class Defaults. A dynamic material is cached once per enemy; hits reset one timer. The prior overlay is restored afterward, and visual-state transitions/death cancel the flash. Bleed/poison ticks and Virulent Strain poison pulses do not start or refresh the flash; immune/rejected damage does not flash. No Niagara assignment or camera-shake setting is involved. Regression test: `HeavensDivide.ImpactFeedback.EnemyHitFlash`.

Source changes: `ImpactFeedback.h/.cpp`, `AutoAttackComponent.h/.cpp`, `AttackProjectileBase.h/.cpp`, `SamuraiBladeWave.h/.cpp`, `EnemyBase.h/.cpp`, `SurvivorPlayerController.h/.cpp`, `HeavensDivideGameUserSettings.h/.cpp`, `MainMenuWidget.h/.cpp`, module dependencies and the EngineCameras plugin entry. `ImpactFeedbackTests.cpp` covers isolated combat/settings regressions. `Tools/configure_impact_feedback.py` assigns Blueprint defaults and backs up existing Blueprints under `Saved/Backups/ImpactFeedback`.

Run regression coverage with Unreal Automation test `HeavensDivide.ImpactFeedback.CombatAndSettings`. Visual/audio feel should also be checked in PIE with 1 and 15 enemies, a miss, basic kunai and slider values 100%, 50% and 0%.

Samurai direct hits also apply a small horizontal pushback (25 cm over 0.1 seconds, easing out). Melee and Cleaver use **BP_Samurai → Auto Attack Component → Auto Attack / Samurai Pushback**. Double Cut suppresses pushback on its first strike (including the strike that earns the proc) and allows it on the second strike. Deathblow, outbound Blade Wave and Returning Wave never push. **Enemy / Pushback → Pushback Multiplier** adjusts susceptibility; 0 disables it for an enemy class. The custom enemy movement component sweeps against world geometry, temporarily prioritizes pushback over pursuit, and replaces rather than adds repeated impulses. Ninja hits and damage-over-time ticks do not initiate pushback. Test: `HeavensDivide.Combat.SamuraiPushback`.

Bleed and Poison icons share the health bar world anchor with fixed screen-space offsets (Bleed left, Poison right). Configure Status Indicator Spacing and Status Indicator Health Bar Gap under enemy Class Defaults / UI / Status. Slots stay fixed when either status expires; the old world offsets are deprecated.
