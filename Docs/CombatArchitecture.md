# Combat code and Blueprint settings

See [Enemy architecture](EnemyArchitecture.md) for the enemy roster, native behavior and Blueprint settings.

See [Tester balance](TesterBalance.md) for the main menu's local per-enemy health/speed overrides and run population controls.

See [Swap presentation](SwapPresentation.md) for character handoff effects, optional entrance montages, sounds and HUD portrait feedback.

See [Run reports](RunReports.md) for the pause/results build and damage tabs, score rules, per-map local leaderboards and the Steam integration handoff.

The current routes are Blood, Iaijutsu and Crescent for Samurai, and Returning Fang, Barrage and Great Shuriken for Ninja. See [Build families](BuildFamilies.md) for the current catalog and route behavior. Saved upgrade assets remain the source of balance values.

## Where to work

| Feature | Implementation | Editable settings |
| --- | --- | --- |
| Steam scores, per-map top ten and personal best/rank | `SteamLeaderboardSubsystem.cpp`, `RunReportSubsystem.cpp`, `RunReportView.cpp` | Steamworks board names/settings and packaging verification in [Run reports](RunReports.md); Windows App ID `5283180` |
| Run setup, locked choices and difficulty progression | `StanceSelectionWidget.cpp`, `RunChallengeSubsystem.cpp`, `SynergyMetaProgressionSubsystem.cpp` | Arena has maps/challenges; Loadout has synergies and Samurai/Ninja/combo dropdowns. Victories unlock the next modifier rank per map, starting at zero |
| Stance purchases and vertical skill trees | `MetaSkillTreeWidget.cpp`, `SMetaSkillMap.cpp` | Samurai/Ninja stance categories, Sigil confirmation, larger symbols, vertical scrolling and connected gold paths; Stats ordered Samurai / Shared / Ninja |
| Display resolution and window mode | `DisplaySettingsView.cpp`, `MainMenuWidget.cpp` | Settings > Display; Windowed / Borderless / Fullscreen, supported resolutions, Apply and 15-second Keep / Revert confirmation |
| Level ambience and audio volume controls | `LevelAudioSubsystem.cpp`, `HeavensDivideGameUserSettings.cpp`, `MainMenuWidget.cpp` | Master / Music / Sound Effects in Settings; `SC_Music` for future music. See [Combat audio](CombatAudio.md) |
| Movement input across upgrade choices, chests and pause menus | `PlayerMenuMovement.cpp`, `SurvivorPlayerController.cpp`, `EliteRewardChest.cpp` | Tracks live movement presses/releases through focused UI, then restores only held movement |
| Attack timing, aiming, normal Ninja volleys | `AutoAttackComponent.cpp` | Auto Attack timing, targeting, animation and projectile categories |
| Samurai melee, Double Cut, Blade Wave emission (Crescent uses presentation montages with no normal melee hitbox) | `SamuraiAutoAttack.cpp`, `AutoAttackComponent.cpp` | Samurai melee trace, Double Cut, Blade Wave and pushback categories on AutoAttackComponent; Crescent Montage and Crescent Alternate Montage under Blade Wave |
| Iaijutsu spectral dash and player animation | `SamuraiIaijutsu.cpp`, `SamuraiAutoAttack.cpp`, `SwapAfterimage.cpp` | Iaijutsu data asset: `DashDistance`, `ChargeDuration`, `Cooldown`, `SlashRadius`; Iaijutsu Montage animates the real Samurai; indicator and hit VFX on AutoAttackComponent |
| Samurai Bleed transfer and overkill explosions | `SamuraiBuildUpgrades.cpp` | Corresponding Samurai upgrade data assets |
| Moving Blade Waves, split waves, Crescent fields | `SamuraiBladeWave.cpp`, `SamuraiWaveField.cpp`, `CrescentBuild.cpp` | Crescent upgrade data assets; field Runtime VFX on Lingering Wake |
| Crescent slow | `EnemyBase.cpp`, `EnemyLightweightMovementComponent.cpp`, `SamuraiBuildUpgrades.cpp` | Crescent stance `SlowFraction` / `SlowDuration`, Increased Slow and Slow Duration cards |
| Blood Rush movement bonus | `SamuraiCharacter.cpp`, `SamuraiBuildUpgrades.cpp`, `EnemyBase.cpp` | Blood Rush `MoveSpeedBonus` / `Duration`; death event and one expiry timer; clears on swap/restoration |
| Ninja stances, Barrage volley cadence, weapon spawning | `NinjaBuildComponent.cpp`, `NinjaFangBuild.cpp`, `BarrageBuild.cpp` | Ninja upgrade data assets; Kunai Throw Sound on NinjaBuildComponent |
| Returning Fang, giant shuriken movement, kunai presentation | `NinjaBuildProjectile.cpp` / `.h` | Weapon upgrade data assets, including Great Shuriken `TravelSpeed` |
| Embedded blades and death scatter | `NinjaEmbeddedBlades.cpp` | Embedded Blades and its support cards |
| Normal kunai impacts, poison, marks, forking | `AttackProjectileBase.cpp` | BP_NinjaProjectile impact feedback and projectile settings |
| Shadow Clone attacks and lifecycle | `ShadowClone.cpp` | Shadow Clone timing/combat defaults and its three upgrade cards |
| Blade Wave previews and retained Splinter Wave support | `SurvivorBuildFamilies.cpp` | Blade Wave cards; disabled cards remain unavailable |
| Tag Team hits and shared effect spawning | `SurvivorAbilityComponent.cpp` | Tag Team data asset |
| Temporary rings, beams and Niagara presentation | `AbilityAccent.cpp` | Upgrade presentation settings |
| Status stacks and timers | `EnemyStatusEffectComponent.cpp` | Enemy status settings and acquired Bleed/Poison upgrades |
| XP pickup grouping, healing drops and magnet/healing urn frequency | `ExperienceDropSubsystem.cpp`, `EnemyBase.cpp`, `EliteRewardChest.cpp`, `HealingPickupDropSubsystem.cpp`, `EncounterDirector.cpp` | Project Settings > Game > Experience Pickup Drops / Healing Pickup Drops; chest Orb Count; director Healing / Magnet schedules. See [Gameplay polish](GameplayPolish.md) for XP conservation and current rates |
| Trial-only stance/weapon rewards and normal-offer filtering | `PlayerUpgradeSelection.cpp`, `PlayerUpgradeRules.cpp` | Samurai/Ninja trial Reward Choice Count; existing route upgrade assets retain their balance settings |
| Shared damage/speed/size conversion | `PlayerUpgradeRunState.cpp`, `IaijutsuBuild.h`, `CrescentBuild.h` | Pre-stance card rarity tuning and destination card `PerRank`; rank caps, bonus strength, mastery and banishments survive stance selection |
| Barrage poison and poison pools | `BarragePoison.cpp`, `BarragePoisonPool.cpp` | Barrage stance and support-card balance parameters |
| Blood Shrine, Ember Circle and wall-event intros/progress | `PlayerObjectiveIntro.cpp`, `ObjectiveIntroWidget.cpp`, `BloodShrine.cpp`, `BloodShrineWidget.cpp`, `SurvivalFieldObjective.cpp` | Shared smoke reveal/hold/dissolve timing in `ObjectiveIntroWidget.h`; Blood Shrine `ChallengeDuration` / `RequiredBlood`; circle `HoldSeconds`; `M_EmberCircleProgress` clockwise blue/yellow fill |
| Wall rocks and sequential rise | `EncounterWall.cpp`, `SurvivalFieldObjective.cpp` | Six Japanese Shrine rock meshes; `WallWaveSeconds` 0.75 / `WallRiseSeconds` 0.22; wall ring hidden, Ember Circle progress ring appears on interaction |
| Objective spawn popups and directional arrows | `ObjectiveGuidanceSubsystem.cpp`, `ObjectiveGuidanceWidget.cpp`, `MinimapMarkerComponent.cpp` | `MarkersPointersV1` arrow texture; three-second popup duration in `ObjectiveGuidanceWidget.h` |

