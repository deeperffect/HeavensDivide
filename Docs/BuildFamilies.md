# Build families

See [Combat architecture](CombatArchitecture.md) for source ownership, Blueprint settings, and cleanup verification.

The shared [combo meter and character abilities](ComboAbilities.md) add **Tornado** for Samurai and **Thousand Cuts** for Ninja, activated with **Q** at full charge. Swaps and Tag Team fill the meter; these abilities do not require upgrade cards.

Combat now uses normal autoattacks and **Blade Wave**, which replaces Samurai's melee attacks in Crescent Stance. The saved catalog retains **156 unique pool cards**, of which **148 are currently enabled**. Normal-attack modifiers, status upgrades, Tag Team, Grand Entrance and the other shared upgrades remain available.

The ten automatic ability families and their 70 starter, branch, scaling and evolution cards are retired. They cannot appear in offers, be acquired through stale references, or run through the old automatic cast scheduler.

## Samurai stances and routes

The first Samurai trial offers **Blood Stance**, **Iaijutsu Stance**, and **Crescent Stance**. Choose one per run. Later trials offer eligible ordinary Samurai cards; stance starters never appear in regular offers.

| Stance | Attack behavior | Stat changes |
| --- | --- | --- |
| Blood Stance (`BattleStance`) | Original melee attack, applying Bleed by default. | +35% area and +30% attack speed; damage unchanged. |
| Iaijutsu Stance (`Iaijutsu`) | Charge a spectral lane, then hit it instantly. | No stance stat modifier. |
| Crescent Stance (`BladeWave`) | Automatically launches traveling blade waves with alternating cast montages and no normal melee damage. | No stance stat modifier. |

Stable internal IDs are retained for save compatibility. Blood Stance retains Battle Stance's multiplicative bonuses. The earlier retired `BloodStance`, `ExecutionStance`, and `WaveStance` IDs remain retired. Old Blood/Execution saves map to the current Blood Stance; old Wave maps to Crescent. Iaijutsu takes priority in formerly mixed saves. Unsupported wave/Blood cards are removed on restoration, while mastery totals are preserved.

### Stance data assets

All six stance asset names include `Stance`. Find them under `/Game/HeavensDivide/Upgrades/Samurai` or `/Game/HeavensDivide/Upgrades/Ninja`:

| Stance | Data asset |
| --- | --- |
| Blood | `DA_Upgrade_SamuraiBattleStance` |
| Iaijutsu | `DA_Upgrade_SamuraiIaijutsuStance` |
| Crescent | `DA_Upgrade_SamuraiCrescentStance` |
| Returning Fang | `DA_Upgrade_NinjaReturningFangStance` |
| Barrage | `DA_Upgrade_NinjaBarrageStance` |
| Great Shuriken | `DA_Upgrade_NinjaGreatShurikenStance` |

Stable upgrade IDs and tuning are unchanged. `Tools/rename_stance_assets.py` performs the rename with backups under `Saved/Backups/StanceAssetNames`; `-ValidateStanceNames` checks all six names, the saved pool, and configured compatibility redirects without writing. `HeavensDivide.Abilities.BuildFamilies` verifies that old saved references load the renamed assets. The redirects are retained in `Config/DefaultEngine.ini`.

### Shared investments and stance conversion

**Attack Damage**, **Attack Speed**, and **Area** are available from the start of a run, before selecting a stance. Each is one five-rank investment. Selecting a stance converts its owned ranks into the corresponding card below; future offers continue that same investment rather than starting a second rank track.

| Investment | Before stance / Blood | Iaijutsu | Crescent |
| --- | --- | --- | --- |
| Damage | Attack Damage | Iaijutsu Damage | Wave Damage |
| Speed | Attack Speed | Iaijutsu Charge Speed | Wave Speed |
| Size / reach | Area | Iaijutsu Width | Wave Range |

Ranks, rarity-weighted strength, mastery, and banishments survive conversion and run restoration. Converted bonuses use the destination card's Common/per-rank tuning: two Common Area ranks become two Width ranks (+50%) or two Range ranks (+40%). A Rare or Epic investment retains its strength relative to a Common rank. The previous melee modifier is removed, so converted speed affects charge or wave travel rather than also changing attack frequency. Global modifiers remain independent. Mixed legacy saves merge matching investments; rank caps stay at five while already invested bonus strength is preserved.

`SamuraiHeavyBlade` is now **Attack Damage**, an ordinary five-rank Samurai card granting +20/30/45% at Common/Rare/Epic, without the old attack-speed penalty. Old saves without a stored damage magnitude retain their former +40% investment. Attack Speed retains +10/15/22%; Area retains +15/25/40%. `Tools/configure_samurai_shared_scaling.py` updates the saved damage card and speed name with backups; `-ValidateSamuraiSharedScaling` is read-only. `HeavensDivide.Combat.SamuraiScalingConversion` covers pre-stance offers, trial conversion, rarity, caps, mastery, banishments and repeated restores.

Each stance now has **20 upgrades plus its stance card**: **6 one-time mechanics, 5 normal scalable cards, 6 Rare scalable cards, and 3 Shrine tradeoffs**. The three shared investments count among each stance's five normal scalable cards. See [Samurai stance audit](SamuraiStanceAudit.md) for the full branching trees and balance findings.

### Temporary Samurai upgrade availability

The three stance choices and their Blood, Iaijutsu and Crescent upgrades below are enabled. Shared damage/speed/area investments are available before stance selection, then use the chosen stance's names and effects. Other stance-specific cards become eligible only after choosing their stance. **Overkill Burst**, **Expanding Ruin**, **Wave Volley**, **Crossing Blades**, **Splinter Wave**, **Force**, **Wide Arc**, and **Velocity** are temporarily disabled.

These eight cards cannot appear in normal offers, unrestricted rewards, repeat trials, or previews, and cannot be acquired through saved asset references. Their saved ranks are inactive, including scaling bonuses. Assets, tuning, combat implementations, and compatible saved ranks remain available for future reuse. Ninja, Global, and Synergy cards are unaffected. The disabled-card list is centralized in `PlayerUpgradeRules.cpp` as `IsSamuraiUpgradeTemporarilyDisabled`.

### Blood Stance: attacks and Bleed

Only Blood Stance can apply Bleed, including Samurai assists. Acquiring the stance grants Bleed automatically; Bleeding Edge and Deep Cuts are retired. Crescent and Iaijutsu cannot acquire Blood upgrades or apply Bleed through intrinsic effects. Blood keeps the ordinary damage, attack-speed and area forms of the three shared investments.

Each stack deals **12.5% of its applying hit's damage over the base 3 seconds**, with six **0.5-second ticks**. There is no flat base damage. Critical hits contribute their actual increased hit damage. Additional duration adds ticks at the same damage rate. Every application refreshes all stacks, including applications at the cap; capped applications do not add damage. The cap starts at **5**, increasing to **10** through upgrades.

| One-time upgrade | Effect |
| --- | --- |
| Blood Rush | Killing a bleeding enemy while Samurai is active grants +20% movement speed for 3 seconds. Refreshes without stacking, also triggers on lethal Blood Detonation, and ends on swap. Uses one expiry timer. |
| Blood Transfer | On a bleeding enemy's death, copy its stacks and per-stack damage to nearby valid enemies, respecting their stack cap. Base radius 300 cm, scaled by Samurai area. |
| Double Cut | Every fourth committed attack becomes a full-damage 360-degree slash, using the existing circular slash montage. |
| Echoing Slash | 15% chance to repeat the committed attack after 0.15 seconds for 50% damage, showing only Niagara slash VFX. Echoes apply Bleed/Marked Blade, can independently crit, advance Double Cut and normal attack counters, and can call assists. Echoes cannot produce further echoes. |
| Critical Strike | 15% chance for double attack damage. |
| Ninja Assist | 5% chance per attack to request the Ninja's equipped assist attack. A busy assist cannot overlap itself. |

| Normal scalable upgrade | Per rank | Maximum ranks |
| --- | --- | --- |
| Attack Damage (`SamuraiHeavyBlade`) | +20/30/45% at Common/Rare/Epic | 5 |
| Attack Speed (`SamuraiTempo`) | +10/15/22% at Common/Rare/Epic | 5 |
| Area (`SamuraiArea`) | +15/25/40% at Common/Rare/Epic | 5 |
| Bleed Duration (`LingeringWounds`) | +1 second | 3 |
| Bleed Stacks per Hit (`Bloodletting`) | +1 stack | 5 |

| Rare scalable upgrade | Per rank | Maximum ranks |
| --- | --- | --- |
| Max Bleed Stacks | +1 maximum stack | 5 |
| Blood Transfer Area | +10% radius; applies to detonation radius when that tradeoff is owned | 5 |
| Double Cut Frequency | One fewer attack; ranks yield 3/2/1 | 3 |
| Critical Chance | +10 percentage points | 5 |
| Echo Chance | +10 percentage points | 5 |
| Assist Chance | +5 percentage points | 5 |

