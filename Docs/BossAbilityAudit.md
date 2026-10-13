# Veshra — The Pale Stinger: ability audit

Updated October 12, 2026 against the saved `BP_CinderOracle`, its placed
`Lvl_B1_Arena` instance, the summoned `BP_AshSeer`, and the native spell/hazard
code. This is the active scorpion arena boss. Values below come from saved
assets, including arena overrides, rather than C++ defaults.

Evidence: `Saved/BossSpellPressure/TuningReport.json`; size evidence remains in
`Saved/ScorpionBoss/SizeCorrection/Verification.json`.
Implementation: [CinderOracleBoss.cpp](../Source/HeavensDivide/CinderOracleBoss.cpp),
[EncounterHazard.cpp](../Source/HeavensDivide/EncounterHazard.cpp),
[TacticalEnemy.cpp](../Source/HeavensDivide/TacticalEnemy.cpp).

## Current size and encounter rules

- The mesh is **2x the initial scorpion size**: component scale **1.30**, with
  the existing arena actor scale **1.06**. The capsule radius is **300 cm** and
  half-height **400 cm** before actor scale. The mesh offset is **-400 cm**;
  capsule and mesh bases meet the arena floor.
- Base health is **6,000**. Below **50% HP** (below 3,000 without health
  modifiers), the spell deck adds Eclipse and casts more frequently.
- The first cast is **2 seconds** after combat starts. Subsequent cast starts
  are **3.3 seconds** apart in phase one and **2.5 seconds** apart in phase two.
  The boss stands still for **1.8 seconds** after starting each cast; this
  recovery is included in the interval, not added afterward.
- It chases until the player's center is within **650 cm**, then holds position
  and faces the player. Casts can start while the boss is outside that distance.
- Every boss hazard requests **22 base damage per accepted hit**. Run challenge
  damage modifiers can change that. `SpellDamage` controls these hits.
- Ordinary warning time is **1.35 seconds**, dropping to **1.05 seconds** below
  half health. Nova and Eclipse have their own fixed warning times.
- The boss cannot be displaced by attack pushback. There is no automatic
  swordsman-style phase-transition invulnerability at half health: the saved
  inherited threshold is zero and this class uses its own spell scheduler.

## All seven abilities

| ID | Ability | What it does | Saved shape and timing | How to avoid it |
| --- | --- | --- | --- | --- |
| 0 | Falling Embers | Ten circular blasts form a fixed line through the captured player position. | **200 cm radius**, **240 cm spacing**, **0.24 s stagger**, ordinary phase warning, **0.2 s** active time. | Move sideways off the line. |
| 1 | Three Lances | Three fixed ground lanes fan outward from the boss. The lanes retain their origin even at arena edges. | **2,600 cm long x 340 cm wide**, at **-55 degrees, 0 degrees, +55 degrees**, ordinary phase warning, **0.2 s** active time. | Move into the gaps between lanes. |
| 2 | Hollow Nova | Boss-centered ring with a safe inner pocket. | **1,300 cm outer / 340 cm inner radius**, **0.85 s warning**, **0.18 s** active time. | Reach the inner pocket or leave the outer radius before the faster detonation. |
| 3 | Cursed Ground | A dense grid of fixed circles covers the ground visible through the current gameplay camera. The count adapts to zoom, perspective and aspect ratio; floor outside the arena is excluded. | **240 cm radius**, nominal **540 cm center spacing**, up to **64 circles**. Warnings start over **0.35 s**; ordinary phase warning and **3 s** active duration. Persistent damage interval **0.8 s**. | Find gaps between fields or leave the captured screen area. Moving the camera later does not move the circles. |
| 4 | Summon Guardians | Summons **2 Ash Seers plus 20 ordinary monsters** (Grunts and Floating Skeletons), with a delayed boss-centered blast. | Caps: **4 living guardians**, **60 living horde monsters**. Horde starts around **750-850 cm** from the boss; placement retries around collisions and respects arena/floor bounds. Blast **320 cm radius**, ordinary phase warning, **0.2 s** active time. | Leave the blast and kill casters before their targeted circles land. Living caps limit later casts until summons die. |
| 5 | Spiral Eruptions | Twenty blasts wind outward around a fixed captured player position. | **160 cm radius**, starts **280 cm** away, grows **50 cm** per blast to **1,230 cm**, rotates **55 degrees** each, **0.12 s stagger**, ordinary phase warning, **0.2 s** active time. | Stay near the original center or cross already detonated spaces. Arena containment can shift individual circles. |
| 6 | Eclipse - phase two | A huge ring forces the player into its captured inner pocket; two crossing lanes cut that pocket into safe wedges. | Outer radius at least **10,000 cm**, expanded further if necessary to exceed every arena corner by **256 cm**. Inner radius **290 cm**. Lanes **1,800 x 180 cm**, **45 degrees / 135 degrees**. **1.7 s warning**, ring **0.18 s**, lanes **0.2 s**. | Stay inside the inner pocket and off both lanes. No position inside the arena can escape beyond the outer ring. |

