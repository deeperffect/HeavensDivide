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

Ability activation and pulse sounds are separate editable fields on each
character's Combo Ability. Upgrade proc sounds use the existing Presentation
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

MetaSound builder API reference: https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/MetaSoundBuilderBase?application_version=5.7

Validation completed: editor build succeeded; saved-asset verification found 36 key bindings and reviewed all 57 cards; all 56 MetaSounds rendered twice with audible, finite, unclipped output, successful completion, and different outputs on repeated plays. All 11 focused automation tests passed. Original audio assets have no version-control changes.