Chance upgrades require their one-time unlocks. Blood Transfer Area requires Blood Transfer or Blood Detonation.

Blood Shrine objectives offer these one-time tradeoffs only when Blood Stance is selected. They are excluded from normal and unrestricted rewards:

- **Crimson Power**: +70% Bleed damage and -30% attack speed.
- **Frenzied Blood**: +40% attack speed and -25% attack damage.
- **Blood Detonation**: reaching maximum stacks consumes Bleed and immediately explodes for **200% of its remaining damage**, hitting the affected enemy and nearby valid enemies. Disables Blood Transfer. Transfer-area ranks increase the explosion radius. Explosion damage applies no Bleed.

Tradeoff stat modifiers multiply after additive upgrades. The three tradeoffs can combine across Shrine rewards. In **BP_Samurai > AutoAttackComponent > Samurai > Blood Stance**, **Blood Echo VFX** and **Blood Echo Delay** control echo presentation.

### Iaijutsu Stance: directional spectral dash attacks

**Iaijutsu Stance** is one of the three mutually exclusive Samurai trial stances. Normal attacks create a spectral Samurai that uses the selected targeting mode and moves along an **800 cm** lane during a **1-second base charge**. A cyan ground indicator fills during the charge. At charge completion, the entire lane hits instantly, damaging each enemy currently inside once for full normal attack damage; no damage travels with the afterimage. Enemies that leave before completion escape the hit. A separate **0.5-second base cooldown** follows the charge. The real Samurai plays the Iaijutsu montage during the charge, with root motion disabled and no facing lock, and remains free to move or dash. During a normal charge, the lane follows the Samurai's current position and aim: Auto Targeting enabled aims at the nearest valid enemy, while disabled follows the cursor/controller aim, including empty space. Mouse aiming reads walkable ground and ignores mobs, pawn-owned weapons, and non-walkable sides of world geometry; empty space falls back to a plane at the active character's feet. Setting changes take effect during the charge. The indicator, spectral copy, vacuum, and final hitbox stay aligned; moving or turning does not restart the charge. Damage and Endpoint Burst use the final lane at release. Double Cut follows as one X with a shared midpoint and aim. If no valid target remains, the charge keeps its last direction while following the Samurai and still completes. Death Cascade retains its fixed victim origin and selected lane; Dash Draw retains the completed dash path.

The slash lane has an **100 cm base half-width** (200 cm full width), scaled by Iaijutsu Width. Iaijutsu Damage and Iaijutsu Charge Speed scale hit damage and shorten charging. Shared Samurai damage, attack-speed and area investments convert into Iaijutsu Damage, Charge Speed and Width when this stance is chosen or a saved run is restored. Future offers use those forms and retain the existing ranks and bonuses. Global stat bonuses still apply; global attack speed shortens the cooldown. Iaijutsu itself applies no stat bonuses or penalties. Marked Blade works on its hits. Overkill Burst is temporarily disabled. Bleed and Blood-specific upgrades are unavailable. Blade Wave is a separate exclusive stance and cannot be acquired alongside Iaijutsu. Grand Entrance adds its existing circular strike when consumed. Tag Team retains its existing Samurai assist slash.

The real Samurai plays `AM_SamuraiIaijutsu` once per normal attack, including Double Cut, fitted to the committed charge duration. **BP_Samurai > AutoAttackComponent > Samurai > Iaijutsu > Iaijutsu Montage** controls this animation; empty disables only the player presentation. The montage scales with **Charge Speed**, not ordinary attack-speed modifiers: its full length fits `ChargeDuration / (1 + Charge Speed bonus)`, with the slower-charge pact applied afterward. At the 1-second base charge, five ordinary +15% ranks yield about 0.57 seconds. Pre-stance Attack Speed ranks convert into Charge Speed when Iaijutsu is chosen; separate character/global attack-speed modifiers affect only the post-charge cooldown. Instant casts still hit immediately and play the montage at its authored speed. Root motion is disabled, and montage damage notifies cannot add a normal melee hit. The spectral character retains the original normal attack animation and spectral material, without gameplay notifies. Dash Draw and Death Cascade animate their spectral lanes without replaying the player montage or interrupting a dash; Endpoint Burst retains its separate spectral montage slot. Tune `DashDistance`, `ChargeDuration`, `Cooldown`, and `SlashRadius` on `/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiIaijutsuStance` under Runtime Balance. `Tools/add_iaijutsu.py` adds the card and pool reference while preserving all existing card tuning, with backups under `Saved/Backups/Iaijutsu`; `-ValidateIaijutsu` is read-only.

In **BP_Samurai > AutoAttackComponent > Samurai > Iaijutsu**, assign **Iaijutsu Hit VFX** (Niagara) for the instant damage moment. It spawns once per slash at the lane center, oriented along the attack; optional parameters are `User.StartPosition` and `User.EndPosition` (vectors), `User.Length` and `User.Width` (floats, cm). The slot is empty by default. **Iaijutsu Indicator Color** defaults to cyan; **Iaijutsu Indicator Material** optionally replaces the enemy-style rectangle material. `Tools/update_iaijutsu_charge.py` applies the 1-second charge, 100 cm base half-width, and +25% width per rank to the saved cards with backups, preserving other tuning; `-ValidateIaijutsuCharge` is read-only.

In **BP_Samurai > AutoAttackComponent > Samurai > Iaijutsu > Path Slashes**, **Iaijutsu Path Slash VFX** selects the release bursts (`NS_Slash_Iaijutsu` from the SlashesV1 folder). Five evenly spaced bursts progress along each final lane starting when damage resolves, including Double Cut, Dash Draw, Death Cascade and instant casts. The first appears immediately; the remaining bursts follow at **0.05-second intervals** (0.2 seconds from first to last). Each burst independently varies its local rotation by up to **?20? pitch, ?35? yaw and ?60? roll**. All gameplay damage still resolves immediately at charge completion. Nothing spawns during the charge. The bursts stay at the resolved world positions and finish independently of the attack actor. **Iaijutsu Path Slash Count**, **Scale**, and **Rotation** adjust density, size and base orientation; size also follows lane width. **Iaijutsu Path Slash Delay** controls spacing in time (zero makes them simultaneous); **Iaijutsu Path Slash Rotation Randomness** sets the maximum random angle on each axis (zero disables that axis). Short one-shot timers retain the final path after the attack actor is destroyed, and cosmetic randomness uses a separate stream from combat rolls. Clear the effect or set Count to zero to disable them. These are cosmetic and leave the separate center Hit VFX intact. `Tools/configure_iaijutsu_path_slashes.py` assigns the effect with a Blueprint backup under `Saved/Backups/IaijutsuPathSlashes`; `-ValidateIaijutsuPathSlashes` checks without writing. `HeavensDivide.Combat.IaijutsuPathVFX` checks release-only timing, delayed burst order, fixed final-lane placement, independent rotation bounds, zero-delay/zero-randomness behavior, particle emission and natural cleanup with rendering enabled.

During charging, a gentle vacuum draws valid nearby enemies into the lane: **60 cm** beyond its edges (up to **105 cm** with Vacuum Reach), at **240 cm/s**, checked every 0.05 seconds within the existing slash presentation tick. The pull compensates for enemy movement away from the lane, so ordinary pursuit cannot overpower it; movement along the lane remains free. It uses the enemy movement system's sweep, respecting walls while allowing grounded capsules to move along the floor. Instant casts perform the pull at resolution. Enemies outside the small vacuum margin remain unaffected.

Lane hits apply a separate **3-second Iaijutsu mark**, making enemies take **50% more damage** from subsequent damage, including Ninja attacks. Reapplying refreshes its duration without stacking copies. The applying hit does not benefit from its newly applied mark. Marked Blade remains a separate consumable effect; both use the existing mark indicator. Iaijutsu marks do not apply Bleed.

| One-time upgrade | Effect |
| --- | --- |
| Death Cascade | An Iaijutsu attack that kills at least one enemy launches one follow-up from a killed enemy toward the nearest valid enemy within 800 cm. Cascaded attacks cannot trigger another cascade, including kills from their extra lanes or Endpoint Burst. An X attack triggers at most one follow-up. |
| Double Cut | Every fourth Iaijutsu attack becomes two full-damage lanes, rotated +/-30 degrees around their common midpoint to form an X. Enemies in their intersection can take both hits. |
| Flash Draw | 15% chance to resolve immediately without charging; normal attacks still have their cooldown. |
| Endpoint Burst | 15% chance to add a full-damage circular hit at one lane endpoint, with a 250 cm radius. Can hit an enemy also hit by the lane and applies the Iaijutsu mark. |
| Ninja Assist | Every second while Samurai autoattacks are enabled, a 5% chance to request Ninja's equipped assist. Uses a timer, with no component tick; a busy assist cannot overlap itself. Pauses with gameplay and stops when autoattacks stop. |
| Dash Draw | +100% Samurai dash distance while Iaijutsu is equipped, with the same dash duration and charge cost. After a completed dash, charge a slash from its old position to its actual endpoint, including collision-shortened dashes. Does not consume or reset normal attack cooldown. Ninja dash distance is unchanged. |

