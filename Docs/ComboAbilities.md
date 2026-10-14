# Combo abilities

Press **Q** when the shared combo meter is full to activate the equipped combined ability. Gamepad uses the left face button (X on Xbox / Square on PlayStation). Keyboard binding is editable in Settings > Keybinds > Combo Ability. Existing saves that already assign Q to another action receive an available fallback key.

## Shared joint combos

Loadout has one **Combined Ability** dropdown below Samurai Stance and Ninja Stance. Q activates the same recipe whichever hero is controlled. The inactive hero appears as an animated, collision-free performer; possession stays unchanged. Eclipse keeps Samurai at his casting origin through charge, release and recovery, whether he is controlled or helping. Both characters cast together. Soul Convergence includes Crimson Tempest; Eclipse Severance and Lunar Tempest unlock individually under Skill Tree > Abilities for 18 Soul Embers each.

| Recipe | Combined attack | Matching route upgrades |
| --- | --- | --- |
| Crimson Tempest | Samurai performs a wide Blood slash; Ninja follows with an arcing kunai volley. | Blood damage/area, Bleed stacks/cap/duration, criticals, Double Cut, Echo and status/death procs; Barrage projectile count, poison load/cap/duration, criticals, pierce, splitting and bounce rules. |
| Eclipse Severance | Ninja throws individual fast Fangs repeatedly while Samurai charges a large Iaijutsu slash. Targets follow the real upgraded lanes, including Double Cut and endpoint areas; remaining victims receive serial follow-through throws. | Iaijutsu charge speed, width, damage, pacts, marks, Double Cut, endpoint/cascade procs; Fang damage, speed, count conversion, critical/pressure/pursuit, spectral twins and return bursts. |
| Lunar Tempest | Ninja releases a Great Shuriken, Samurai sends a Crescent into it, then it emits four Crescent slashes in a rotating + pattern along its flight. | Shuriken count-to-size, size, lifetime, contact cadence, serration, growth, grinding, orbit/pulse/burst and twin rules; native Crescent width, damage, travel, speed, multishot, crossing/Double Cut, split, return, slow, fields and pacts. |

Every combo works with any pair of stances. Matching is evaluated separately for each character. With a different stance, that half uses the combo's base attack and generic character/shared stats, without granting the matching route or borrowing its upgrade ranks. Crimson retains one intrinsic Bleed stack off Blood. Cooldown and assist scheduling do not turn the cinematic into ordinary automatic attacks. Procession retains its bounce/poison upgrades, while the combo's volley remains an arc.

Choreography opens with a restrained camera push. Once it settles, the helper appears in smoke (default: 35% of charge), then both heroes finish charging in a stable frame. The helper leaves in smoke at its actual final position; the camera makes one 0.48-second return. FOV stays fixed. There is no release zoom-out, reversing orbit, secondary camera kick or repeated projectile hit shake. Eclipse no longer creates an extra native Samurai ghost during its joint cast. Both performers stay at their casting positions; the Iaijutsu slash still travels along its full upgraded attack lane.

Eclipse has a **1.25-second base charge** (previously 0.75); matching Iaijutsu charge upgrades still shorten it. Ninja keeps the return-to-ready portion of her final throw animating through recovery, including when no targets remain.

Ninja alternates **Ninja Montage** and **Ninja Alternate Montage** on successive throws (the saved right/left Ninja throws). After the helper entrance, she throws one Fang per 0.14-second beat during Eclipse's charge, cycling through living enemies inside the actual upgraded lane shapes. Targets leaving the lane are removed from the charge queue. New slash victims join the serial follow-through queue; defeated targets receive harmless visual Fangs. A hitch cannot collapse the queue into a simultaneous fan. Matching Fang upgrades can still produce their native spectral twins.

Lunar's wheel is **50% larger** in both presentation and hit radius, on top of matching Great Shuriken upgrades. Its base Crescent emission interval is **0.65 seconds** (previously 0.16). Lunar emits four base directions at 90-degree intervals relative to the spinning wheel's current yaw. Twin shurikens use their own rotation. Native Crescent multishot, Double Cut and other matching upgrades can add waves to those four base arms. Delayed waves use the joint cast clock so slow motion does not stretch their cadence.