The normal sequence is **0 → 1 → 2 → 3 → 4 → 5**, repeating. Below half health,
the scheduler uses **0 → 1 → 2 → 3 → 4 → 5 → 6**. It keeps the same running
spell index across the threshold; switching the modulo from six to seven can
repeat or skip a position in the sequence at the moment phase two starts.

## Summoned guardian ability

The assigned guardian is **BP_AshSeer**, role **Ash Seer**. Its saved values are:

| Setting | Value |
| --- | --- |
| Base health | **61.6 HP**, before tester/run health modifiers |
| Attack | One fixed circle at the player's captured position |
| Circle radius | **155 cm** |
| Base damage | **12** per accepted hit |
| Cast range | **900 cm** |
| Warning | **1.15 s** |
| Active time | **0.18 s** |
| Initial cooldown | Random **1.2–2.8 s** after its BeginPlay |
| Subsequent cast-start interval | At least **6.35 s**: 5.2 s ability cooldown plus 1.15 s windup; movement/range can delay it further |
| Post-cast recovery | **0.45 s**, included within the cooldown |

The guardian chases into range and holds its ground while waiting for its next
cast. It does not heal or haste the boss; those are other tactical enemy roles.
The boss explicitly assigns its current player target and disables permanent-currency
rewards for both guardians and horde monsters. Guardians retain their normal
61.6 HP: killing them before the initial cooldown ends prevents a cast.
Their ordinary hazards are owned by the guardian, so killing it removes its
remaining warnings/fields on the next hazard update.

## Behavior findings

1. **All attacks use locked geometry.** Player-targeted positions and boss-facing
   directions are captured when the cast starts. Later movement does not retarget
   the pattern. Cursed Ground also snapshots the visible floor. Lances and Eclipse
   retain their requested centers across arena edges; ordinary hazards still
   clamp or reject footprints that do not fit.
2. **Summoning happens before its warning resolves.** Guardians are created in
   `CastAbility(4)` immediately, along with the twenty-monster horde. Only the
   boss-centered damage waits for the
   warning. The guardian's own initial cooldown determines when it begins casting.
3. **Persistent ground can overlap later casts.** In phase one the last field
   finishes about **4.70 s** after cast start; in phase two, **4.40 s**. The next
   boss cast starts at 3.3 s or 2.5 s respectively.
4. **Scorpion claws, tail and three orbiting embers are presentation.** This class
   has no separate claw-hit, tail-sting, contact-damage or poison application
   attack. Its combat consists of the seven ground patterns and guardian casts.
   The orbiting ember components have collision disabled.
5. **Inherited swordsman attack settings are inactive in normal scorpion combat.**
   `Tick` deliberately calls `AEnemyBase::Tick`, bypassing `AFinalBossBase`'s
   cleave/AoE/dash/ground-pursuit scheduler. Editing `BossBaseDamage`,
   `ForwardAttackDamage`, `AoEDamage`, `DashDamage` or `GroundCircleDamage` will
   not tune this spell deck. The Blueprint and arena have different saved values
   for some of those inherited fields; the active spell values match.
6. **Ordinary hits do not explicitly interrupt these native spell casts.**
   Hazards are spawned at cast start and resolve on their own timers. Death,
   stopped boss combat and gameplay suspension cancel hazard activity.