Normal, dash and kill-triggered attacks advance the Iaijutsu Double Cut counter and can roll instant cast and Endpoint Burst. Extra lanes do not separately advance the counter. Only normal attacks consume Grand Entrance or advance normal attack/Tag Team events. Kill follow-ups stop when Samurai is no longer active or autoattacks cannot run.

| Normal scalable upgrade | Per rank | Maximum ranks |
| --- | --- | --- |
| Iaijutsu Damage | +20% Iaijutsu damage | 5 |
| Iaijutsu Charge Speed | +15% charge speed; duration = base duration / (1 + total bonus) | 5 |
| Iaijutsu Width | +25% lane width | 5 |
| Mark Damage | +10 percentage points vulnerability | 5 |
| Vacuum Reach | +15% vacuum reach; 60 cm base reaches 105 cm, without widening the damage lane | 5 |

These are initial balance defaults. Each card exposes `PerRank` under Runtime Balance.

| Rare scalable upgrade | Per rank | Maximum ranks |
| --- | --- | --- |
| Double Cut Frequency | One fewer attack; 3/2/1 | 3 |
| Endpoint Burst Chance | +10 percentage points | 5 |
| Instant Cast Chance | +10 percentage points | 5 |
| Assist Chance | +5 percentage points per one-second roll | 5 |
| Cascade Power | +20% Death Cascade attack damage, including its crossing lanes and Endpoint Burst | 5 |
| Dash Draw Power | +20% Dash Draw attack damage, including its crossing lanes and Endpoint Burst | 5 |

Rare upgrades require their respective one-time unlocks. Cascade Power requires Death Cascade; Dash Draw Power requires Dash Draw. Their bonuses are separate: dash damage does not carry into a cascade, and cascades still cannot chain. All 20 upgrades require Iaijutsu; Blood Double Cut and its counter remain separate.

**Blood Shrine rewards for Iaijutsu Stance only**, excluded from ordinary and unrestricted offers:

- **Focused Malice:** +100% mark damage bonus (doubles vulnerability after Mark Damage ranks; base +50% becomes +100%), -20% lane width. Mutually exclusive with Relentless Steps.
- **Patient Blade:** +50% Iaijutsu damage, +30% charge duration (slower). Both multiply after scalable bonuses.
- **Relentless Steps:** marks no longer increase damage taken. A marked enemy's death from any source reduces the current dash recharge by 0.3 seconds, including lethal applying hits. Finishing that recharge grants one charge; excess reduction does not carry into another charge. Existing Mark Damage ranks convert one-for-one into Iaijutsu Damage, and Mark Damage stops appearing in offers. Mutually exclusive with Focused Malice.

Tradeoff conversions preserve existing damage strength, mastery and the damage card's five-rank ceiling; if merged investments exceed that ceiling, all already purchased bonus strength is retained. Older saves containing both Iaijutsu pacts retain Relentless Steps and convert the ineffective Focused Malice purchase into one damage rank, removing its width penalty. Repeated saves/restores do not repeat the conversion.

In **BP_Samurai > AutoAttackComponent > Samurai > Iaijutsu**, assign **Iaijutsu Endpoint Montage** for the spectral Samurai at the endpoint when Endpoint Burst procs. This is presentation only, with gameplay animation notifies suppressed; damage works even when the slot is empty. The slot is empty by default.

`Tools/overhaul_iaijutsu.py` authors the 20 upgrades and pool entries, preserving existing artwork. Existing card tuning is preserved, backups go under `Saved/Backups/IaijutsuBuild`, and `-ValidateIaijutsuBuild` checks the 156-card catalog without writing. `-UpdateIaijutsuCascade` updates only Death Cascade's saved description for the one-follow-up rule. Stance Runtime Balance exposes `VacuumReach`, `VacuumSpeed`, `MarkBonus`, and `MarkDuration` alongside the existing lane/timing values. Proc cards expose `Chance`, and Endpoint Burst also exposes `Radius`.

Dash Draw exposes `DashDistanceBonus` (1.0 = +100%) on `DA_Upgrade_SamuraiIaijutsuDash`. Its distance is committed when the dash begins and does not modify the controller's base dash distance. `Tools/update_iaijutsu_dash.py` updates only this card with a backup; `-ValidateIaijutsuDash` validates without writing.

Implementation: `SamuraiIaijutsu.cpp`, `SamuraiAutoAttack.cpp`, `IaijutsuBuild.h`, `EnemyBase.cpp`, and dash hooks in `SurvivorPlayerController.cpp`. Regression coverage: `HeavensDivide.Combat.Iaijutsu`, `SamuraiBuilds`, and `TrialBuildRewards`.

### Disabled overkill upgrades (retained for future reuse)

**Overkill Burst (`OverkillBurst`)** is temporarily disabled, along with **Expanding Ruin**. Their retained behavior is described below. Direct normal melee and Blade Wave kills explode for their excess damage within **220 cm**, scaling with Samurai area. A hit dealing 150 damage to a 40-HP enemy creates a 110-damage explosion. Exact kills with no overkill create no explosion.

Assists, Bleed, explosion kills and other proc damage cannot start an explosion. Each directly killed melee target can produce its own burst. Explosion damage respects enemy source restrictions and does not apply Bleed.

- **Expanding Ruin (`BurstRadius`)**: +12/18/25% explosion radius per Common/Rare/Epic rank, up to 5 ranks. Requires Overkill Burst.
- Continue scaling with **Attack Damage** and Samurai area. Blood Stance's Double Cut and echoes can also create direct-hit overkill bursts.

### Crescent Stance: waves and slow

**Crescent Stance** replaces normal melee attacks with traveling blade waves, fired directly by the existing attack timer. The character alternates `AM_SamuraiBladeWave` and `AM_SamuraiBladeWave2`, creates no normal melee hitbox, and remains free to move. Auto-targeting selects the nearest valid enemy within the upgraded wave travel range; cursor aiming keeps its chosen direction. The normal attack interval is unchanged. Wave hits apply **30% slow for 5 seconds**. Reapplication refreshes the duration; copies do not stack. Split waves also apply the slow. Movement uses the current slow multiplier without adding an enemy tick or changing its underlying movement speed.

In **BP_Samurai > AutoAttackComponent > Samurai > Blade Wave**, **Crescent Montage** and **Crescent Alternate Montage** select the alternating animations. Playback uses the existing attack-interval animation scaling. The visual faces the committed wave direction without a facing lock; root motion is suppressed. These montages are presentation only: waves fire at attack start, and animation notifies cannot add melee damage or duplicate waves. Dash, swap and combo transitions can interrupt the animation. Leaving either slot empty uses the other; leaving both empty still fires waves. Assists retain the normal Samurai assist montage.

All three stance montages are in `/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai`. `Tools/configure_stance_montages.py` assigns them on the saved Samurai Blueprint, preserving Blood/assist and Endpoint Burst montage settings, with backups under `Saved/Backups/StanceMontages`; `-ValidateStanceMontages` is read-only.

Shared Samurai damage, attack-speed and area investments convert into Wave Damage, Wave Speed and Wave Range when Crescent is selected or a saved run is restored. Their ranks and bonuses carry over. Future offers use these Crescent forms in normal offers, unrestricted rewards and repeat trials. Global stat bonuses still apply. Returning Blade is enabled with the behavior below; the other legacy wave branches and scaling cards remain disabled.

| One-time upgrade | Effect |
| --- | --- |
| Double Cut | Every fourth committed attack fires four full-damage waves in a **+ pattern**, oriented forward/right/back/left. Each retains the wave upgrades. |
| Splitting Waves | **15%** chance on each wave's first successful hit to launch two smaller waves left and right from the struck enemy, at +/-90 degrees to the incoming direction. The original wave continues along its path. Each branch deals **50% damage**, with **60% width and range**. Children cannot split again or immediately hit their triggering enemy. |
| Lingering Wake | Each primary wave has a **15%** chance to leave one continuous stationary strip along its **whole outbound path, from launch to its actual endpoint**, using its scaled width and height and including the wave hitbox at both ends. It appears when outbound travel ends; early hits do not shorten it. Split waves inherit their parent's success or failure without rerolling; successful children leave their own narrower full-path strips. Base duration **3 seconds** and damage **30% of its wave damage basis per second**. |
| Ninja Assist | Kills while Samurai is active in Crescent have a **5%** chance to request Ninja's equipped assist. Includes melee, wave, field and other kills during that stance. Busy assists cannot overlap. Uses the death event, with no polling or tick. |
| Returning Blade | Primary waves return to their committed launch point for a second pass at **50% damage**. Each enemy can be hit once per pass. Split children do not return. |
| Arc Volley | Attacks have a **15%** chance to fire three waves in a **40-degree arc**. On Double Cut, each of the four directions becomes a fan, producing twelve waves. |