Dropdown controls and open lists use solid gold borders; stance icons keep a square slot and preserve their image aspect ratio. Committed assists grant 10 combo points (previously 25), plus existing charge bonuses, using `AssistGain` on `ComboAbilityComponent`. The Editor build and eleven focused checks pass; tree connection, column-order, charge and visual evidence is in `Saved/MenuTreePolish/ValidationSummary.json`.

Run preparation separates Arena (map and difficulty) from Loadout. Loadout keeps synergies on the left and Samurai stance, Ninja stance and combo selectors on the right. All stance options remain visible; unavailable choices have chains and locks. Each map starts with modifier rank 0, and a victory unlocks the next rank above each modifier used, capped at 5. Replaying a lower rank does not skip ranks. Stance purchases require a Soul Sigil confirmation. Skill trees use larger symbols, vertical scrolling and gold selected paths. Build and nine focused checks pass; previews and save-persistence evidence are in `Saved/LoadoutMenus/ValidationSummary.json`.

Settings > Display is shared by the main menu and pause menu. Windowed and Fullscreen offer available resolutions; Borderless uses the desktop resolution. Apply previews both choices, and Keep saves them. Revert, leaving the page, or a 15-second real-time timeout restores the working mode, including a manually resized window. The countdown continues while gameplay is paused. Actual display changes are enabled in Standalone and packaged games; PIE keeps its editor-owned viewport.