7. **Damage has shared safeguards.** Ground warnings have a global minimum of
   **0.6 s**; a burst skips damage if a hitch consumes its complete active window
   before its first visible impact. The player has **0.25 s** shared hit grace,
   so overlapping hazards do not necessarily deal their summed damage.
8. **The boss and its summons can create hazards up to a shared 128-hazard
   scene budget.** This leaves room for screen-wide ground and guardian attacks;
   ordinary enemies retain the global 36-hazard spawn limit. Additional spawns
   can be omitted at their applicable cap. Persistent fields try damage every
   **0.8 s** and do not
   catch up missed ticks after a hitch. Hit tests use **240 cm vertical tolerance**
   and **half the player's capsule radius** as horizontal padding; padding narrows
   ring safe pockets as well as expanding outer borders.
9. **Combat shutdown clears the encounter.** Boss-owned hazards, its casting cue
   and living guardians/horde monsters are removed when boss combat stops. On death, its ember
   visuals hide; victory records immediately, while the authored collapse plays
   before the victory screen freezes the world.
10. **The inner pockets are accessible through the boss's body.** The saved boss
    and both saved player capsules ignore each other's collision channels. The
    arena boss capsule has a scaled radius of **318 cm**; each player's saved
    capsule radius is **34 cm**, producing **17 cm** hazard padding. Away from
    arena clamping, Nova's inner safe boundary for the player center is therefore
    **323 cm**, Eclipse's **272 cm** (also avoid its lanes), and Spiral leaves a
    guaranteed central pocket of approximately **103 cm radius**. Collision
    inspection is saved in `SizeCorrection/ArenaPreview/CollisionAudit.json`.

## Where to tune

Open `BP_CinderOracle` class defaults and use **Oracle** for shared spell damage,
**Oracle > Pacing** for health threshold/cadence/warnings, and the named ability
categories for each footprint and count. Check the placed `Lvl_B1_Arena` actor
for instance overrides. Its active spell values currently match the Blueprint.

Guardian values live in `BP_AshSeer` under **Tactics**. Shared warning, density,
padding and persistent-damage rules live in `DA_GlobalBalance`.
`Tools/boss_pressure_tuning.json` and `Tools/apply_boss_challenge.py` can reapply
the authored boss pressure values. `Tools/activate_scorpion_boss.py` and
`Tools/resize_scorpion_boss.py` now both save the corrected 2x size.

## Validation

The Development Editor build and all three selected automation tests passed:
`HeavensDivide.Encounters.ScorpionBoss`, `HeavensDivide.Encounters.Mechanics`,
and `HeavensDivide.Combat.TagTeamRegression`. They cover saved size, floor-relative
mesh placement, locomotion, casting/death animation, pushback immunity, all seven
spells producing warnings, grounded/capped summons and combat cleanup.

The resize script also compiled the Blueprint, reloaded the saved arena, and
compared the then-current 41 editable numeric Oracle settings and inherited combat settings
before/after. Logs and the automation report are under
`Saved/ScorpionBoss/SizeCorrection/`.

Idle, casting and final death poses were rendered and inspected in the real
arena. The preview pass verified that rendering did not change the saved map;
images and `Validation.json` are under `SizeCorrection/ArenaPreview/`.

The spell-pressure pass passed the Development Editor build and all four tests:
`HeavensDivide.Encounters.BossSpellPressure`,
`HeavensDivide.Encounters.ScorpionBoss`, `HeavensDivide.Encounters.Mechanics`,
and `HeavensDivide.Combat.TagTeamRegression`. It validates the saved Blueprint and arena overrides and
updates their balance-reset baseline. `HeavensDivide.Encounters.BossSpellPressure`
covers viewport geometry, all changed spell counts/timings, Eclipse at an arena
edge, a boss-spawned guardian casting and dealing damage, the twenty-monster
horde, repeated living caps and encounter cleanup. Reports live under
`Saved/BossSpellPressure/`.

Rendered native Lances, Cursed Ground, Eclipse and Summon patterns were inspected
in the saved arena without changing the map. The preview produced 35 Cursed
Ground circles and 22 summoned monsters; Eclipse retained a visible inner
pocket and crossing lanes. This editor preview uses the headless camera fallback;
gameplay reads the active player viewport. Images and the review record are in
`Saved/BossSpellPressure/Preview/`.