Each wave has only one split roll and one field opportunity across both passes. A successful field proc commits its entire outbound strip before returning; its first return hit can still use its unused split roll. Split children inherit the parent's original field result, even when first created during the return after its field has spawned. Returning Blade applies to Double Cut and Arc Volley primary waves. There is no Crescent cascade upgrade. Double Cut advances once per committed normal attack; assists and child waves do not advance its counter. Split and arc waves preserve the committed attack's damage, speed and range scaling. Ground-motion slowdown stretches with range and speed so a slower wave does not lose its configured reach.

| Normal scalable upgrade | Per rank | Maximum ranks |
| --- | --- | --- |
| Wave Damage | +20% wave damage; also increases the field damage basis | 5 |
| Wave Speed | +20% travel speed; does not change attack frequency | 5 |
| Wave Range | +20% travel range | 5 |
| Increased Slow | +5 percentage points; 30% base reaches 55% | 5 |
| Slow Duration | +1 second; 5-second base reaches 10 seconds | 5 |

| Rare scalable upgrade | Per rank | Maximum ranks |
| --- | --- | --- |
| Double Cut Frequency | One fewer attack; 3/2/1 | 3 |
| Field Duration & Damage | +15% duration and total damage; damage per second unchanged | 5 |
| Split Chance | +15 percentage points; maximum 90% total | 5 |
| Assist Chance | +5 percentage points; maximum 30% total | 5 |
| Arc Volley Chance | +10 percentage points; maximum 65% total | 5 |
| Wake Chance | +5 percentage points per primary wave; maximum 40% total; split waves inherit the result | 5 |

Rare cards require their corresponding one-time unlocks. Field damage uses timers, including a final partial tick when needed. Fields retain Samurai damage-source restrictions and do not apply slow, split or create more fields. Fields and waves already spawned persist through swaps.

Fields sweep the wave's rectangular hitbox from its ground-adjusted launch position to its actual outbound endpoint. The resulting strip is centered halfway along that path, aligned with travel, and retains the wave's scaled width and end-cap thickness. It uses actual distance travelled rather than configured range, so shortened travel cannot damage ground ahead. The strip stays fixed after the wave disappears or returns. Its full duration starts when outbound travel ends; Sudden Eruption waits 0.3 seconds from that point. Normal pulses and eruptions cover the same full strip; neither spawns a separate ground indicator. One field damage budget applies per target, regardless of strip length; returns create no duplicate field.

The wave's crescent and glowing ground line always remain visible. Its three ground-debris layers appear only when that wave successfully procs **Lingering Wake**; owning the upgrade alone does not enable debris. Split waves inherit their parent's field result and debris state without rerolling. Returning waves retain the original proc result, including after their field has spawned, without rerolling. The field has no blue decal, rectangle or eruption flash. Its only persistent ground visual is the Niagara debris deposited along the wave path. The two stationary debris layers remain visible for the whole field lifetime (3 seconds base, 5.25 seconds with all duration ranks) and disappear when the field ends; Sudden Eruption clears them when it erupts after 0.3 seconds. Returning waves reuse the deposited ground trail rather than laying a second one. Air debris keeps its authored animation. See [Blade Wave ground slash](BladeWaveGroundSlash.md) for presentation tuning.

**Blood Shrine rewards for Crescent only**, excluded from ordinary and unrestricted rewards:

- **Scorched Wake:** +50% ground field damage, -30% direct wave damage. Requires Lingering Wake.
- **Crushing Tide:** +50% direct wave damage, -50% wave travel speed.
- **Sudden Eruption:** Crescent hits no longer apply slow. Fields wait **0.3 seconds**, then erupt for all damage they would have dealt over their full duration. Requires Lingering Wake. Existing Increased Slow and Slow Duration ranks convert one-for-one into Wave Damage, and both slow upgrades stop appearing in offers. Conversion preserves invested strength and mastery, including in older saves.

Field Duration & Damage increases the total damage budget only once: five ranks give **5.25 seconds and 175% total damage**, with unchanged DPS. Sudden Eruption uses the same total. Scorched Wake multiplies the result by 1.5, giving 262.5% of the unupgraded field's total damage at maximum field ranks.

Tradeoffs combine multiplicatively. The direct-wave damage tradeoffs do not change the damage basis of the field, so Scorched Wake grants its full field benefit. Existing slows expire normally after acquiring Sudden Eruption.

`Tools/overhaul_crescent.py` authors the 20 upgrades and controller pool references, preserving existing artwork and card tuning. Backups are under `Saved/Backups/CrescentBuild`; `-ValidateCrescent` checks the 156-card pool without writing. Edit `SlowFraction` and `SlowDuration` on the Crescent stance asset; the new cards expose `Chance`, `PerRank`, split ratios, field `Duration`, and `DamagePerSecond` under Runtime Balance. Field dimensions come from the wave; the obsolete field `Radius` setting is removed. Lingering Wake's **Runtime VFX** accepts the existing presentation settings for a custom field effect. Optional Niagara parameters are `User.HalfExtents` (vector) and `User.Length`, `User.Width`, `User.Height` (floats, cm); effects are oriented to the wave.

`Tools/update_crescent_field_shape.py` updates Lingering Wake's completed-path and split-inheritance description, Wake Chance's description, and removes only the obsolete field radius, with backups under `Saved/Backups/CrescentFieldShape`; `-ValidateCrescentFieldShape` is read-only. `CrescentBuilds` checks full-path coverage at the launch point, middle and endpoint, shortened travel, inherited split strips, return deduplication, rotated/scaled/elevated boxes, fixed placement, and identical pulse/eruption coverage. Damage and duration tuning are preserved.

`Tools/update_crescent_waves.py` migrates the saved stance to 30% slow and updates its descriptions, preserving other tuning with backups under `Saved/Backups/CrescentWaves`; `-ValidateCrescentWaves` is read-only. Grand Entrance keeps its separate circular proc on wave launch. Samurai Tag Team retains its existing assist slash.

Implementation: `CrescentBuild.cpp`, `AutoAttackComponent.cpp`, `SamuraiAutoAttack.cpp`, `SamuraiBladeWave.cpp`, `SamuraiWaveField.cpp`, `SamuraiBuildUpgrades.cpp`, `EnemyBase.cpp`, and `EnemyLightweightMovementComponent.cpp`. Regression coverage: `HeavensDivide.Combat.CrescentBuilds`, `SamuraiBuilds`, `TrialBuildRewards`, and `GroundSlashMotion`.

## Legacy Blade Wave supports (`BladeWave`)

With Crescent Stance, Samurai automatically launches traveling blade waves directly from the attack timer, accompanied by alternating stance montages. Emission needs no animation notify and deals no normal melee damage. The original seven family cards and Wave Volley remain saved for compatibility. The stance and Returning Blade are active; the other support cards listed below are disabled and replaced by the Crescent upgrades above:

- **Crescent Stance:** exclusive trial stance that unlocks the attack-triggered wave.
- **Returning Blade (enabled):** primary waves return to their launch point at 50% damage, with the proc limits described above.
- **Crossing Blades:** every third swing launches three crossing waves, plus acquired Wave Volley waves. This proc staggers its waves by 0.12 seconds so they are visibly separate. Tune **BP_Samurai > AutoAttackComponent > Samurai > Blade Wave > Crossing Blade Wave Delay**; zero restores simultaneous spawning. Pending waves retain the committed swing's origin, aim and damage, and are cancelled when autoattacks stop (such as swapping or starting the combo ability). Ordinary Wave Volley swings keep their simultaneous fan.
- **Splinter Wave:** the first enemy hit on each wave pass sheds a small 30%-damage burst that applies no Bleed.
- **Force (`BladeWavePower`):** increase wave damage.
- **Wide Arc (`WideArc`):** increase wave width and damage.
- **Velocity (`BladeWaveHaste`):** increase wave travel speed; does not change basic attack frequency.

The following tuning is retained for future reuse. Disabled branches combine and require Blade Wave when re-enabled. The three scaling cards have five ranks. Force grants 20/30/45%, Wide Arc 12/18/25%, and Velocity 10/15/22% at Common/Rare/Epic rarity; magnitudes add across ranks.

## Ninja weapon routes

Completing a Ninja trial offers three Rare weapon-route cards that replace or modify Ninja's normal attack: Returning Fang, Barrage, and Giant Shuriken. These starters are excluded from normal character offers and unrestricted upgrade rewards. Choose one per run; each stance has its own 6/5/6/3 upgrade tree. Earlier shared Ninja supports are excluded after choosing a stance, with paid ranks converted into that stance's damage investment.