Display validation: the Editor build and five focused checks pass (Display, Keybinds, MasterVolume, DisplaySettingsVisual and PauseMenu). Both 720p and 1080p previews were inspected. The display check uses an isolated apply callback, tests the native config save path with disk writes blocked, and restores the config cache afterwards; physical monitor changes were not exercised. Evidence is in `Saved/DisplaySettings/ValidationSummary.json`.

Ground-following Crescent waves use the authored EnemyGround surface from elevated assist origins. Ground anchoring replaces legacy negative presentation Z offsets; `GroundOffset` controls height above the floor. Assist cleanup waits for all committed delayed waves to spawn.

Barrage's normal fan uses a 1.25 spread multiplier (`SpreadMultiplier` on its stance data asset); Procession retains its straight serial volley. Releases are clamped ahead of the Ninja's capsule in the firing direction. `BP_NinjaProjectile` uses the project-owned `NS_BarrageProjectile`: its beam length grows with actual travel distance and its opacity fades after impact/range expiry. Collision and movement stop immediately; the effect remains for at least 0.35 seconds. This avoids the source Niagara's kill-on-deactivate behaviour. `BarrageVFXReview -Fix` creates the effect copy; `Tools/configure_barrage_projectile.py` assigns it with a Blueprint backup.

Ninja's `Alternate Attack Montage` uses `AM_AutoAttackNinja_Left`, whose segment references the baked `AS_NinjaThrow_Left` clip. `Tools/fix_ninja_alternate_montage.py` repairs that nested segment reference with a backup; `-ValidateNinjaAlternate` checks the saved reference and mirrored poses. `HeavensDivide.Combat.NinjaAlternatingThrow` evaluates the montage hand poses as well as selection order and release sockets.

