# Abilities and swap synergies

The four automatic ability families share the existing upgrade pool, damage attribution, status components, rewards and enemy death flow. This pass connects their effects to the two-character combat loop.

## Playtest

Start a fresh run. `AbilityShowcase` grants all four abilities, rank 2 Force/Reach/Rhythm and their evolutions, plus Bleeding Edge, Venomous Kunai, Tag Team and Hemotoxic Reaction when eligible. It changes only this run, preserves higher ranks, and repeated use does not add mastery. It is disabled in shipping builds.

1. Play Ninja until a green Garden appears around a crowd. Its first pulse seeds Poison.
2. Swap to Samurai and land a basic melee hit on an enemy inside the ring. The field disappears into a larger golden burst, dealing 150% of its remaining direct damage and applying Bleed to survivors. Misses, passive abilities and assists do not detonate it.
3. Return to Ninja. Night Thread prioritizes bleeding survivors and applies Poison to them. With Hemotoxic Reaction, subsequent player swaps cash in the existing combined Bleed/Poison damage through the original reaction system.
4. With Tag Team, every five qualifying basic attacks brings in the partner for a setup attack. These resolve on the existing montage hit notify, so canceled attacks do not produce early damage.

## Base abilities

| Ability | Damage / area | Timing and interaction |
| --- | --- | --- |
| Steel Tempest | 24 damage, 260 cm radius | 4-second recharge. Razor Halo echoes after 0.3 seconds for 65% damage. |
| Heavenfall | 42 damage, 280 cm radius | 4-second recharge, 0.5-second warning. Selects the densest nearby group; applies Bleed, and Mark with Marked Blade. Starfall targets up to three separate groups with staggered strikes. |
| Night Thread | 25 damage per target, three targets, 360 cm jumps | 3.5-second recharge. Prefers bleeding targets and poisons them. Venomous Kunai also enables Poison on other targets. Black Web adds delayed 50%-damage bursts. |
| Venom Garden | 4 damage per pulse, 340 cm radius | Eight pulses, 1.1 seconds apart, first at 0.1 seconds: approximately 7.8 seconds total. Nine-second recharge; one live Garden. Poisons enemies that do not already have Poison. |

Withering Garden extends the field to eleven pulses (approximately 11.1 seconds), then bursts for three pulses' damage in 25% more radius. Detonation includes that unspent final burst in its payout. The detonation radius is also 25% larger than the original field. It retains the Garden's snapshotted Ninja damage scaling but attributes the finishing hit and Bleed to Samurai. It does not consume Poison or Bleed; Hemotoxic Reaction remains responsible for consuming those statuses.

The field's future pulses are removed before detonation deals damage, preventing duplicate payouts. Already-spent pulses are excluded. Garden seeds one Poison stack rather than adding a new stack every pulse while Poison remains present.

## Setup assists

Tag Team keeps its existing unlock, attack counter, placement, montage and cleanup flow.

- Ninja: a targeted streak fan hits up to five enemies within 1,000 cm, for 10 base damage each and one Poison stack. Targets must be in front of the assistant.
- Samurai: a broad forward slash hits up to twelve enemies within 420 cm, for 12 base damage each, one Bleed stack and 65 cm pushback. Marked Blade additionally marks survivors.

Direct assist damage scales with the assisting character's damage, shared damage and mastery. Intrinsic setup statuses do not require the basic-attack status starter. Their damage and reactions use the existing status component and its status upgrades. Ordinary basic attacks still need their own status starter. Samurai's acquired Blade Wave behavior continues through its existing attack-notify path.

## Upgrade scaling

Each ability has five-rank Force, Reach and Rhythm cards. Common/Rare/Epic magnitudes are respectively:

- Force: +20% / +30% / +45% own ability damage.
- Reach: +12% / +18% / +25% own radius or chain distance. Every two Night Thread Reach ranks add one target, capped at eight.
- Rhythm: +10% / +15% / +22% recharge speed. Cooldown = base / (1 + total bonus).

Evolutions require the ability plus rank 2 Force and Reach. Character offers reserve an eligible starter and support when available. Only the active character casts and advances its ability cooldowns. Existing casts survive swaps. World pause pauses timers; run end cancels pending effects and attacks.

## Implementation and validation

`SurvivorAbilityComponent` owns the 10 Hz scheduler, bounded crowd scoring (at most 128 candidates), setup attacks and Garden payout. `AutoAttackComponent` invokes setup attacks at the existing hit notify and reports successful Samurai melee hit positions after its normal damage resolves. `EnemyStatusEffectComponent` accepts explicit intrinsic status sources while retaining damage-source restrictions. `InactiveCharacterAssistComponent` additionally guards run end.

`Tools/configure_swap_synergies.py` updates only the affected starter/evolution and Tag Team descriptions, preserving other asset settings. Backups are under `Saved/Backups/SwapSynergies`. `Tools/configure_ability_expansion.py` reflects the new descriptions for future asset generation.

Automation covers crowd selection, Garden timing and Poison seeding, partial-damage payout, misses, duplicate hits, status setup, separated Starfall groups, all evolutions, upgrade gating, run-state restoration and end-of-run cancellation. Final feel and balance should be assessed in a fresh run.

Validation completed: Win64 Development Editor build succeeded; all five tests passed (ability expansion/synergies, Samurai pushback, basic melee, Goblin Bomb, enemy death lifecycle). Two fixture warnings concerned teardown world context and missing Goblin Bomb skeletal meshes. Report: Saved/Automation/SwapSynergies/index.json. Upgrade description save verification passed.