Movement, dash, swap and basic attacks are blocked through windup plus **0.42 seconds of animated recovery on the combo clock**. At release, animation, native combo projectiles and Niagara start at **35% playback** and ease to full speed over **0.28 combo seconds**. This adds roughly 0.2 seconds of visible follow-through without an idle hold. Monsters, enemy projectiles and attack timers run at **10% world speed** during charge, then smoothly return to normal through recovery. The separate cinematic time layer preserves other freezes and pre-existing slow motion. Shared health remains protected while control is locked. Menus pause the sequence. Lingering wheels, waves and fields continue after control returns at normal world speed. Cancellation, death and teardown restore time, visuals, protection and camera settings.

Accepted slash, Fang, kunai, shuriken and Crescent hits spawn an impact burst at the enemy, including lethal hits. The effects belong to the combo rather than the enemy, so death does not cut them off. A per-enemy cosmetic interval prevents overlapping wave contacts from flooding one target; it never limits damage. Immune targets do not show false hit bursts.

Crimson's owned VFX disables the vendor's large flash, smoke and distortion shockwave emitters. The shared electric charge omits its dark ground mask to keep the performers readable.

### Editing joint combos

Open **Content > HeavensDivide > Abilities > Combos**. Stable asset names preserve previous purchases:

| Asset | Current combo |
| --- | --- |
| `DA_Combo_BloodMoon` | Crimson Tempest |
| `DA_Combo_HeavensDivide` | Eclipse Severance |
| `DA_Combo_Moonwell` | Lunar Tempest |

**Joint Combo** controls Samurai/Ninja power multipliers, area, windup, Release Recovery, Fang Throw Interval, Cinematic World Speed, Wave Interval and Fang Speed Multiplier. **Joint Combo > Presentation** supplies both montages, charge/secondary effects and release sound. **Presentation** supplies the main effect, tint, rotation, scale and emission time. Niagara copies live under **Content > HeavensDivide > VFX > Combos > Joint**. Native route effects remain in their existing stance settings: combo upgrades reuse those gameplay actors and their presentation.

For damage, edit **Samurai Power** and **Ninja Power**. Crimson uses Radius for its slash and Count/Range for its volley. Eclipse uses Radius/Range for its lane. Lunar always has four base arms, uses Wave Interval for shedding, and Duration for the off-stance wheel lifetime; a matching Great Shuriken inherits its native lifetime upgrades.

The three definitions expose the actual effect slots and tuning:

| Settings | Controls |
| --- | --- |
| Presentation | Main VFX, offset in cast forward/right/up axes, rotation, scale, color and emission seconds; Tint VFX preserves authored colors when disabled; VFX Completion Seconds allows particles to finish after emission stops. |
| Joint Combo > Presentation > Charge | Charge VFX scale, offset, rotation, color and seconds. Zero seconds uses the whole charge; a positive value ends emission earlier. |
| Joint Combo > Presentation > Secondary | Ninja volley/throw flash system plus scale, offset, rotation, color and emission seconds. |
| Joint Combo > Presentation | Samurai Montage, Ninja Montage, Ninja Alternate Montage, Fang Presentation Scale, optional Lunar infusion accent, Release Sound and volume. |

Eclipse **Fang Presentation Scale** is `1.0`, matching the `BP_NinjaProjectileFang` root scale. All Fang kunai share the same visible size: ordinary stance shots, spectral twins, assists, clones and Eclipse throws (including cosmetic follow-through). This reduces the former stance scale of about `2.98` and increases the former Eclipse scale of `0.3`; damage and native hit detection are unchanged.

An empty main, charge, secondary or impact VFX slot disables that effect. Empty helper VFX overrides use the helping character's shared smoke settings. Main/charge/secondary tuning affects only presentation. Native Fang projectiles, Iaijutsu lanes and Crescent waves retain their existing Blueprint/stance VFX settings; these remain shared with normal attacks. Eclipse falls back to Ninja's Alternate Attack Montage if its alternate slot is empty, and to its primary throw if neither alternate is assigned.

