# Combo abilities

Press **Q** when the shared combo meter is full to activate the current character's ability. Gamepad uses the left face button (X on Xbox / Square on PlayStation). Keyboard binding is editable in Settings > Keybinds > Combo Ability. Existing saves that already assign Q to another action receive an available fallback key.

The meter starts empty, caps at **100**, gains **20 per successful swap** and **25 per successfully triggered Tag Team assist**, and spends its full charge on activation. Failed swaps and pending/failed assists give no charge. Tag Team grants once per assist, regardless of how many targets it hits. The meter does not decay and remains shared when changing characters. Boss-map travel preserves it; a new run starts empty.

| Character | Ability | Initial tuning |
| --- | --- | --- |
| Samurai | Tornado | Damage area grows from 360 to 1,000 cm over 3 seconds; 12 damage per pulse, every 0.2 seconds. Spinning comes entirely from the assigned montage. |
| Ninja | Thousand Cuts | Rapid AOE pulses in a 650 cm radius for 1.4 seconds; 6 damage per pulse, every 0.08 seconds. Uses the large-AOE version, without teleports. |

Both areas follow the character. Activation triggers the same world freeze as swapping; the ability's animation, damage pulses, and Niagara effect continue in real time during it. Movement resumes after the brief freeze; basic attacks, dash, and swap remain blocked during the ability. Damage scales with character damage, shared damage, and the matching Samurai/Ninja power upgrades, and respects enemies restricted to a particular character's damage. These pulses do not count as basic attacks or synergy triggers. Death or the end of a run cancels the ability, restores time, and clears the meter.

## Editing the ability

Open **BP_Samurai** or **BP_Ninja**, then **Class Defaults > Combo Ability**. Each character has independent settings:

- **Display Name / Enabled**: HUD name and activation toggle.
- **Damage Per Pulse, Initial Radius, Final Radius**: damage and linear area growth.
- For Ninja pulse tuning, edit those radii (set both equal for a constant AOE), **Effect Duration**, and **Pulse Interval** here.
- **Effect Duration / Pulse Interval**: duration and hit frequency. The first pulse occurs when the effect starts; subsequent pulses occur at the interval through the duration. Changing the interval changes total damage.
- **Effect Delay**: windup before damage, sound, and VFX start.
- **Freeze On Activation / Freeze Duration / Freeze Ease In Duration**: independent per-character ability settings under **Combo Ability > Freeze**, defaulting to 0.5 / 0.12 real seconds. Zero duration disables the ability freeze. Ease-in is capped at the chosen duration. Recovery lasts at least through the freeze. Swap tuning is unchanged; pause, cancellation, and time restoration still use the shared freeze system.
- **Recovery Duration**: minimum action duration; automatically extended to cover the effect and the montage's played duration.
- **Montage / Montage Play Rate**: optional animation. Use a montage slot supported by that character's animation Blueprint. Gameplay pulses use the configured timing and need no attack notifies.
- **Face Enemy Pack / Pack Search Radius / Pack Radius**: enabled for Ninja. On activation, count living enemies around each nearby candidate and face the center of the largest group. Equal-sized groups prefer the closer center. Defaults: search within 1,200 cm, group neighbors within 350 cm. The chosen target overrides cursor facing until the ability ends or is cancelled. With no nearby enemies, activation leaves facing unchanged.
- **Sound / Sound Volume**: optional sound played once at effect start, attached to the character and stopped when the ability ends.
- **VFX / VFX Offset / VFX Rotation / VFX Scale**: optional Niagara effect attached to the visual root. `VFX Radius Parameter` (default `User.Radius`) receives the current radius divided by **VFX Reference Radius** (default 1 cm). It updates before activation and every frame, interpolating between damage pulses. `VFX Duration Parameter` (default `User.Duration`) receives the effect duration in seconds. The assigned system must implement those user parameters to use them. The component cleans itself up when Niagara finishes, with the completion timeout as a fallback.
- **Fallback Color**: colors the native pulse rings used when no Niagara system is assigned.
- **VFX Visibility Duration**: time from VFX spawn until graceful deactivation; zero uses Effect Duration. Can outlast ability recovery without extending damage or blocking input. Niagara systems that implement the duration parameter also receive this value. A system's own shorter emitter lifetime still applies.
- **Spawn VFX Every Pulse**: enabled by default for Ninja. Each damage pulse spawns and activates a separate Niagara instance at that pulse's radius. Each instance has its own visibility and completion timer. With this option enabled, zero **VFX Visibility Duration** uses **Pulse Interval** instead of Effect Duration. Existing particles finish after emission stops. All instances detach when recovery ends and are removed on cancellation. Samurai retains its continuous effect by default.
- **VFX Completion Timeout**: extra time after deactivation for existing particles to finish, default 5 seconds. This is a cleanup limit, not a forced opacity fade; Niagara's inactive response and particle lifetime control the actual finish. After recovery the effect detaches at the attack location. Cancellation, death, and level teardown remove remaining effects immediately.