| Stance | Attack behavior | Scaling |
| --- | --- | --- |
| Returning Fang (`ReturningFang`) | One homing blade hits, returns, and immediately relaunches. Close targets shorten the cycle. | Attack speed becomes travel speed. Each extra projectile adds 25% damage instead. |
| Barrage (`BarrageStance`) | Poison volleys with +30% attack speed and +2 projectiles. | Attack speed, projectile count and damage. |
| Giant Shuriken (`GreatShuriken`) | A large spinning blade pierces crowds and can hit each enemy every 0.25 seconds before Cutting Tempo. +50% damage and -50% throw speed. Travel speed is independently tunable. | Damage, throw speed, size, lifetime, and contact frequency; each extra projectile adds 12% blade size instead. |

### Returning Fang

Returning Fang uses **6 one-time mechanics, 5 normal scalable upgrades, 6 rare scalable branches, and 3 Shrine-only tradeoffs**. Outward hits damage the selected enemy; returning blades deal no contact damage. Staying close shortens each attack cycle. Additional projectile bonuses still become 25% Fang damage each.

Before choosing a Ninja stance, **Attack Damage**, **Attack Speed**, and **Coverage** are available. Choosing Fang converts their ranks, rolled strength, and banishment into **Fang Damage**, **Flight Speed**, and **Attack Range**. Other Ninja routes retain attack-speed scaling; Coverage scales normal kunai and Great Shuriken size.

#### One-time upgrades

| Upgrade | Effect | Ranks |
| --- | --- | --- |
| Twin Fang (`FangTwin`) | Every fourth launch sends 2 additional spectral Fangs, seeking different enemies when possible. Spectral Fangs return once and cannot summon more Fangs. | 1 |
| Relentless Fang (`RelentlessFang`) | Consecutive hits from the same Fang against an enemy gain 15% damage per hit, up to 75%. Hitting another enemy resets the bonus. | 1 |
| Resonant Fang (`FangResonance`) | After striking an enemy, each returning Fang releases a burst around you for 50% Fang damage. Bursts cannot trigger Fang effects. | 1 |
| Deadeye (`FangDeadeye`) | Fang hits have a 15% chance to deal double damage. | 1 |
| Samurai Assist (`FangAssist`) | Completed main-Fang round trips have a 5% chance to call Samurai for an assist attack. Can trigger once per second. | 1 |
| Splintering Fang (`FangSplinter`) | Fang kills scatter one kunai for each direct Fang hit the enemy took, including the killing hit. Each kunai deals 40% Fang damage and cannot trigger Fang effects. | 1 |

#### Normal scalable upgrades

| Upgrade | Effect | Ranks |
| --- | --- | --- |
| Fang Damage (`FangDamage`) | Fangs deal 20% more damage. | 5 |
| Flight Speed (`FangSpeed`) | Fangs travel 15% faster on outward and return trips. | 5 |
| Attack Range (`FangRange`) | Fangs seek enemies 15% farther away. | 5 |
| Close Quarters (`FangCloseQuarters`) | Fang hits deal 10% more damage to enemies within 3 metres of you. | 5 |
| Killing Edge (`FangKillingEdge`) | Fang hits deal 15% more damage to enemies below 30% health. | 5 |

#### Rare scalable upgrades

| Upgrade | Effect | Ranks |
| --- | --- | --- |
| Twin Fang Frequency (`FangTwinFrequency`) | Twin Fang requires 1 fewer launch, down to every launch. | 3 |
| Relentless Pressure (`FangPressure`) | Increases Relentless Fang's damage per stack and maximum bonus by 20% of their base values. | 5 |
| Resonant Reach (`FangResonantReach`) | Resonant Fang bursts have 15% more radius. | 5 |
| Deadeye Mastery (`FangCriticalChance`) | Adds 10% critical-hit chance to Fang hits. | 5 |
| Reinforcements (`FangAssistChance`) | Adds 5% chance to call Samurai on a completed main-Fang round trip. | 5 |
| Splinter Power (`FangSplinterPower`) | Splintering Fang kunai deal 20% more damage. | 5 |

#### Shrine tradeoffs

| Upgrade | Effect | Ranks |
| --- | --- | --- |
| Farstrider (`FangFarstrider`) | Fangs travel 25% slower. Outward hits gain damage with distance travelled, up to 150% bonus damage at 10 metres. | 1 |
| Redline (`FangRedline`) | Fangs travel 60% faster, but deal 30% less damage. | 1 |
| Predator's Price (`FangPredator`) | Fang critical hits deal 50% more damage, but noncritical hits deal 25% less damage. | 1 |

Normal upgrades roll Common/Rare/Epic magnitudes. Damage grants 20/30/40%; Flight Speed and Attack Range grant 15/22.5/30%; Close Quarters grants 10/15/20%; Killing Edge grants 15/22.5/30% per rank. Rare upgrades require their matching one-time unlock. Shrine rewards require Returning Fang; Predator's Price also requires Deadeye. All three tradeoffs can combine.

Twin Fang's two spectral blades prefer different targets, then fall back to the main target. Each ends after one return, cannot summon further Fangs, and does not roll assists. Relentless Fang maintains a separate consecutive-target streak on each blade. Successful returns trigger Resonant Fang around the active Ninja; interrupted trips without a hit, swap recalls, clone returns and assist returns do not grant player return effects.

Splintering Fang counts direct Fang hits across blades, including the killing hit. Only a direct Fang kill scatters kunai. Bursts, poison and scattered kunai cannot trigger it. Scattered kunai use the existing straight-flying Ninja projectile presentation and cannot embed or recursively scatter. Large kill counts are emitted in bounded batches rather than discarded. Farstrider measures the current outward trip only, resets each launch and caps its distance bonus.

Cutting Return and Final Pursuit are retired, including stale-reference acquisition. Old saved ranks migrate to Resonant Fang and Splintering Fang. Earlier shared Ninja supports are excluded from Fang; their purchased ranks convert into Fang Damage, preserving their investment and mastery. Barrage and Giant Shuriken have their own trees and corresponding investment conversion.

Tune the cards under `/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaFang…`; the stance remains `DA_Upgrade_NinjaReturningFangStance`. `Tools/fang_build_upgrades.json` seeds the catalog. `Tools/overhaul_fang.py` authors it with backups under `Saved/Backups/FangBuild`; `-ValidateFang` is read-only. In BP_Ninja's NinjaBuildComponent, **Fang Return Burst VFX** overrides burst presentation and **Spectral Fang Material** overrides the default swap-ghost material. `NinjaBuildPreview Fang` grants the ordinary tree at one rank without Shrine tradeoffs. Regression coverage: `HeavensDivide.Combat.FangBuilds`, `NinjaBuilds`, `TrialBuildRewards`, and `SamuraiTreeStructure`. Starting balance requires playtesting.

The Fang stance, its 20 upgrades and the three convertible Ninja basics have matching hand-drawn icons. Source images are in `Art/UpgradeIcons/Generated/Fang`; exact generation prompts are in `Art/UpgradeIcons/fang_generation.json`. `Tools/import_fang_icons.py` imports and assigns card artwork without changing gameplay tuning; `-ValidateFangIcons` checks assignments without writing. Open `Art/UpgradeIcons/index.html` for the icon gallery.

### Barrage: poison volleys

Barrage grants **30% attack speed and 2 extra projectiles**, with no stance damage penalty. Direct kunai hits apply poison before damage resolves, so lethal hits also qualify for poisoned-kill effects. Each stack deals **5% of its applying hit's damage over 5 seconds**, in ten 0.5-second ticks. Critical damage contributes to the stack. Applications refresh all stacks, including at the cap, without adding damage when full. The base cap is 20; Deep Venom raises it to 40. Duration adds ticks at the same damage rate.

The tree has **6 one-time, 5 normal scalable, 6 rare scalable and 3 Shrine upgrades**. Basic Ninja Damage, Speed and Coverage convert to Kunai Damage, Attack Speed and Kunai Reach, preserving ranks, rarity strength and banishment. Coverage becomes targeting and projectile travel range. Travel distance is enforced by projectile flight lifetime at its launch speed, including ricochets; Point-Blank Barrage also shortens targeting range.

#### One-time upgrades

| Upgrade | Effect | Ranks | Requires |
| --- | --- | --- | --- |
| **Needle Rain** | Every fourth attack fires twice as many kunai. | 1 | Barrage Stance |
| **Forked Blades** | Kunai have a 15% chance to split on impact into 2 blades dealing 50% damage each. Split blades apply poison but cannot split again. | 1 | Barrage Stance |
| **Toxic Ground** | Killing an enemy poisoned by your kunai leaves a puddle for 3 seconds. Every 0.5 seconds it applies 1 poison stack, matching the victim's average stack strength. Puddle poison alone cannot create another puddle. | 1 | Barrage Stance |
| **Samurai Assist** | Each attack has a 5% chance to call a Samurai assist. An active assist cannot overlap itself. | 1 | Barrage Stance |
| **Critical Strike** | Kunai hits have a 15% chance to deal double damage. Poison inherits the increased hit damage. | 1 | Barrage Stance |
| **Viper's Rush** | Killing a poisoned enemy grants 20% movement speed for 3 seconds. Further kills refresh the duration. | 1 | Barrage Stance |

