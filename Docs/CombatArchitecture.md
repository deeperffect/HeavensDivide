# Combat code and Blueprint settings

See [Enemy architecture](EnemyArchitecture.md) for the enemy roster, native behavior and Blueprint settings.

See [Swap presentation](SwapPresentation.md) for character handoff effects, optional entrance montages, sounds and HUD portrait feedback.

Combat currently uses 55 upgrade cards and the existing attack animations. Bloodhound and Alternating Fans were removed after the cleanup documented below; the remaining cards retain their saved balance values.

## Where to work

| Feature | Implementation | Editable settings |
| --- | --- | --- |
| Attack timing, aiming, normal Ninja volleys | `AutoAttackComponent.cpp` | Auto Attack timing, targeting, animation and projectile categories |
| Samurai melee, Double Cut, Blade Wave emission | `SamuraiAutoAttack.cpp` | Samurai melee trace, Double Cut, Blade Wave and pushback categories on AutoAttackComponent |
| Samurai Bleed transfer and overkill explosions | `SamuraiBuildUpgrades.cpp` | Corresponding Samurai upgrade data assets |
| Moving Blade Waves | `SamuraiBladeWave.cpp` | Blade Wave upgrade data assets and Samurai attack defaults |
| Ninja stances, Crescendo, weapon spawning | `NinjaBuildComponent.cpp` | Ninja upgrade data assets; Kunai Throw Sound on NinjaBuildComponent |
| Returning Fang, giant shuriken movement, kunai presentation | `NinjaBuildProjectile.cpp` / `.h` | Weapon upgrade data assets, including Great Shuriken `TravelSpeed` |
| Embedded blades and death scatter | `NinjaEmbeddedBlades.cpp` | Embedded Blades and its support cards |
| Normal kunai impacts, poison, marks, forking | `AttackProjectileBase.cpp` | BP_NinjaProjectile impact feedback and projectile settings |
| Shadow Clone attacks and lifecycle | `ShadowClone.cpp` | Shadow Clone timing/combat defaults and its three upgrade cards |
| Prepare, consumption, Splinter Wave | `SurvivorBuildFamilies.cpp` | SurvivorAbilityComponent's Synergies / Prepare settings and Blade Wave cards |
| Tag Team hits and shared effect spawning | `SurvivorAbilityComponent.cpp` | Tag Team data asset |
| Temporary rings, beams and Niagara presentation | `AbilityAccent.cpp` | Upgrade presentation settings |
| Status stacks and timers | `EnemyStatusEffectComponent.cpp` | Enemy status settings and acquired Bleed/Poison upgrades |

`USurvivorAbilityComponent` keeps its existing reflected class name so saved Blueprints still resolve the component. It now contains only live assist, preparation, Blade Wave and presentation support.

## Removed implementations

- Retired automatic ability pulse queues, cast queues, cooldown arrays, activation stubs, targeting helpers and the old Samurai-to-Venom-Garden callback.
- Cleaver, Duelist and Deathblow attack branches, state, event delegates and editable controls.
- Fan of Blades and Blade Cascade counters, montage selection, volley generators and editable controls.
- Chain Execution and Executioner's Kunai impact branches, target searches, spawned follow-up attacks and debug controls.
- Hemotoxic Reaction and Virulent Strain execution paths, delegates and settings; Accelerated Venom's upgrade listeners and multiplier lookup.

Serialized enum slots and old run-state padding remain where they protect compatibility. Retired upgrade IDs and unavailable catalog entries remain as rejection records, preventing old references from reintroducing removed upgrades. Active Handoff, Grand Entrance, Tag Team, swap dash restoration and Marked Blade remain intact.

## Runtime work

- Status application no longer attaches per-enemy listeners for the retired Accelerated Venom upgrade.
- Prepare target prioritization builds one lookup set instead of rescanning every preparation for every candidate. Existing target ordering is preserved.
- Ninja hit queries deduplicate with a set while preserving the original candidate order before sorting.
- Giant shuriken limit checks count existing blades directly instead of allocating a filtered projectile array.
- Embedded-blade mesh assets are loaded once, and the Ninja's attack component lookup is cached weakly.
- Duplicate embedded-blade cleanup was removed from component shutdown.

Projectile movement remains frame-based. Attack timers, the 0.1-second preparation scheduler, status tick intervals, targeting limits and visual lifetimes retain their existing behavior. These are reductions in unnecessary work, not measured frame-rate claims.

## Auditing and verification

`HeavensDivide.Combat.BlueprintAudit` inspects Blueprint graph references and compiles the saved combat Blueprints without saving by default. With `-CleanupCombatBlueprints`, it also removes completely unconnected standard event nodes from BP_Samurai, BP_Ninja and BP_NinjaProjectile, after backing up each asset under `Saved/Backups/CombatBlueprintCleanup`.

The audit covers 55 project Blueprint assets and 10 combat Blueprint compilations. The upgrade baseline contains 57 cards, all present in the pool, with no missing prerequisites or unused upgrade assets. `Saved/CombatCleanupBaseline.json` records the original card hashes and pool order; retained card bytes are checked after cleanup.

Behavior checks cover Samurai stance scaling using saved tuning, Bleed budgets and transfer, overkill, Blade Waves, Double Cut pushback, Ninja stance attacks, clone weapons, forking, Crescendo, poison, Prepare, Grand Entrance, impact feedback and upgrade tuning. Old assertions that hardcoded pre-tuning Samurai values now use the saved card factors.

The cleanup validation passed 13 automation tests and the editor build. All 57 original upgrade asset hashes matched afterward. Nine unconnected event nodes were removed across the three saved Blueprints. No frame-rate benchmark or interactive playtest was performed.
