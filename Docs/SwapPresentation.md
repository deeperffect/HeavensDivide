# Character swap presentation

A successful swap starts a configurable 0.5-second presentation freeze. World simulation nearly stops while the incoming mesh and its Niagara burst play at normal speed. Movement input, attacks, dashes and repeated swaps are blocked during the entrance; normal attack cooldowns are preserved and attacks resume once both the freeze and the complete entrance animation have finished. Possession still changes immediately.

## Working defaults

- **Swap > Timing > Swap Freeze Duration** defaults to **0.5 real seconds**. Set it to **0** to disable the freeze.
- **Freeze Ease In Duration** defaults to **0.12 seconds**, included inside the total freeze window. A smooth curve slows the world from its previous speed to near-zero, then holds it there. Set this value to **0** for the original instant freeze. Entrance animation and arrival Niagara remain at real-time speed throughout the ramp.
- Ninja drops visually from above. **Swap > Arrival Drop** exposes **Enable Arrival Drop**, **Arrival Drop Height** (180 cm), and **Arrival Drop Duration** (0.35 seconds). Existing Ninja tuning is preserved.
- Samurai walks forward into position instead. **Swap > Samurai Walk In** exposes **Enable Samurai Walk In**, **Arrival Walk Distance** (180 cm), and **Arrival Walk Duration** (0.6 seconds without a montage). He starts behind his facing direction at ground height and moves forward at constant speed. With an entrance montage, movement lasts the full animation duration at its configured play rate, even beyond the freeze. Assign an in-place walking montage to **Animation > Entrance Montage**, with root motion disabled; adjust distance to match the stride.
- Both arrivals move only the visual root and attached mesh/weapons; the capsule, camera target and actor position are unchanged. The original visual offset is restored on completion or cancellation.

- A snapshot of the outgoing character and its static-mesh equipment fades over 0.20 seconds.
- The incoming character gets a short Niagara burst: violet smoke for Ninja and a red-black ink slash for Samurai. Clearing Arrival VFX restores the 0.25-second fallback ring.
- An existing swish sound plays with a character-specific accent. These are temporary choices from the game's sound library.
- The incoming HUD portrait enlarges to 115%, brightens and settles over 0.25 seconds.
- Grand Entrance can show the optional Ready VFX while its enhanced attack is ready. It never changes character or weapon materials.
- Tag Team characters retain their original materials throughout the assist. Ninja uses her equipped Returning Fang, Great Shuriken or projectile volley; an assist Fang completes one outbound/return cycle.

The smoke and ink slash are assigned on the character Blueprints. Entrance montages remain optional.

## Smoke and ink slash assets

Open `Content/HeavensDivide/VFX/Swap`:

| Asset | Effect / tuning |
| --- | --- |
| `NS_SwapVioletSmoke` | 14 dark violet puffs expand, drift outward/upward, rotate, and fade over 0.38–0.58 seconds |
| `NS_SwapInkSlash` | Two overlapping red-black strokes rotate and stretch, then disappear after 0.18–0.25 seconds |
| `M_SwapSmoke` / `M_SwapInkSlash` | Translucent materials with adjustable `Shape`, `InkColor` and `EdgeBrightness` |

Both systems use one lightweight Niagara emitter with a single burst, no collision, no lights and no continuous spawning. Open the system to edit **Initialize Particle** (lifetime/size), **Add Velocity**, **Sprite Rotation Rate**, **Scale Sprite Size**, and **Scale Color**. `User.SwapColor` controls their edge tint and receives the character color at runtime. The particles start 65 cm above the effect origin; the component's default Z offset of −80 therefore places them around the character's lower body.

The existing `VFX Scale` scales the burst. **BP_Ninja → SwapPresentation → Arrival VFX** uses the smoke; **BP_Samurai → SwapPresentation → Arrival VFX** uses the slash. To also burst on departure, assign the same system to **Departure VFX**; that slot is left empty by the setup script to keep each swap to one burst plus the existing afterimage.

`Tools/create_swap_vfx.py` creates these assets using UE's lightweight burst template and the project's existing smoke/slash textures. It preserves existing generated assets and custom Arrival VFX assignments. Blueprint backups are under `Saved/Backups/SwapVFX`. `SwapVFXSetupLibrary` provides editor scripting access to emitters, unwrapped Niagara properties, color binding and compilation; it adds no runtime tick.

Validation: editor build and Niagara compilation passed. A fresh editor process verified both Blueprint assignments, one-shot loop settings, burst counts, particle lifetimes/sizes, fade-to-zero curves, color bindings and owner scaling. The effects still need an in-game visual playtest. For headless regeneration, set `-ShaderWorkingDir` to a writable project directory such as `Intermediate/SwapShaders`.

## Character settings and asset assignments