#### Normal scalable upgrades

| Upgrade | Effect | Ranks | Requires |
| --- | --- | --- | --- |
| **Kunai Damage** | Kunai deal 20% more damage. | 5 | Barrage Stance |
| **Attack Speed** | Attack 15% faster. | 5 | Barrage Stance |
| **Kunai Reach** | Kunai travel 15% farther. | 5 | Barrage Stance |
| **Lingering Venom** | Poison lasts 2 seconds longer, dealing damage at the same rate. | 3 | Barrage Stance |
| **Venom Load** | Each kunai hit applies 2 additional poison stacks. | 3 | Barrage Stance |

#### Rare scalable upgrades

| Upgrade | Effect | Ranks | Requires |
| --- | --- | --- | --- |
| **Relentless Rain** | Needle Rain requires 1 fewer attack. At maximum rank, every attack fires twice as many kunai. | 3 | Needle Rain |
| **Branching Blades** | Kunai gain 10% split chance. With Serpent's Procession, each ricochet instead loses 2% less damage. | 5 | Forked Blades |
| **Reinforcements** | Gain 5% chance to call a Samurai assist. | 5 | Samurai Assist |
| **Spreading Blight** | Poison puddles have 20% more radius. | 5 | Toxic Ground |
| **Critical Precision** | Kunai gain 10% critical strike chance. | 5 | Critical Strike |
| **Deep Venom** | Enemies can hold 4 additional poison stacks. | 5 | Barrage Stance |

#### Blood Shrine tradeoffs

| Upgrade | Effect | Ranks | Requires |
| --- | --- | --- | --- |
| **Point-Blank Barrage** | Gain 50% attack speed, but kunai travel 70% less distance. | 1 | Barrage Stance |
| **Serpent's Procession** | Fire kunai one after another in a straight line. They ricochet to 3 additional nearby enemies, losing 25% damage per bounce, but cannot split. Each 10% split chance reduces this loss by 2%. | 1 | Barrage Stance |
| **Venom Bloom** | Puddles no longer apply poison. Every 0.5 seconds they instead deal 10% of each enemy's remaining poison damage without consuming its stacks. | 1 | Toxic Ground |

Deep Venom requires only the stance. Viper's Rush is independent; repeated kills refresh its single movement bonus and swapping away clears it. Assist chance rolls once per committed player volley, including Needle Rain; assists and clones cannot roll more assists.

Forked kunai deal half their parent's base damage and can independently crit and poison, but cannot split again. Serpent's Procession retains committed origin, direction, damage and assist identity; pending throws are canceled when autoattacks stop. Three ricochets can hit three additional distinct valid enemies. Split chance improves ricochet damage retention from 75% to 78% with Forked Blades, then to 88% at five Branching Blades ranks.

Toxic Ground pulses six times, applying one stack per pulse regardless of Venom Load. Puddle poison uses the dead enemy's average per-stack damage rate and cannot alone seed another puddle. A direct kunai application makes that victim eligible again. Venom Bloom replaces poison application with damage equal to 10% of the target's current remaining poison damage per pulse; it does not consume stacks or create poison on fresh enemies. Puddles use timer-driven ground discs, green for poison and violet for Bloom; no actor tick is required.

Focused Volley and Crescendo are retired. Earlier shared poison, fragment and clone cards are unavailable for Barrage; paid ranks from these cards and the two retired branches convert into Barrage Damage on restoration, preserving mastery. Giant Shuriken likewise replaces the old shared supports with its own tree.

`Tools/barrage_build_upgrades.json` seeds the tree. `Tools/overhaul_barrage.py` authors only its cards and saved pool, with backups under `Saved/Backups/BarrageBuild`; `-ValidateBarrage` is read-only. The stance asset is `/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaBarrageStance`. New tuning is under Runtime Balance on the relevant card. `NinjaBuildPreview Barrage` grants the ordinary tree at one rank without Shrine tradeoffs.

The 21 matching icons are under `Art/UpgradeIcons/Generated/Barrage`, with exact prompts in `Art/UpgradeIcons/barrage_generation.json`. `Tools/import_barrage_icons.py` assigns them; `-ValidateBarrageIcons` is read-only. The gallery is `Art/UpgradeIcons/index.html`.

Regression coverage: `HeavensDivide.Combat.BarrageBuilds`, `NinjaBuilds`, `FangBuilds`, `TrialBuildRewards` and `TagTeamRegression`. Starting balance needs playtesting.

### Giant Shuriken

The stance has **6 one-time upgrades, 5 normal scalable upgrades, 6 rare scalable upgrades, and 3 Blood Shrine tradeoffs**. Basic Ninja Damage, Speed, and Coverage convert into Shuriken Damage, Attack Speed, and Shuriken Size, preserving accumulated rarity strength and mastery. Attack Speed controls throws; Cutting Tempo independently controls repeated contact hits and periodic bursts.

**One-time upgrades**

| Upgrade | Effect | Ranks / prerequisite |
| --- | --- | --- |
| Grinding Halt | Each blade slows to 15% speed for 1 second on its first elite or boss hit. | 1 |
| Growing Shuriken | Blades grow throughout their lifetime, reaching twice their starting size. | 1 |
| Breaking Wheel | When a blade expires, it releases a circular slash dealing 75% blade damage. Larger blades create larger slashes. | 1 |
| Twin Wheel | Every fourth throw launches an additional shuriken. | 1 |
| Samurai Assist | Shuriken contact and burst kills have a 5% chance to call a Samurai assist. | 1 |
| Serrated Edge | Each blade gains 15% damage against an enemy for every previous contact hit, up to 75%. Bursts benefit from this bonus without increasing it. | 1 |

**Normal scalable upgrades**

| Upgrade | Effect | Ranks / prerequisite |
| --- | --- | --- |
| Shuriken Damage | Increase blade and burst damage. | 5 |
| Attack Speed | Throw shurikens more frequently. | 5 |
| Shuriken Size | Increase blade size and burst radius. With Blood Hunger, increase size gained per kill instead. | 5 |
| Lingering Shuriken | Blades last 30% longer per rank. | 3 |
| Cutting Tempo | Increase contact hit and pulse frequency by 15% per rank. | 5 |

**Rare scalable upgrades**

| Upgrade | Effect | Ranks / prerequisite |
| --- | --- | --- |
| Persistent Grind | Grinding Halt lasts 0.2 seconds longer per rank. | 5 / Grinding Halt |
| Unstoppable Growth | Increase maximum blade growth by 20 percentage points per rank. Also raises Blood Hunger's growth cap. | 5 / Growing Shuriken |
| Shattering Force | Increase expiry slash and periodic burst damage by 15% per rank. | 5 / Breaking Wheel |
| Twin Frequency | Twin Wheel triggers one throw sooner per rank: every third, second, then every throw. | 3 / Twin Wheel |
| Reinforcements | Increase Samurai Assist chance by 5 percentage points per rank. | 5 / Samurai Assist |
| Deep Grooves | Serrated Edge gains 3 percentage points more damage per hit and a 15 percentage point higher cap per rank. | 5 / Serrated Edge |

**Blood Shrine tradeoffs**

| Upgrade | Effect | Ranks / prerequisite |
| --- | --- | --- |
| Bound Orbit | Blades orbit around you instead of flying forward, but deal 30% less contact damage. Expiry slashes are retained. | 1 |
| Blood Hunger | Blades start 30% smaller and grow 10% per kill, up to their growth cap. Size upgrades increase growth per kill instead of starting size. Growth resets for each blade. | 1 |
| Pulsing Core | Blades release a circular burst every second, but deal 35% less contact damage. Cutting Tempo speeds up bursts. Expiry slashes are retained. | 1 / Breaking Wheel |

All three Shrine tradeoffs can combine. Bound Orbit retains expiry bursts and Grinding Halt slows orbital movement. Normal orbiting blades follow the active Ninja; after swapping, they complete their lifetime around the last center. Assist blades orbit their arrival point; legacy clone blades orbit their clone. Assists and clones do not advance the player's Twin Wheel counter or summon further assists.

Growing Shuriken uses elapsed lifetime, including while stationary or orbiting. Blood Hunger adds kill growth to time growth under a shared cap (initially 2× starting size); Unstoppable Growth increases this cap. Size ranks become growth-per-kill multipliers with Blood Hunger. Each blade starts fresh. Serrated Edge tracks each enemy independently for each blade. Only successful contact hits build stacks; bursts benefit without adding stacks. Contact and burst kills count toward Blood Hunger and Samurai Assist; kills never recursively create bursts.

