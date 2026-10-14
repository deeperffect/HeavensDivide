# Combat audio

56 original procedural effects are in `/Game/HeavensDivide/Audio/MetaSounds`.
Their 48 kHz, mono, 16-bit WAV sources are in `SourceAudio/Combat`, with imported
waves under `/Game/HeavensDivide/Audio/Waves`. No third-party recordings were used
in these generated sounds. Original audio assets remain in `/Game/Assets/Sounds`.

## Editing the sound and variation

Each `MS_*` is a real one-shot MetaSound: On Play triggers Random (Float), its
On Next triggers Wave Player, its Value drives Pitch Shift, and Wave Player's
On Finished releases the sound voice. The exposed `Wave`, `PitchMin`, and
`PitchMax` inputs are editable. Pitch is measured in semitones; most attacks vary
by roughly 1-2 semitones, with narrower ranges for musical/UI cues. The random
seed is -1, so playback does not repeat a fixed seed. Source Volume controls mix
level. Source wave peaks stay below 0.65, with RMS capped at 0.105 before event mix.

`Audio/Mix` contains per-category concurrency limits, a shared 20-voice combat
limit, and spatial attenuation for a top-down camera. UI rewards do not compete
with the combat voice pool. Rapid impacts/procs/deaths have retrigger limits.
`DA_CombatAudio` maps native gameplay event names to editable sound assets;
`UCombatAudioSubsystem` loads this palette once per game world. The audio directory
is explicitly included in cooking. Dedicated-server playback is skipped.

## Coverage

- Samurai: normal swing/impact, Double Cut, Blade Wave launch/impact/splinters,
  Crossing Blades, Returning Blade, Overkill Burst, bleed application/transfer,
  active ability start and each damage pulse.
- Ninja: kunai throw/impact, returning-fang relaunch, shuriken throw/impact,
  poison, embedded-blade scatter, shadow-clone spawn, active ability start/pulses.
- Synergies: Grand Entrance, Tag Team, and swap arrival/departure. Other synergy
  modifiers inherit their underlying weapon/swap sound.
- Enemies: melee swings, Ogre windup/slam, Bomb warning/explosion, ranged fireball
  cast/impact. Gorilla's contact damage uses actual player-damage feedback.
- Deaths: small enemies, creatures, skeletons, elites, and boss. Boss death audio
  also plays through the separate montage path; it remains audible on victory.
  Bomb detonation uses its explosion cue; killing it early uses its death cue.
- Boss: warning, cleave, circle AoE, dash, phase transition, pursuit-circle impacts.
- Trials: Samurai lane strikes, Ninja trap hits, completion, objective emergence.
- Player/rewards: dash, damage, death, XP, healing, level-up, upgrade selection,
  elite chest opening.

All 57 upgrade cards were reviewed. Stat-only upgrades share the selection cue
and alter the existing weapon/ability; they do not produce a separate sound per
hit just because a stat modifier is equipped. The test dummy and unrelated
footstep/draw foley retain their existing behavior.

Combined ability release sounds and volumes are editable on the three
`Abilities/Combos/DA_Combo_*` definitions. Upgrade proc sounds use the existing Presentation
Sound field. Enemy death sounds remain editable on EnemyDeathComponent. Ogre and
Bomb expose Windup Audio Event and Impact Audio Event. Weapon animation-notify
timings are unchanged; Double Cut has one dedicated swing sound.

## Reproduction and review

`Tools/generate_combat_audio.py` synthesizes deterministic source WAVs and writes
`Tools/combat_audio_manifest.json`. `create_combat_metasounds.py` imports and builds
the graphs; `configure_combat_audio.py` wires assets and backs up existing assets
under `Saved/Backups/CombatAudio/<timestamp>`. For restoration, use the earliest
backup containing the asset to recover its pre-pass assignment. Source changes
are separately reviewable in version control.

`SourceAudio/CombatPreview.wav` previews 15 sounds at their intended source mix
levels, separated by silence; `CombatPreview.json` lists their timestamps. It is
a source-sample preview, not a recording of an in-game encounter. These are
stylized synthesized effects, not recorded creature voices.