Open **BP_Samurai** or **BP_Ninja** under `Content/HeavensDivide/Blueprints/PlayerCharacters`. Select the inherited **SwapPresentation** component in the Components panel. Each Blueprint can have different assets and tuning.

If the editor was open during this C++ build, restart it first so the new native component and settings are loaded.

| Component category/property | What to assign or tune |
| --- | --- |
| Swap > Timing > Swap Freeze Duration | Real-time freeze window; default 0.5 seconds, zero disables it |
| Swap → General → Enabled | Master switch for this character's world presentation |
| Use Character Color / Custom Color | Automatic red/violet, or your own color |
| Afterimage → Enable Afterimage / Afterimage Duration | Departure snapshot and fade time |
| Ghost Material | `Content/HeavensDivide/Materials/M_SwapGhost`; used only for the departure afterimage |
| VFX → Departure VFX | Optional **Niagara System** at the outgoing character's position |
| VFX → Arrival VFX | Optional **Niagara System** at the incoming character's position; replaces the fallback ring |
| VFX Offset | World-space offset from the character actor origin; default Z −80 places the effect near the feet |
| VFX Scale | Uniform Niagara scale and fallback-ring radius multiplier |
| Use Fallback Arrival Ring | Turn off the ring when you want no arrival visual and have no Niagara assigned |
| Sound → Swap Whoosh | Shared transition sound, set separately on each character if desired |
| Sound → Arrival Sound | Character accent; accepts Sound Wave, Sound Cue or MetaSound Source through SoundBase |
| Sound Volume / Enable Sound | Overall swap-sound level and switch |
| Animation → Entrance Montage | Optional montage matching that character's skeleton |
| Entrance Play Rate | Actual real-time speed multiplier, combined with the montage asset Rate Scale. 1 is normal speed and 0.5 is half speed when the asset Rate Scale is 1 |
| Grand Entrance → Show Ready Glow | Enable the pending-enhanced-attack cue |
| Ready VFX | Optional attached Niagara System, active only while Grand Entrance is ready |
| Weapon Component Name | Exact component name for glow/Ready VFX; leave empty to use the autoattack weapon visual, with character-mesh fallback |

For example, create your custom assets under `Content/HeavensDivide/VFX/Swap/` and `Content/HeavensDivide/Audio/Swap/`, then drag them into these fields. Their folder is your choice; the component references are what matter.

Leaving **Arrival Sound** empty uses `/Game/Assets/Sounds/Samurai/Swing1` for Samurai and `/Game/Assets/Sounds/Ninja/freesound_community-knife-draw-48223` for Ninja. **Swap Whoosh** defaults to `/Game/Assets/Sounds/Ninja/freesound_community-knife-swish-1-82559`. Disable Sound to silence both layers.

## Niagara setup

Use short **one-shot** systems for Arrival/Departure, roughly 0.15–0.35 seconds. They should finish automatically; do not assign indefinitely looping systems to these slots. Ground-oriented rings/slashes work well at the default offset; raise the offset toward Z 0 for torso smoke.

Add a **Linear Color user parameter named `User.SwapColor`** if you want the component to supply red/violet/custom color. Use that parameter in your particle color modules. Systems can instead use their own authored colors.

**Ready VFX** may loop. It attaches to the selected weapon component and is explicitly destroyed when readiness ends, the character leaves active mode, or the component ends play. It also receives `User.SwapColor`. Keep it small enough that normal attacks remain readable.

## Swap montages: optional

### Departure montages

Each character also has **SwapPresentation > Animation > Departure Montage**, **Departure Play Rate**. Assign your Samurai exit montage on **BP_Samurai** and your Ninja exit montage on **BP_Ninja**. Use the corresponding character skeleton and a non-additive, full-body montage; the visual copy evaluates its first slot track directly, so no new Animation Blueprint slot wiring is needed. These slots are empty until you assign your exit animations.

Build and swap regression checks passed, including departure playback on both saved character Blueprints, real-time animation advancement, disabled gameplay collision and copy expiry. Existing montages were used only as test fixtures; no exit animation assets were assigned or authored by this change.

The outgoing character becomes inactive immediately. A temporary animated copy plays the departure alongside the incoming entrance, then fades away; socket-attached weapons follow its animation. Its animation uses real time during the swap freeze, ignores root-motion movement and suppresses animation notifies. It cannot attack, collide, or take over Tag Team. Playback speed is Departure Play Rate multiplied by the montage asset Rate Scale (minimum effective rate 0.01). With asset Rate Scale 1, Play Rate 1 is normal speed and 0.5 is half speed. The montage plays fully at that speed, independently of the freeze or incoming entrance. Departure Max Duration is no longer used or exposed. Clearing Departure Montage restores the original static afterimage. The copy and its weapons keep their original materials until the complete montage has played. Only then does Ghost Material apply to the held final pose, which fades over Afterimage Duration (default 0.2 seconds). That fade is additional to the montage window.