Contact intervals have a 0.05-second minimum; pulse intervals have a 0.2-second minimum. Shattering Force scales burst damage only. Pulsing Core keeps the expiry burst. Heavy Shuriken is retired; saved ranks migrate into Cutting Tempo. Older shared Ninja supports are excluded after selecting the stance, with saved paid ranks credited to Shuriken Damage without losing mastery.

`Tools/shuriken_build_upgrades.json` defines the tree. `Tools/overhaul_shuriken.py` authors it with backups under `Saved/Backups/ShurikenBuild`; `-ValidateShuriken` checks without saving. Existing stance travel speed, lifetime, mesh, sounds, and unrelated tuning are preserved. New effects are tunable under Runtime Balance on their cards. In BP_Ninja's NinjaBuildComponent, **Shuriken Burst VFX** optionally supplies the circular slash; empty uses the existing upgrade-accent presentation.

`Tools/import_shuriken_icons.py` imports the 21 generated stance/upgrade icons with backups and gameplay-property checks; `-ValidateShurikenIcons` is read-only. Sources and exact prompts are recorded in `Art/UpgradeIcons/shuriken_generation.json`. Regression coverage: `HeavensDivide.Combat.ShurikenBuilds` and `NinjaBuilds`. Numeric balance still needs playtesting.

### Legacy shared Ninja supports

These cards remain available before stance selection and for legacy compatibility. Selecting any current Ninja stance converts their paid ranks into that stance's damage investment; the effects below no longer apply afterward.

Direct Ninja projectile hits embed a fragment **before damage resolves**, so even a one-hit kill scatters a blade toward nearby enemies. Each fragment deals 40% of its embedding hit's damage. Surviving enemies can store up to 12 fragments. Scattered fragments cannot embed more fragments.

- **Fragment Damage:** +20/30/45% fragment damage per Common/Rare/Epic rank, up to 5 ranks.
- **Fragment Load:** embed one additional fragment per hit per rank, up to 3 ranks.
- **Fragment Reach:** +15/22/30% scatter targeting range per Common/Rare/Epic rank, up to 5 ranks; base range 700 cm.

The old Ninja cards are retired, except **Shadow Step**, **Multiple Strikes** and **Afterimage Frenzy**, which retain Shadow Clone and its upgrades. **Venomous Kunai** is restored as a standalone card: Ninja attacks apply Poison, before stance selection; Barrage grants its own poison, while Fang and Giant Shuriken use their own trees. The other old poison upgrades remain retired. The removed cards cover the old projectile bonus, bounce/pierce/split, Fan of Blades, Blade Cascade, execution branches and poison scaling upgrades. Hemotoxic Reaction and Accelerated Venom are also retired alongside the retired poison scaling upgrades. Shared Marked Blade remains available. The first Ninja trial rewards a weapon route; after choosing a route, later Ninja trials offer eligible normal Ninja upgrades. None of the new Ninja attacks applies universal Prepare.

## Tag Team

Ninja's Tag Team attack uses her equipped stance and attack upgrades: Great Shuriken throws a shuriken, Returning Fang makes one outbound/return trip, and Barrage fires its upgraded projectile volley. Assist projectiles retain their source after the animation ends. Assists do not spend Grand Entrance or advance the active Ninja's Needle Rain counter. Both characters retain their normal materials during arrivals and assists; the swap color is reserved for the departure afterimage.

Prepare has been removed: Blade Wave no longer marks targets, and assists grant no reaction bonus damage or status spreading. The four Prepare skill-tree passives are retired; their paid ranks are refunded on profile load, while surviving skills remain connected.

Ninja selects targets around her upcoming assist position beside the active Samurai. At the throw notify, she selects a new target if the original one has died or left range. Samurai assists apply Bleed only with Blood Stance; Ninja assists apply Poison only with Venomous Kunai.

## Shared synergy upgrade: Grand Entrance

**Grand Entrance** (`GrandEntrance`) is a one-rank Epic synergy card available through normal Synergy offers, without a family prerequisite or discovery lock. After a successful player-initiated swap, the incoming character's next normal attack is enhanced:

- **Samurai:** a 360-degree slash with a **600 cm base radius**, scaling with basic attack area. All secondary targets take full normal attack damage; the primary target retains its normal technique modifiers.
- **Ninja:** **8 additional projectiles** in a **100-degree fan**, retaining normal projectile damage and acquired projectile modifiers. Extra projectiles combine with the normal volley.

For Returning Fang, Grand Entrance's extra projectiles become bonus damage on the next throw. For Great Shuriken, they become extra size on the next blade. Barrage retains the enhanced normal volley.

The enhancement is spent at the attack's hit/projectile notify. Canceled attacks before that point retain it. It does not stack, and leaving the active character clears an unused enhancement. Assists, automatic abilities and Double Cut follow-ups cannot spend it. Acquiring the card does not immediately arm it: swap after acquisition.

Balance is editable on `/Game/HeavensDivide/Upgrades/Synergy/DA_Synergy_GrandEntrance` under Runtime Balance: `SamuraiRadius`, `NinjaBonusProjectiles`, and `NinjaFanAngle`. The current icon and exact built-in imagegen prompt are recorded in `Art/UpgradeIcons/manifest.json`; `Art/UpgradeCards/GrandEntrance.json` retains the earlier illustration.

## Preview and maintenance

In a non-shipping build, press **5** during a live run to open the normal Blood Shrine upgrade selection. The shortcut respects stance prerequisites and owned ranks, and is ignored while paused, dead, or already choosing a reward. The binding and handler are absent from Shipping builds. Keys 1-4 remain editor-play-only shortcuts.

Only the six actual stance data assets contain `Stance` in their asset names. In the Content Browser, use `Name=Stance AND Type=UpgradeDefinition` to exclude matching textures, effects, and metadata. `Tools/rename_stance_assets.py -ValidateStanceNames` checks this rule across all saved upgrade DAs without changing them.

In a non-shipping build, `BuildPreview BladeWave` grants Crescent Stance and can grant the enabled Returning Blade branch. Other legacy branch arguments cannot grant temporarily disabled support cards. `BuildPreview All` and the legacy `AbilityShowcase` command now preview only Blade Wave. The Blade Wave preview respects stance exclusivity and cannot replace Iaijutsu Stance or Blood Stance. These commands affect the current run only.

- `Tools/build_family_catalog.json`: Blade Wave is the only available family. Retired entries retain their stable internal indices for compatibility and stale-ID rejection.
- `Tools/generate_build_catalog.py`: regenerates the runtime catalog.
- `Tools/configure_build_families.py`: authors only available families.
- `Tools/remove_automatic_ability_upgrades.py`: removes retired saved assets with backups; `-ValidateAttackRoster` verifies the saved pool without editing.
- `Art/UpgradeIcons/manifest.json`: the current artwork catalog and exact built-in imagegen prompts for all 108 saved upgrade assets. `Art/UpgradeCards/manifest.json` retains the earlier family illustrations.

Normal-attack execution and Blade Wave still use `AutoAttackComponent` and `SamuraiBladeWave`. `SurvivorAbilityComponent` retains assist, presentation and Blade Wave hit support; automatic casts are disabled.

## Upgrade artwork

Player-facing names and descriptions are maintained in the [card copy catalog](CardDescriptions.md). The upgrade gallery shows the current published wording; stable IDs in this document remain unchanged.

All **108 saved upgrade assets** have distinct clean cartoon icons assigned to both **Icon** and **Card Artwork**, including the 106 pool entries and the two legacy assets remaining on disk. Artwork does not change which upgrades are enabled or offered. The icon style uses bold contours, simple silhouettes, category colors, and dark teal backgrounds. The card artwork panel fits the full symbol without cropping it.

- Browse the searchable [upgrade icon gallery](../Art/UpgradeIcons/index.html). Original PNGs are in `Art/UpgradeIcons/Generated`.
- `Art/UpgradeIcons/manifest.json` maps stable upgrade IDs to source PNGs, Unreal textures, and exact generation prompts. Its `artwork` and `icon` fields record the previous references.
- `Tools/import_all_upgrade_icons.py` imports the set and assigns both artwork slots. It backs up affected assets under `Saved/Backups/UpgradeIcons` and compares all serialized non-art properties before and after.
- Run the same script with `-ValidateAllUpgradeIcons` for asset-read-only verification. `Tools/import_upgrade_art.py` and `Tools/validate_upgrade_art.py` also use this catalog.
- Textures live in `/Game/HeavensDivide/Blueprints/UI/UpgradeIconArt`, with a 512-pixel runtime cap, UI compression, and no mipmaps. Original source PNGs retain their generated resolution.
- `Tools/build_upgrade_icon_gallery.py` rebuilds the offline gallery after a manifest change. Re-run the artwork importer after any legacy authoring workflow that replaces artwork references.

## Samurai build authoring