Samurai and Ninja trials grant their respective build-defining stance/weapon choices. The six route starters are excluded from ordinary character offers and unrestricted rewards. Once that character has a route, later trials offer eligible normal upgrades. Branches and support cards remain in the normal pool. See [Build families](BuildFamilies.md#trial-reward-routing).

`USurvivorAbilityComponent` keeps its existing reflected class name so saved Blueprints still resolve the component. It contains assist, Blade Wave and presentation support. Prepare and the automatic cast scheduler are retired; the remaining 0.1-second timer cleans up presentation when a run ends.

Blood Shrine and Ember Circle require interacting with their central mesh. The wall event introduces itself automatically when its ring spawns. All three pause the world for a 1.2-second smoke reveal, 2-second reading hold and 1.1-second smoke dissolve; the reveal and dissolve move left to right. A real-time ticker advances the UI while world timers, physics and AI are paused. The challenge begins after the overlay disappears. Objective removal and run teardown cancel the overlay and release its pause. The wall message is "Survive or break out.. if you can.."; its full 20-second lifetime and wall warning begin after the intro, and its old world-space labels are hidden.

Blood Shrine displays "You've enraged the spirits. Kill 30 within 30 seconds" and counts one kill per Bloodbound enemy; ordinary enemies do not count. During the challenge, a large `number/30` counter stays at the top center of the UI, with the remaining time underneath. `RequiredBlood` and the existing progress API names remain for saved Blueprint compatibility. `Tools/configure_objective_intro.py` persists the 30-second duration in the shrine Blueprint. Ember Circle displays "Survive inside for 20 seconds", has a 1,200 cm radius (previously 800), and uses that same radius for placement validation. It has no world instruction, progress or completion text. Progress increases one second per second inside and drains one second per second outside, clamped at zero. The blue ring fills clockwise with yellow as progress increases, retreating to blue when it drains; zero progress is all blue and full progress all yellow. `Tools/create_ember_circle_progress.py` creates its dedicated material, preserving shared enemy/wall rings. Menus and intros neither advance nor drain progress. Completion retains its existing rewards.

New objectives produce a three-second "Objective spawned: [name]" popup and a player-relative directional arrow using `T_Arrow_01` from `MarkersPointersV1`. Guidance never pauses gameplay. Nearby objectives hide their arrows; an active circle points back toward its center only when the player leaves, and a wall event points toward its boundary until the player escapes. Completed, failed and removed objectives clear their guidance. Spawn notices queue during intro/menu pauses so they do not cover challenge instructions. Objective minimap markers register automatically; field events register after initialization, and the boss gate announces when unlocked.

## Upgrade code organization

| File | Responsibility |
| --- | --- |
| `PlayerUpgradeComponent.cpp` | Queries, acquisition eligibility, normal/debug acquisition and shared acquisition completion |
| `PlayerUpgradeRules.cpp` / `.h` | Stable IDs, retired/disabled-card rules, rank caps and scaling-card groups |
| `PlayerUpgradeRunState.cpp` | Snapshot capture/restoration, stance migration and investment conversion |
| `PlayerUpgradeSelection.cpp` | Category rolls, trial/shrine/discovery rewards, rarity and offer presentation |
| `PlayerUpgradeStats.cpp` | Stat modifiers, mastery multipliers, meta-skill bonuses and diagnostics |
| `UpgradeDraftTools.cpp` | Reroll and banish actions |

These are implementation files for the same reflected component. Blueprint class/property paths and saved card IDs remain stable. Normal and debug acquisition share stat rebuilding, conversion, mastery and notification logic. Retired-family rejection IDs are built once from `BuildFamilyCatalog.h`, and retired ranks/magnitudes cannot reactivate their old effects through run snapshots.

## Removed implementations

- Retired automatic ability pulse queues, cast queues, cooldown arrays, activation stubs, targeting helpers and the old Samurai-to-Venom-Garden callback.
- Cleaver, Duelist and Deathblow attack branches, state, event delegates and editable controls.
- Fan of Blades and Blade Cascade counters, montage selection, volley generators and editable controls.
- Chain Execution and Executioner's Kunai impact branches, target searches, spawned follow-up attacks and debug controls.
- Hemotoxic Reaction and Virulent Strain execution paths, delegates and settings; Accelerated Venom's upgrade listeners and multiplier lookup.
- Unreachable fixed-damage Bleed calculations, the unused damage-budget transfer path, retired Potent Venom scaling, and Bleed-triggered poison timer refreshes. Blood Stance retains its current stack-transfer behavior.
- Crescendo's unused consecutive-volley state and unused direction parameters across player, assist and clone volleys.
- The unused generic family-status application helper. Current Blood and Barrage status paths remain separate.

Serialized enum slots and old run-state padding remain where they protect compatibility. Obsolete status tuning properties remain serialized under `Enemy|Status|Legacy` with deprecation metadata; they do not control current damage. Retired upgrade IDs and unavailable catalog entries remain as rejection records, preventing old references from reintroducing removed upgrades. Active Handoff, Grand Entrance, Tag Team, swap dash restoration and Marked Blade remain intact.

## Runtime work

- Status application no longer attaches per-enemy listeners for the retired Accelerated Venom upgrade.
- Ninja hit queries deduplicate with a set while preserving the original candidate order before sorting.
- Giant shuriken limit checks count existing blades directly instead of allocating a filtered projectile array.
- Embedded-blade mesh assets are loaded once, and the Ninja's attack component lookup is cached weakly.
- Duplicate embedded-blade cleanup was removed from component shutdown.

Projectile movement remains frame-based. Attack timers, the presentation cleanup timer, status tick intervals, targeting limits and visual lifetimes retain their existing behavior. These are reductions in unnecessary work, not measured frame-rate claims.

## Auditing and verification

`HeavensDivide.Combat.BlueprintAudit` inspects Blueprint graph references and compiles the saved combat Blueprints without saving by default. With `-CleanupCombatBlueprints`, it also removes completely unconnected standard event nodes from BP_Samurai, BP_Ninja and BP_NinjaProjectile, after backing up each asset under `Saved/Backups/CombatBlueprintCleanup`.

`HeavensDivide.Enemies.BlueprintAudit` compiles enemy Blueprints and compares editable defaults to `Saved/EnemyCleanupBaseline.txt`, when present. A snapshot from an earlier gameplay pass is not a valid baseline for later tuning changes. The October 2026 source cleanup preserves the earlier snapshot and its session baseline under `Saved/Backups/ProjectCleanup/2026-10-05`.

Run the `HeavensDivide.Combat`, `HeavensDivide.Abilities`, `HeavensDivide.Enemies` and `HeavensDivide.Upgrades` automation groups. Coverage includes current stance scaling and conversions, Bleed caps and transfers, Barrage poison, independent clone/assist volley counters, offers, rank caps and Blueprint compilation. `HeavensDivide.Upgrades.RetiredRunState` also checks stale family references, orphan retired magnitudes, mastery preservation and both acquisition paths. Double Cut and healing-burst tests read their saved Blueprint assignments instead of obsolete asset paths.

For the full `HeavensDivide` suite, enable rendering and audio initialization (`-RenderOffscreen -AllowCommandletRendering -AudioMixer`). Do not pass `-nosound`: the MetaSound PCM test needs sound-wave sample rates initialized even though it renders samples directly.

Build and automation logs for this pass use the `Saved/Logs/ProjectCleanup` prefix. Offscreen automation does not replace an interactive playtest.

The October 5 cleanup editor build passes. Across the full suite and targeted presentation reruns, 62 of 63 tests pass, including the combat and enemy Blueprint audits and the new retired-run-state regression. All 687 checked project asset hashes stayed unchanged. The remaining `HeavensDivide.Pickups.BurstScale` failure is an existing effect-authoring issue: the currently assigned healing burst keeps the same particle sprite sizes at component scales 1 and 2. The test now reaches that effect instead of failing to load the deleted `NS_HealPickup` path; this source cleanup does not rewrite its Niagara tuning. Reports are under `Saved/Automation/ProjectCleanupFinal` and `Saved/Automation/ProjectCleanupPresentation`.

## Tool maintenance

Keep current route authoring and read-only validation scripts in `Tools`; historical repair scripts may still be needed for their named migrations. `generate_build_catalog.py` generates the native family catalog from JSON. `BuildFamilies.md` is maintained directly: the old hardcoded `generate_build_guide.py` was removed because it overwrote current documentation with retired mechanics. Python bytecode caches are ignored and are not project source.