`verify_combat_audio.py` checks saved assignments; `HeavensDivide.Audio.CombatPalette`
renders every MetaSound twice, checks nonzero unclipped PCM and one-shot completion,
and checks that repeated plays vary. Combat regression tests cover damage,
abilities, enemies, rewards, and the Ninja throw notify. Reports go under
`Saved/CombatAudio`. Dense-fight listening in the user's actual camera/mix remains
useful for subjective volume and timbre adjustments.

The elite-chest jackpot adds `MS_ChestJackpot`, separate from the normal opening cue. Its original 3.25-second C-major bell fanfare includes three chord accents timed to the jackpot reward cards. It uses the existing UI voice limit, plays through the paused reveal, and has no world attenuation. Source WAV and synthesis metadata are under `SourceAudio/Combat/ChestJackpot.*`; regenerate/import with `Tools/generate_chest_jackpot_audio.py` and `Tools/import_chest_jackpot_audio.py`. `HeavensDivide.Audio.ChestJackpot` renders this sound twice and checks one-shot completion, audible finite unclipped PCM and pitch variation. Run audio checks with audio enabled, without `-nosound`.

MetaSound builder API reference: https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/MetaSoundBuilderBase?application_version=5.7

Validation completed: editor build succeeded; saved-asset verification found 36 key bindings and reviewed all 57 cards; all 56 MetaSounds rendered twice with audible, finite, unclipped output, successful completion, and different outputs on repeated plays. All 11 focused automation tests passed. Original audio assets have no version-control changes.

## Level ambience and volume controls

Level 1 (`Lvl_B1_Lvl1`) plays `W_Ambience_Lvl1` as a non-spatial looping sound. The original desktop WAV is retained in `SourceAudio/Ambience/Ambience_Lvl1.wav`; `Tools/import_level_ambience.py` imports it and creates the sound classes/mix. The 85.107-second recording loops until its gameplay world is removed. Its Play When Silent virtualization keeps the loop running through Master or Sound Effects mute so turning the volume back up resumes immediately. The main menu has no level ambience. Pauses keep the ambience playing, and level travel destroys the old audio component before a new world starts a loop.

Main-menu and pause-menu Settings expose Master, Music and Sound Effects volume. The three gains persist independently in game-user settings and apply immediately. Master multiplies the Music and Sound Effects gains in the world sound mix (Master x category), with a 0.1-second fade. It does not change Unreal's application gain, which the editor owns and resets during PIE. `SC_SoundEffects` includes combat, menu audio and ambience; `SC_Music` is reserved for future music tracks. Existing project sounds are assigned to `SC_SoundEffects` without changing their recordings or synthesis graphs. `SM_UserVolumes` applies the independent class gains. Future music Sound Waves, Sound Cues or MetaSounds must explicitly use `SC_Music`; new ordinary sounds default to `SC_SoundEffects` through Engine Audio Settings.

`LevelAudioSubsystem` owns the world mix and ambience lifecycle; `HeavensDivideGameUserSettings` stores the gains; `MainMenuWidget` supplies the sliders. The audio import report is `Saved/MenuCombatAudio/AudioImport.json`.

Master-volume fix: `SetMasterVolume` now broadcasts to the active world mix, and loading/applying settings uses the same path. The previous `FApp::SetVolumeMultiplier` implementation was overwritten by `UEditorEngine::Tick` during PIE; that global gain is now left to engine focus/background handling. `HeavensDivide.Audio.UserVolumeMix` exercises an isolated offline audio device with the real ambience loop, verifies actual Effects/Music class gains, master mute after an application-gain reset, live unmute, independent Effects mute, and saved-settings/world-travel restoration without applying Master twice. The loop remains active while muted. The Editor build and all three selected checks (`UserVolumeMix`, `Settings.MasterVolume`, `Combat.MenuBugRegressions`) pass. All 150 current sound assets route through the controlled classes. Evidence: `Saved/MasterVolumeFix/ValidationSummary.json`, `AudioAssets.json`, and `AutomationVerified/index.json`; this is mixer validation rather than a hardware listening review.
