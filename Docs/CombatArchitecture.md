# Combat code and Blueprint settings

See [Enemy architecture](EnemyArchitecture.md) for the enemy roster, native behavior and Blueprint settings.

See [Tester balance](TesterBalance.md) for the main menu's local per-enemy health/speed overrides and run population controls.

See [Swap presentation](SwapPresentation.md) for character handoff effects, optional entrance montages, sounds and HUD portrait feedback.

The current routes are Blood, Iaijutsu and Crescent for Samurai, and Returning Fang, Barrage and Great Shuriken for Ninja. See [Build families](BuildFamilies.md) for the current catalog and route behavior. Saved upgrade assets remain the source of balance values.

## Where to work

| Feature | Implementation | Editable settings |
| --- | --- | --- |
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
| Trial-only stance/weapon rewards and normal-offer filtering | `PlayerUpgradeSelection.cpp`, `PlayerUpgradeRules.cpp` | Samurai/Ninja trial Reward Choice Count; existing route upgrade assets retain their balance settings |
| Shared damage/speed/size conversion | `PlayerUpgradeRunState.cpp`, `IaijutsuBuild.h`, `CrescentBuild.h` | Pre-stance card rarity tuning and destination card `PerRank`; rank caps, bonus strength, mastery and banishments survive stance selection |
| Barrage poison and poison pools | `BarragePoison.cpp`, `BarragePoisonPool.cpp` | Barrage stance and support-card balance parameters |

Ninja's `Alternate Attack Montage` uses `AM_AutoAttackNinja_Left`, whose segment references the baked `AS_NinjaThrow_Left` clip. `Tools/fix_ninja_alternate_montage.py` repairs that nested segment reference with a backup; `-ValidateNinjaAlternate` checks the saved reference and mirrored poses. `HeavensDivide.Combat.NinjaAlternatingThrow` evaluates the montage hand poses as well as selection order and release sockets.

Samurai and Ninja trials grant their respective build-defining stance/weapon choices. The six route starters are excluded from ordinary character offers and unrestricted rewards. Once that character has a route, later trials offer eligible normal upgrades. Branches and support cards remain in the normal pool. See [Build families](BuildFamilies.md#trial-reward-routing).

`USurvivorAbilityComponent` keeps its existing reflected class name so saved Blueprints still resolve the component. It contains assist, Blade Wave and presentation support. Prepare and the automatic cast scheduler are retired; the remaining 0.1-second timer cleans up presentation when a run ends.

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