`Tools/samurai_stance_expansion.json` defines the eight additions/re-enabled cards. `Tools/complete_samurai_stance_trees.py` adds the seven new assets, re-enables and balances Returning Blade, and updates Sudden Eruption text with backups under `Saved/Backups/SamuraiStanceTrees`. `-ValidateSamuraiTrees` checks the 156-card pool, 98 enabled cards, all three 21-card trees and their 6/5/6/3 categories without writing. Other upgrade assets and their tuning are preserved. `HeavensDivide.Combat.SamuraiTreeStructure` covers tree counts, prerequisites, rank caps, ordinary offers, stale-reference rejection and restoration.

`Tools/balance_samurai_upgrades.py` updates the three Double Cut Frequency caps and tradeoff/field descriptions with backups under `Saved/Backups/SamuraiBalance`; `-ValidateSamuraiBalance` is read-only. All three frequency cards cap at three ranks, reaching every attack. Old fourth ranks clamp to three on restoration without changing attack behavior or mastery. `HeavensDivide.Combat.SamuraiStanceBalance` covers caps, stale references, tradeoff conversions, offer exclusions and save restoration; `CrescentBuilds` checks actual field and eruption damage at every rank.

`Tools/overhaul_blood_stance.py` authors the current Blood cards and stance display names, preserves unrelated card tuning, and removes retired Bleeding Edge/Deep Cuts references from the pool. Backups are under `Saved/Backups/BloodStance`. `-ValidateBloodStance` validates the 156-card pool without writing. New cards reuse existing Samurai illustrations.

`Tools/samurai_build_upgrades.json` remains the seed catalog for the earlier shared supports. `Tools/configure_samurai_builds.py` forwards to the current Blood authoring workflow. Runtime behavior is in `SamuraiAutoAttack.cpp`, `SamuraiBuildUpgrades.cpp`, and `EnemyStatusEffectComponent.cpp`; Shrine routing is in `PlayerUpgradeSelection.cpp` and `SurvivorPlayerController.cpp`.

`HeavensDivide.Combat.SamuraiBuilds` covers exclusivity, stack caps and refresh, damage totals, transfers, detonation, Double Cut frequency, Shrine routing, disabled-card rejection, and base wave spawning. Balance still needs playtesting.

## Ninja testing and authoring

During a run in a non-shipping build, switch to Ninja and enter one of these console commands:

- `NinjaBuildPreview Fang`
- `NinjaBuildPreview Barrage`
- `NinjaBuildPreview Shuriken`
- `NinjaBuildPreview Clear`

All three Ninja previews grant their stance and all 17 ordinary upgrade cards at one rank. Shrine tradeoffs are not granted by the previews. Switching previews replaces the new Ninja build cards while preserving other acquired upgrades. Clear removes the new Ninja cards. These commands affect only the current run.

`Tools/ninja_build_upgrades.json` retains the legacy Ninja seed catalog; `Tools/barrage_build_upgrades.json` defines the new Barrage tree; `Tools/fang_build_upgrades.json` defines the new Fang tree and basic investments. `Tools/configure_ninja_builds.py` creates missing cards and updates the controller pool while preserving existing card tuning; `-ValidateNinjaBuilds` validates without writing. Cards reuse existing Ninja illustrations. Fang and scattered kunai reuse the original Ninja projectile Blueprint visuals, trails and impact sound. The giant shuriken retains its spinning mesh visual.

`NinjaBuildComponent` implements the weapons and fragments. `HeavensDivide.Combat.NinjaBuilds` exercises the saved Ninja Blueprint, real volley spawning, timer-independent Fang returns, repeated shuriken hits, growth with distance, lethal-hit fragments, stance exclusivity and preview switching.

## Legacy Samurai cleanup

The old Cleaver, Duelist and Deathblow technique cards are removed. The first Samurai trial rewards a stance; after choosing one, later Samurai trials offer eligible upgrades from the current Samurai build pool. The old technique effects cannot activate from stale run data.

There are 71 Samurai-owned cards (63 enabled), including three Shrine-only tradeoffs for each of Blood, Iaijutsu and Crescent. Bleeding Edge and Deep Cuts are retired; Blood Stance now grants Bleed. Shared synergies and Ninja/global upgrades remain available. Existing tuned card assets are preserved without rewriting their values.

`Tools/remove_legacy_samurai_techniques.py` removes the three legacy assets and verifies retained card file hashes. Backups and hashes are under `Saved/Backups/LegacySamuraiCleanup`. Their shared artwork is retained because current build cards reuse it.

`Tools/remove_legacy_ninja_upgrades.py` removes the 12 retired cards and changes Wide Orbit into Growing Shuriken, preserving all other saved card tuning and Shadow Clone artwork. Backups are under `Saved/Backups/LegacyNinjaCleanup`; `-ValidateNinjaCleanup` checks the saved roster without writing. Growth distance and maximum size multiplier are editable on the Growing Shuriken card under Runtime Balance.

## Ninja clone attacks and weapon speed

Shadow Clones use the Ninja's current weapon: Barrage/normal projectiles, Great Shuriken, or an independent Returning Fang that returns to the clone. Multiple Strikes increases the attack quota. A Fang round trip counts as one attack and immediately relaunches while attacks remain; Afterimage Frenzy increases its travel speed. Barrage counters belong to each clone. Clone attacks do not spend Grand Entrance or advance the player's attack counters.

Open `/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaGreatShurikenStance` and edit **Runtime Balance > Balance Parameters > TravelSpeed** (cm/s, default 900). This directly controls player and clone shuriken speed, independently of normal kunai speed. Grinding Halt can still briefly slow a blade. The stance's old projectile-speed stat modifier no longer determines giant shuriken travel speed.

`Tools/restore_ninja_poison.py` restores the original poison starter from the cleanup backup and re-adds it to the saved pool, preserving other card tuning.

`Tools/update_barrage_upgrades.py` is the earlier Crossfire/Crescendo migration. Current Barrage authoring uses `Tools/overhaul_barrage.py`.

### Shuriken mesh

In `BP_Ninja`, select `NinjaBuildComponent` and open **Ninja Builds > Shuriken**. Assign **Shuriken Mesh** to use a custom static mesh for normal attacks, Tag Team, and Shadow Clones. Leave it empty for the existing placeholder. Author the mesh flat in XY with its pivot at the center; it spins around Z. The mesh fits the attack radius and grows with Wide Orbit. **Shuriken Mesh Scale** adjusts only its appearance, not hit detection. Materials come from the assigned mesh.

Ninja Tag Team hits apply poison only when **Venomous Kunai** is acquired, on legacy regular projectile assists. The new Fang and Giant Shuriken trees exclude Venomous Blades; Barrage uses its built-in poison.
`Shuriken Hit Sound` in the same component category overrides impact audio for giant shurikens from normal attacks, Tag Team, and clones. Empty uses the normal projectile hit sound; kunai, Returning Fang, and scattered fragments keep their existing audio.
`Shuriken Throw Sound` in **Ninja Builds > Shuriken** replaces the sound on the Ninja attack montage when Great Shuriken is equipped, including Tag Team and clones. Empty retains the authored montage sound. Normal kunai still use the sound assigned to the **Ninja Throw Sound** notify in `AM_AutoAttackNinja`; its timing, volume, and pitch are preserved.

## Trial reward routing

The six build starters remain in the shared catalog for save restoration, prerequisites, mastery, and debug previews, but regular offers filter them out. Character trials offer only eligible starters while a route is unchosen, then fall back to that character's ordinary eligible cards. Eligible branches and support upgrades stay in the normal pool. Overkill and legacy Crescent support upgrades are temporarily disabled. Each stance offers only its own upgrade set, including its Shrine tradeoffs. The shared Samurai damage/speed/area investments are available before stance selection and convert into the selected stance's forms without losing ranks. Old Samurai routes are normalized as described above. Existing wave, Iaijutsu, and support-card tuning is preserved.

Implementation: `PlayerUpgradeSelection.cpp` and `PlayerUpgradeRules.cpp`. Regression coverage: HeavensDivide.Combat.TrialBuildRewards (both characters, all six route selections, normal/direct offer exclusion, exclusivity, and repeat-trial rewards).

### Tradeoff-aware upgrade cards

Reward cards describe their active effect after a Shrine tradeoff, while retaining their IDs, prerequisites, ranks and tuning. Serpent's Procession keeps both split upgrades available: Forked Blades becomes **Rebounding Blades** (3 percentage points less damage lost per ricochet), and Branching Blades becomes **Ricochet Mastery** (2 points per rank). Existing ranks provide the same benefit.

With Sudden Eruption, Enduring Wake becomes **Violent Wake**, increasing eruption damage, and Restless Wake becomes **Restless Eruption**. Blood Hunger changes Shuriken Size to **Ravenous Growth**, increasing size gained per kill. Venom Bloom changes Spreading Blight to **Spreading Bloom**. Crimson Reach describes detonation radius when Blood Detonation is active. These are presentation changes; useful upgrades remain obtainable.

`HeavensDivide.Combat.StanceDependencies` checks all six stances, mechanic prerequisites, incompatible rewards, retained tradeoff benefits, save restoration, and contextual card presentation.