`Tools/cleanup_combo_abilities.py` migrates the saved presentation settings and removes retired assets only after checking package references. It backs up every changed/deleted asset under `Saved/JointComboCleanup/AssetBackups`. Existing main effect assignments and gameplay tuning are preserved.

Tune camera framing in **BP_PlayerCameraRig > Camera > Joint Combo**: Enable Joint Camera, Joint Orbit Degrees and Joint Zoom Amount. Changing targets or ending play restores distance, orthographic width, FOV, rotation, vignette, tracking and camera lag.

`Tools/polish_joint_combos.py` updates only pacing and descriptions, with backups under `Saved/JointComboPolish/AssetBackups`. It preserves assigned effects, montages and attack balance.

`Tools/create_joint_combos.py` reproduces the initial three definitions/effect copies and backs up their prior assets under `Saved/JointCombos/AssetBackups`. Re-running it resets initial tuning. Vendor VFX are unchanged. Retired solo definitions, owned effects, gameplay actors, pulse settings/events, camera behavior and authoring scripts have been removed. BalanceLab now activates Crimson Tempest. Historical save-ID migration remains so previous purchases retain their value.

Save version 7 retains the three stable unlock IDs, maps relevant old Ninja purchases to their joint recipes, and refunds retired/duplicate purchases once. Root owners can use Crimson immediately. Equipment remains shared across map travel and retries in the game instance.

Implementation: `JointComboActor` owns choreography, native attack children and presentation; `JointComboTime` owns the cinematic clock and restoration; `JointComboCamera` owns framing; `ComboAbilityComponent` spends the shared meter and controls recovery; `RunLoadoutSubsystem` stores the single recipe.

**Joint Combo > Timing** exposes `ReleaseStartSpeed`, `ReleaseRampSeconds` and `HelperArrivalFraction`. The fraction controls when the camera finishes its push and the helper enters. `CinematicWorldSpeed` controls the charge slowdown; `ReleaseRecovery` controls the animated follow-through.

**Joint Combo > Presentation > Helper** exposes separate `HelperArrivalVFX` / `HelperDepartureVFX`, plus shared smoke scale, rotation, offset and emission time. **Impact** exposes `HitVFX`, scale, rotation, offset, emission time and minimum cosmetic interval. Impacts use the combo's existing `Color`; smoke preserves its authored colors. Helpers use `SwapPresentation > Character Smoke VFX` on BP_Samurai / BP_Ninja by default: `VFX/Swap/NS_SamuraiSmoke` is red and `NS_NinjaSmoke` is purple. Their combo scale is 0.125 (25% larger than before). The independent impact effect remains `VFX/Combos/Joint/NS_JointHit`. `Tools/refine_joint_combos.py` saves charge, wheel, pulse and smoke defaults with backups under `Saved/JointComboRefinement/AssetBackups`; it preserves main/secondary VFX and custom helper overrides. Tune `Windup`, `WaveInterval` and `ShurikenSizeMultiplier` in the corresponding combo definition.

### Verification

The refinement Editor build and 16 focused checks pass, including actual Ninja recovery poses, matching charge upgrades, reduced Lunar cadence, both helper colors, planted Samurai scale, assist smoke and swap-freeze cleanup. Nine of 24 rendered combat previews were reviewed. Evidence is `Saved/JointComboRefinement/ValidationSummary.json`.

The flow-polish Editor build and 11 focused checks pass. Release/world acceleration, frame-order compensation, helper timing/smoke, accepted and lethal hits, immunity, cosmetic throttling and both leader choices are covered. Nine final previews were reviewed. Evidence is `Saved/JointComboFlow/ValidationSummary.json`. Existing artist-selected main/secondary VFX are retained; secondary effects remain optional.