### Entrance montages

For Samurai's walk-and-draw entrance, enable **SwapPresentation > Samurai Weapon Draw > Enable Arrival Weapon Draw**. **Weapon Back Socket** defaults to `WeaponBackSocket` on his skeletal mesh. The existing equipped weapon component is temporarily attached there; there is no second weapon mesh. Adjust the socket preview or **Weapon Back Offset** for its position and rotation on the back. **Arrival Weapon Component Name** can override the autoattack weapon lookup if needed.

In the entrance **montage's Notify track**, add a named notify **`SwapDrawWeapon`** at the frame where his hand grips the weapon. No Animation Blueprint event wiring is required: entrance playback reads this marker directly and restores the weapon's original hand parent, socket, relative transform and scale. Put the marker in the montage, not only in its source animation sequence. The first matching marker is used. Without a marker, **Arrival Weapon Draw Time** supplies a fallback in montage seconds (default 0.5). Both timings follow Entrance Play Rate and montage Rate Scale automatically. Completion or cancellation always restores the hand attachment; a missing back socket leaves the weapon equipped safely. This behavior applies only to Samurai's entrance, not attacks, assists or departure.

Assign an optional **full-body, non-additive, in-place montage** matching the character skeleton to **SwapPresentation > Animation > Entrance Montage**. Its first slot track is evaluated directly; no Animation Blueprint slot wiring is needed. Root-motion or incompatible montages are skipped.

The entrance temporarily owns the mesh pose, so idle, locomotion and attack slots cannot blend over it. Playback advances in real time, suppresses gameplay notifies, and ignores early montage blend-out. Every frame through the montage end is evaluated, including the final pose, before normal animation resumes. The Animation Blueprint and normal mesh ticking are restored afterward. Do not use gameplay notifies for attacks, projectiles, dash, damage or swapping in these cosmetic montages.

**Entrance Play Rate** multiplies the montage asset **Rate Scale**. At an asset Rate Scale of 1, Play Rate 1 plays at normal speed and 0.5 takes twice as long. The effective positive rate is clamped to at least 0.01. Playback duration is montage length divided by that rate. Freeze duration never accelerates or truncates the entrance; the old Entrance Max Duration setting is no longer used or exposed. The world resumes after the configured freeze, while a longer entrance continues at its chosen speed. Movement input, attacks, Double Cut, dashes and another swap stay blocked until the entrance has finished, including its final displayed frame. Starting the arrival clears queued movement and leftover velocity so the character cannot slide when the freeze expires. The freeze timer can finish first without interrupting the remaining animation. Ordinary game pause suspends playback. Level-up, death, victory, mode changes, disabling presentation and teardown explicitly cancel and restore normal animation.

Do not replace the existing Tag Team attack montages with entrance montages. Tag Team continues to use normal attack animations/notifies, which are required for its gameplay.

## Swap camera focus

Both characters trigger camera focus when their swap freeze starts. In **BP_PlayerCameraRig > Class Defaults > Camera > Swap Focus**, adjust **Enable Swap Focus**, **Swap Zoom Amount** (0.18: 18% narrower field of view), **Swap Zoom In Duration** (0.12 real seconds), **Swap Zoom Out Duration** (0.65 real seconds), and **Swap Vignette Strength** (0.12; zero disables the accent).

The camera smoothly eases in, then eases back more slowly. Focus blends toward the character's current visual position, following Ninja's drop or Samurai's walk, then returns to the normal actor anchor. Tracking continues if the player moves during the return. The effect runs in real time through time dilation and pauses with the game. The return may outlast the freeze; it does not extend gameplay locks. Field of view, vignette settings and camera lag restore afterward or on cancellation/target changes. No new montage, VFX or sound asset is required. Camera lag is temporarily disabled during focus so the near-frozen world cannot stall tracking.

## HUD portrait pulse

Open **WBP_PlayerHUD → Class Defaults → Player HUD → Swap Feedback**:

- Enable Swap Portrait Pulse
- Samurai Portrait Name: `IMG_IconSamurai`
- Ninja Portrait Name: `IMG_IconNinja`
- Swap Portrait Duration: `0.25`
- Swap Portrait Scale: `1.15`

These names match the current HUD images. If you rebuild the HUD, point the names at the incoming character's `Image` widgets. Their original scale and tint are restored after the pulse or when the HUD is removed. Avoid another animation driving the same image's render scale/tint simultaneously. No Widget Animation asset or event-graph wiring is required.

## Files and checks

Samurai weapon draw: editor build and seven checks passed (six swap tests plus GrandEntrance). Tests cover back attachment, preservation of scale, marker precedence over fallback, timing at normal/half speed, exact restoration of a nested hand attachment, cancellation, fallback time, and missing sockets. Place the marker and adjust back-socket alignment against the actual walking/draw montage in the editor.