Presentation asset slots are optional. Pulse rings provide fallback effects; the ability does not procedurally rotate or reset the mesh. Assign a montage for Tornado's spinning. Override the character's **Execute Combo Ability** Blueprint event to replace per-pulse gameplay; call its parent implementation to retain the native AOE damage.

### Samurai tornado sizing

`NS_SamuraiAbility` already connects `User.Scale_All` to its emitter sizing, including Particle Update. BP_Samurai uses that parameter with **VFX Reference Radius = 600 cm**: a 600 cm hitbox gives scale 1, 900 cm gives 1.5, and 1,200 cm gives 2. Its authored size curves, colors, and component scale are preserved. Growth updates in real time during the activation freeze too.

The reference radius preserves the existing visual size at the current starting radius; it is an artistic calibration, not a measured boundary of the smoke and sparks. Lower it to make the VFX larger relative to the damage area, or raise it to make the VFX smaller. Changing Initial/Final Radius automatically changes the VFX size. `Tools/configure_samurai_ability_scale.py` applies this binding without modifying the Niagara asset or other ability settings.

## Meter and HUD tuning

In **BP_SurvivorPlayerController > ComboAbilities**, edit **Max Combo**, **Swap Gain**, and **Synergy Gain**. Future synergy implementations call **Notify Synergy Attack** once after successfully triggering, or **Add Combo** for a custom award. Do not also call this for Tag Team; its existing event is already bound.

The HUD automatically adds a bottom-center ink-slash meter with a dark empty track, ivory fill, Cinzel label, active ability name, key, charge percentage, and READY state. A full meter has a pulsing gold halo, which clears immediately when charge is spent. The glow uses real time, including during a world freeze.

Adjust **Player HUD > Combo** position, size, colors, **Combo Ink Texture**, **Combo Glow Material**, **Combo Label Font**, **Combo Glow Intensity**, and **Combo Glow Pulse Speed**. The generated texture and UI material live under `/Game/HeavensDivide/Blueprints/UI/Combo`. Source art, provenance, and the generation prompt are in [Art/ComboMeter](../Art/ComboMeter/README.md); `Tools/import_combo_meter_art.py` imports the texture and creates the material.

A custom HUD can supply `ComboMeterBar` (Progress Bar) and `ComboAbilityText` (Text Block), or disable the native meter and use **Get Combo Percent**, **Is Combo Ready**, and the component's **On Combo Changed** event.

Automation coverage: `HeavensDivide.Combat.ComboAbility`, `HeavensDivide.Combat.ComboHUD`, `HeavensDivide.ImpactFeedback.SwapFreeze`, and `HeavensDivide.Settings.Keybinds`. Run ComboHUD with `-ComboMeterScreenshots` and an active rendering backend to export empty, half-full, and ready previews into `Saved/ComboMeter`.

### Samurai beam and level-up presentation

Samurai's assigned `NS_VerticalBeam19_Spirals1` plays once at its spawn location. In **BP_Samurai > Class Defaults > Combo Ability > Presentation**, **Attach VFX To Character** and **Spawn VFX Every Pulse** are disabled. Radius/Duration parameter names are empty and VFX Scale is 1, preserving the Niagara asset's authored appearance. VFX Visibility Duration 0 uses the ability duration before graceful deactivation. These settings affect presentation; damage pulses retain their existing behavior. Ninja uses montage-authored effects, as described below.

The level-up burst uses `NS_VerticalBeam22_Cosmic` (Niagara), configurable in **BP_SurvivorPlayerController > Player > Level Up > Presentation**. It retains the presentation delay, active-character following, real-time playback during slow motion, and cleanup before the upgrade menu opens. `Tools/configure_level_up_and_samurai_vfx.py` updates the two Blueprint assignments with backups.

### Ninja montage VFX

**BP_Ninja > Combo Ability > Presentation > Enable Code VFX** is disabled; the optional code VFX slot is retained but emits neither Niagara nor fallback rings. `AM_AbilityNinja` owns four **Ability Niagara (Freeze Aware)** notifies at the authored timings (approximately 0.095 / 0.251 / 0.438 / 0.642 seconds), with their original 0.3 scale.

The notifies register Niagara with the activation-freeze clock before playback. Each slash snapshots the mesh/socket transform and remains at that transform. `/Game/HeavensDivide/VFX/Abilities/NS_NinjaAbilitySlash` is a project-owned copy of `NS_CrossSlash03` with consistent local emitter space so its mesh and particle motion rotate together. Original vendor assets are unchanged. Edit effect/offset/rotation/scale directly on each montage notify. `Tools/configure_ninja_ability_vfx.py` preserves event timing and unrelated notifies, with backups under `Saved/Backups/NinjaAbilityVFX`.

`HeavensDivide.Combat.NinjaAbilityVFX` verifies four independent montage effects, freeze cleanup, fixed spawn transforms, and matching local slash positions/orientations at four facing directions.