The Win64 Development Editor build and 14 focused checks pass for the cleanup. They cover shared-meter activation/recovery, unlock migration, loadout persistence, current Niagara assignments, editable VFX transforms/tint/duration, alternating right/left Ninja montage assets, native route upgrades and camera restoration. The final montage blend-in safeguard has a separate targeted rerun. Nineteen retired assets were backed up and removed after reference checks; exactly three definitions remain. Evidence is `Saved/JointComboCleanup/ValidationSummary.json`. Earlier camera-polish evidence is retained under `Saved/JointComboPolish`.

Previews use an isolated scene and disposable profiles. A packaged full-level playthrough, audio audition and final combat balance remain unverified. The existing UpgradeRarityGlow material reports a missing texture parameter/default material fallback; source automation reports retain that warning and native fixture warnings.

## Shared meter

The meter starts empty, caps at **100**, gains **20 per successful swap** and **10 per successfully committed assist**, and spends its full charge on activation. Failed swaps and pending/failed assists give no charge. All inactive-character assists, including Tag Team and stance-triggered assists, grant once per committed assist, regardless of how many targets it hits. `AssistGain` is separate from the unchanged `SynergyGain` (25) used by explicit non-assist synergy triggers. The meter does not decay and remains shared when changing characters. Boss-map travel preserves it; a new run starts empty.

## Meter and HUD tuning

In **BP_SurvivorPlayerController > ComboAbilities**, edit **Max Combo**, **Swap Gain**, and **Synergy Gain**. Future synergy implementations call **Notify Synergy Attack** once after successfully triggering, or **Add Combo** for a custom award. Do not also call this for Tag Team; its existing event is already bound.

The in-run HUD adds a circular badge at bottom center. The equipped definition's **Presentation > Icon** sits inside an ivory ring that fills clockwise from the top. Charge is shown as a percentage; a full meter turns gold, reads READY, and gently pulses without moving or resizing the icon. Spending the meter clears readiness immediately. The key badge follows the current Combo Ability binding, and the ability name stays below it. Locked and unequipped states remain dim and cannot flash.

Adjust **Player HUD > Combo** position, size, charging/ready colors, **Combo Label Font**, **Combo Glow Intensity**, and **Combo Glow Pulse Speed** on `WBP_PlayerHUD`. Set glow intensity to zero for a steady ready indicator. The circle and icon preserve their proportions in differently sized slots. The ring is drawn in Slate and needs no generated texture or UI material. Dash and swap use smaller matching circles; see [Action HUD](ActionHUD.md).

Disable **Show Combo Meter** to supply a completely custom HUD using **Get Combo Percent**, **Is Combo Ready**, and the component's **On Combo Changed** event. Legacy `ComboMeterBar` bindings are kept updated but hidden when the native circular badge is enabled.

Automation coverage: `HeavensDivide.Combat.ComboHUD` checks actual meter integration, clockwise rendered sectors, locked/ready/spent states, icon assignments and the shipped HUD at 720p/1080p. Run with `-ComboMeterScreenshots` and an active rendering backend to export previews into `Saved/RadialComboHUD/Previews`.

The Editor build and five focused checks pass. The final HUD rerun includes full-ring visibility checks; nine final previews were reviewed. Evidence: `Saved/RadialComboHUD/ValidationSummary.json`. Packaged full-level playtesting remains unverified.

## Blood Moon effect footprint

Crimson Tempest retains the stable asset name `DA_Combo_BloodMoon`. Its slash damage and VFX now share **Joint Combo > Blood Slash > Blood Slash Anchor** plus **Presentation > VFX Offset**. **Blood Slash VFX Radius** describes the effect radius at component scale 1 (234 cm for the current `NS_Slash_BloodStance` mesh). **VFX Scale**, **Area Multiplier** and the Samurai's attack area upgrades multiply both the damage radius and the rendered effect. The current 2x effect scale and 1.6x area multiplier produce a base radius of approximately 749 cm.

Changing the Niagara component scale or offset keeps the hitbox aligned automatically. Replacing the Niagara system or changing its internal particle scale/mesh requires calibrating **Blood Slash VFX Radius**. Cosmetic particle bounds do not determine damage. Matching Blood builds retain their native critical, Bleed, mark and direct-hit upgrade behavior; their normal-sized Blood Echo cosmetic no longer overlays the combined slash.