Arrival/assist materials and Ninja stance assists: editor build and eight checks passed (five swap tests, NinjaBuilds, GrandEntrance, BuildFamilies). Coverage includes unchanged arrival/assist materials, stance-specific assist projectile spawning, single-cycle Returning Fang, independent Shadow Clone projectile identity, and Prepare consumption after the assist character becomes inactive. Ghost Material is now exclusive to the departure afterimage; Grand Entrance retains optional Ready VFX without material overlays.

Swap camera focus: editor build and all five swap tests passed, including the saved BP_PlayerCameraRig with both characters. Coverage verifies eased zoom, tracking moving visuals, FOV/orthographic-width restoration, vignette/lag restoration, repeat triggers, target loss, actual local-player arrival triggering, and cancellation. Visual framing and effect strength still need an in-editor playtest.

Arrival movement lock: editor build and all four swap tests passed. Both possessed characters reject movement during entrance playback, after freeze expiry, and on the held final frame; movement resumes after completion. Regression checks also verify clearing queued input and residual velocity at arrival start.

Samurai walking arrival: editor build, all four swap tests and Grand Entrance passed. Coverage verifies ground-level movement from behind the facing direction, constant speed, exact finish position, unchanged actor position, cancellation and disabling without falling back to a drop. Both normal and slow montage rates synchronize the walking duration independently of freeze length. Ninja keeps its existing drop behavior and tuning.

Departure speed correction: editor build and all four swap tests passed. Both characters were checked with Departure Play Rate 1/0.5 and montage Rate Scale 1/0.5, at normal and near-frozen world speeds. Tests verify authored playback speed, full duration, original materials before completion, the colored fade afterward, and cleanup.

Arrival speed correction: editor build and all four swap tests passed. Speed regression coverage checks both characters with Entrance Play Rate 1/0.5, montage Rate Scale 1/0.5, and freeze durations 0/0.05/0.5 seconds. Montage position advances at the configured rate before and after freeze expiry; no duration fitting overrides slow playback.

Arrival completion and delayed departure color: the editor build and six checks passed (`SwapEntrance`, `SwapDeparture`, `SwapFreeze`, `SwapPresentation`, `GrandEntrance`, and `SamuraiBuilds`). `SwapEntrance.cpp` owns exclusive entrance pose playback and restores normal animation after the final frame. Tests cover both character skeletons, no early idle blend, a long frame crossing the end, freeze expiry before animation completion, cancellation, normal departure materials during playback, and the separate colored fade tail. Saved Blueprint tuning and montage assignments are preserved. Visual polish still needs an in-editor playtest with your assigned montages.

`SwapArrivalMovement.cpp` owns the Ninja drop and Samurai walk-in. The editor rebuild and all three `HeavensDivide.ImpactFeedback.Swap` tests passed after adding the drop and freeze ease-in. Coverage includes Ninja descent, Samurai walking movement, exact final positions, unchanged collision position, smooth slowdown, and cancellation with or without a freeze. Adjust Ninja height/duration and Samurai walking distance against the assigned montages in an editor playtest.

`SwapPresentationComponent` owns character settings and presentation lifetime. `SwapFreeze.cpp` owns the temporary world dilation, mesh compensation and real-time release. The freeze uses Unreal's minimum time dilation (0.0001), not a menu pause; simulation advances by a negligible amount during it. `SwapAfterimage` copies the outgoing pose and fades independently. `PlayerSwapFeedback.cpp` owns the portrait pulse. `M_SwapGhost` is generated by `Tools/create_swap_presentation_material.py`, which preserves an existing material on reruns. Custom ghost materials must expose vector parameter `Tint` and scalar parameter `Opacity` and support skeletal meshes.

Test both directions while moving, attacking, dashing, and with Grand Entrance and Tag Team acquired. Check that the glow ends when the enhanced attack is spent, assists regain their normal materials, the portrait settles, and rapid swaps leave no stuck effects. Custom montage slots, sound balance, Niagara scale and visual layering need an in-editor playtest.

The swap-freeze build and six focused combat/presentation checks passed. `HeavensDivide.ImpactFeedback.SwapFreeze` verifies the saved Ninja entrance montage starts and advances during the freeze, activation/direct attacks are blocked, the first frame cannot fast-forward the animation, cooldowns remain intact, and expiry/mode changes/teardown restore world and mesh speed. Restart the editor after the native build before testing the updated swap behavior.

The editor build passed. The 12 existing combat/upgrade/damage-feedback tests passed, as did the new `HeavensDivide.ImpactFeedback.SwapPresentation` test and a rerun of Grand Entrance. Coverage includes readiness glow creation/removal, ghost material restoration, afterimage expiry/collision, portrait restoration, and unchanged player position/collision.
