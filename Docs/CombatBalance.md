# Fixed-health combat balance — first playable pass

The target is a difficult fresh-save run that becomes easier through permanent progression and player skill. These are analytic starting values, not a measured first-win rate. Keep the permanent skill tree and Ember economy; remove hidden damage from mastery. Mastery still tracks investment. Blood and Barrage stances plus Tag Team start unlocked; other stances and synergies cost Soul Sigils. In-run stance prerequisites and rank limits still apply.

## Sources and reproducibility

- Coordinated overrides: Tools/combat_balance_v1.json.
- All 150 retained pool definitions are recorded in Tools/combat_balance_reference.json. The unused-upgrade cleanup below removed 15 obsolete assets, including eight formerly pooled cards that were already blocked from offers.
- Apply assets with Tools/apply_combat_balance_v1.py through Unreal's Python commandlet. Every edited package is backed up; an existing Balance Lab map is preserved.
- Run "python Tools/analyze_combat_balance.py" to produce Saved/BalancePass/MathReport.json: every definition's coefficients/rank totals and all 26 enemy-health budgets.
- Historical setup scripts can restore older tuning. Apply this coordinated pass after them. Docs/EnemyWaves.md describes the historical schedule.

## Starting attacks

Samurai's base hit is **8** (previously 5); Ninja stays **10**. Both have a 1.2-second base interval. Blood retains full secondary-target damage. These figures exclude avoidance, misses, overkill, assistance and combos.

| Stance | Fresh-save damage model | Purpose |
| --- | --- | --- |
| Blood | 8 × 1.3 / 1.2 = **8.67 direct DPS**, up to **1.67 bleed DPS** at five stacks | Close-range cleave and sustained status damage |
| Crescent | 8 × 1.5 / 1.2 = **10 DPS per pierced target** | Range and reliable lane coverage |
| Iaijutsu | 8 × 2.5 / (2 + 0.5) = **8 DPS**; **12** against an already marked target | Committed hits and setup for later attacks |
| Returning Fang | 10 / max(0.5, round-trip time) = **up to 20 DPS**; point-target estimate **7.5** at 600 cm | Close-range single-target specialization; return deals no damage |
| Barrage | 6 × 1.3 / 1.2 = **6.5 DPS per connecting projectile**, up to **1.2 poison DPS** at 20 stacks | Up to 19.5 direct DPS when all three kunai connect |
| Great Shuriken | 15 / 2.4 = **6.25 DPS per contact per throw**; two contacts = **12.5** | Reward keeping enemies in the moving blade's path |

DoT figures are steady-state caps after stacking, not immediate damage. Refreshing a status can exceed one application's nominal total damage over a long fight. Shuriken can exceed the two-contact estimate on large or stationary enemies; its stationary six-contact baseline ceiling is 37.5 DPS. This is not its assumed ordinary DPS.

A point-blank lab sample originally measured about 35 Barrage DPS because all three kunai connected. Barrage now applies a 0.6× stance damage coefficient to each kunai (and inherited poison), retaining its coverage while controlling full-volley damage.

Crescent previously dealt only 3.2 damage even after raising the Samurai base to 8. Iaijutsu waited through its charge for a plain base hit. Their new stance coefficients fix those gaps without multiplying unrelated Samurai assists.

## Upgrade budgets

    hit = base damage × character damage × shared damage × stance/family multiplier
    ordinary DPS = hit × attacks per second × expected connecting hits
    critical multiplier = 1 + crit chance × (critical multiplier − 1)
    extra attacks per ordinary attack = extra attacks / trigger frequency
    capped status DPS = applying hit × fraction × stack cap / base duration
    TTK = enemy HP / effective DPS actually reaching that enemy

Percentage-point increases accumulate within their card; they are not a fresh multiplier every rank. Mastery contributes **1.0×**, even when an old Blueprint has a nonzero mastery-power setting.

- Direct damage rarity steps: **20/30/40%** where cards use rolled rarity. Iaijutsu/Crescent retain explicit 20% damage ranks. Global damage: **10/15/20%**.
- Ordinary attack-speed rarity steps: **10/15/20%**. Fang flight-speed investment retains 15/22.5/30%; travel and the two-launch-per-second ceiling limit throughput. Crescent Wave Speed changes travel, not firing cadence. Iaijutsu ordinary attack speed changes recovery; Charge Speed changes the charge.
- A 15% double-damage crit adds 15% expected direct damage. Existing ten-point critical-chance ranks remain useful; they were reviewed rather than universally reduced.
- Blood Echo starts at 20% chance and repeats a full-strength hit: **20% expected extra hit damage** before bleed/crit interactions. Echoes cannot recurse.
- Double Cut and equivalent frequency routes retain four-to-one attack frequency over three support ranks. A full extra hit every fourth attack is +25% expected damage before geometry; every attack is +100%, requiring the mechanic plus three ranks.
- Twin Fang now adds one spectral Fang instead of two. Its previously zero-valued frequency support advances the trigger by one launch per rank. Four purchases therefore follow the +25% to +100% extra-projectile budget.
- Flash Draw gains five percentage points per rank: maximum 40%, preventing near-constant instant charged attacks. Crescent Arc adds five points because a proc adds two waves; Splitting Waves adds ten.
- Crimson Power: 2.5× bleed / 0.7× attack speed. Its value depends on status investment. Blood Detonation consumes stacks for 1.5× remaining damage.
- Scorched Wake: 0.8× wave contact / 2× field damage. Farstrider tops out at +200% after 1000 cm of outward travel, with slower flight. Predator retains its critical payoff while ordinary hits deal 85% damage.
- Poison duration and load each add one unit per rank. Pool spread still cannot recursively seed itself without a direct kunai hit.

Other active attack paths retain explicit budgets: Tag Team follows the assisting character’s selected stance: Blood uses its 12-damage setup slash, Crescent launches its tuned wave, Iaijutsu performs its charged lane slash, and Ninja uses the selected Fang/Barrage/Shuriken attack with the existing 10-damage assist base; Marked Blade doubles one consumed Ninja hit; Grand Entrance changes coverage or the next projectile budget; shadow clones inherit tuned Ninja attacks with finite quotas and the existing five-clone limit. None receives mastery damage. Unavailable automatic ability families remain unavailable.

Combo attacks now total **80 Samurai / 81 Ninja base damage** if every pulse connects (16 × 5 and 18 × 4.5), down from 192/180. Character/shared damage still applies. Meter generation and keeping targets in the effect limit actual output. Saved swap cooldown is 3 seconds.

## Enemy health and rewards

Elapsed time no longer changes HP. Native spawning ignores legacy growth overrides; their editor controls are removed. Marching enemies use fixed HP. Blood Shrines retain an explicit temporary **1.5× challenge health modifier** (down from 3×), not time growth; their damage modifier is 1.2×.

Basics take roughly 1–4 unupgraded hits; Bear takes five Samurai hits as the slow tank. Tactical budgets assume about 30 effective DPS after investment. Elite rewards are around 11 XP per 100 HP. Final-boss HP assumes roughly 60 effective late-run DPS for a 100-second fight: 40 DPS takes 150 seconds, 80 DPS takes 75. Verify in moving combat.

| Enemy | Fixed HP | XP | Seconds at 30 effective DPS |
| --- | ---: | ---: | ---: |
| CinderOracle (Veshra) | 6000 | 0 | 200.0 |
| SamuraiBoss | 6000 | 0 | 200.0 |
| EnemyGorilla | 660 | 72 | 22.0 |
| EnemyOgre | 264 | 30 | 8.8 |
| EnemyBear | 35.2 | 5 | 1.2 |
| EnemyCreature | 26.4 | 4 | 0.9 |
| EnemyDevilRanged | 52.8 | 8 | 1.8 |
| EnemyFish | 8.8 | 2 | 0.3 |
| EnemyFloatingSkeleton | 17.6 | 3 | 0.6 |
| EnemyGoblinBomb | 35.2 | 6 | 1.2 |
| EnemyGoblinCrawler | 13.2 | 3 | 0.4 |
| EnemyGrunt | 17.6 | 3 | 0.6 |
| TestDummy | 1000 | 0 | 33.3 |
| AshenMarcher | 26.4 | 3 | 0.9 |
| AshSeer | 61.6 | 9 | 2.1 |
| FangStalker | 52.8 | 8 | 1.8 |
| FrostOracle | 105.6 | 15 | 3.5 |
| GraveCantor | 105.6 | 15 | 3.5 |
| HexSniper | 70.4 | 10 | 2.3 |
| HornLancer | 70.4 | 10 | 2.3 |
| MireWeaver | 79.2 | 11 | 2.6 |
| OgreWarden | 440 | 48 | 14.7 |
| StormGorilla | 825 | 90 | 27.5 |
| WarDrummer | 158.4 | 22 | 5.3 |

Technique and Twin Soul trials are retired and their targets no longer enter a run. Boss HP is set on the setting BeginPlay actually uses. Grave Cantor heals 2% across at most four allies instead of 5% across eight.

Basic attacks deal 6–14 damage; tactical attacks 8–28; major boss attacks 16–28. Existing enemy **damage** growth remains. A quarter-second player hit-grace window prevents simultaneous swarm hits; enemies have zero grace so multi-hit builds still work.

## Run pacing and progression

- Keep basic population growth and enemy introduction times; composition and density replace HP inflation.
- Timed threats share a six-enemy ceiling plus per-class caps. Spawn-slot chance is 18%, down from 30%. The separate marching formation is not counted in this ceiling.
- Blood Shrines: 3 and 5 minutes; boss gate: 10 minutes. Technique Trials and Twin Soul trials no longer spawn, including through legacy debug shortcuts. Equip up to five unlocked synergies in New Run; synergies do not appear in run drafts.
- Preserve the XP curve and 30-seconds-per-Ember economy. Replace two nonfunctional XP knobs with a working whole-curve multiplier, default 1.0.
- Full permanent damage × ordinary cadence is approximately **2.08×** fresh-save output before stance-specific utility (1.17 shared damage × 1.33 character damage × 1.14 shared cadence × 1.17 character cadence). Projectile count exists only on the Barrage branch. See [MetaSkillTree.md](MetaSkillTree.md) for all six stance paths and their distinct mechanics.

Measure fresh-save runs, modest permanent investment, and heavy investment. Failed fresh runs should teach a visible threat and award useful currency. A first-win target of roughly 10–20 attempts is provisional; arithmetic alone cannot establish a win rate.

## Balance Lab

In Unreal, open **Tools → Balance Lab** (also under **Window → Balance Lab**, or the **Balance Lab** button in Bloodshift Balance). The dockable panel has descriptions above each group of controls.

1. Click **Open lab map**, then **Start test**. The map button uses Unreal's normal save prompt for unsaved work.
2. While playing, press **Shift+F1** to release the mouse and click the editor panel; click the game to resume movement.
3. Set **Damage + speed ranks** (0–5), then click a stance. This resets run upgrades and creates a fresh dummy. It preserves your selected permanent-stat and invulnerability settings.
4. Choose an enemy and count, then **Spawn selected targets**, or use **Fresh single dummy** / **12 Grunts**. These preserve the build and reset the measurement.
5. Use **Fresh stats** / **Full stats** to preview permanent combat bonuses without changing saved skills. Search/select an upgrade and click **Grant selected upgrade** for individual cards; the feedback line reports how many ranks were granted.
6. Use **Invulnerability ON**, **Incoming damage ON**, and **Fill combo** as needed. Fill combo requires normal ability input afterward.
7. Attack for 30–60 seconds and click **Save report**, then **Open reports folder** to view Measurements.txt. Live DPS and the current combat profile appear at the top. **Stop test** ends the lab session.

Test controls require a living lab player in Play In Editor. They stay disabled outside the lab, during startup, and after death. Standalone games still use the console commands below.

Alternatively, open **/Game/Maps/Lvl_BalanceLab** and press Play. It starts with Blood Stance, fresh permanent combat stats, a stationary high-health target and player invulnerability. Approach the target for melee tests. Open the console with tilde:

    BalanceLab build Blood 0
    BalanceLab build Crescent 3
    BalanceLab build Iaijutsu 3
    BalanceLab build Fang 3
    BalanceLab build Barrage 3
    BalanceLab build Shuriken 3
    BalanceLab target Dummy 1
    BalanceLab target Grunt 12
    BalanceLab target OgreWarden 1
    BalanceLab target StormGorilla 1
    BalanceLab grant BloodCritical 1
    BalanceLab meta full
    BalanceLab meta fresh
    BalanceLab god off
    BalanceLab god on
    BalanceLab combo
    BalanceLab report
    BalanceLab clear

The combo command fills the meter; activate it with the normal ability input. After granting cards during a sample, use `BalanceLab target Dummy 1` to clear existing statuses and start a clean measurement.

The build number gives that many common damage/speed ranks (0–5); it does not grant every branch. Grants respect prerequisites/exclusions/caps. Builds include a basic opposite-character stance for swap checks. Meta profiles affect combat stats only and never purchase/reset skills. The lab ignores local enemy-health overrides and awards no XP or Ember run reward.

The overlay shows actual accepted health damage, time since first hit, aggregate DPS, damage events and kills. Reports append the card/rank list to **Saved/BalanceLab/Measurements.txt**. Compare single-target samples with single-target samples; pack DPS is aggregate across targets. Use 30–60 second samples while stationary and while dodging. Record time to kill and damage taken too.

Automated smoke option: launch this map with **-game -BalanceLabSmoke -MetaSaveSlot=Automation_BalanceLabSmoke**. It checks damage from all six stances at zero and three investment ranks. This is not a win-rate simulation.

## Measured starting point

The final headless lab smoke passed all 12 samples (six stances at zero and three common damage/speed ranks). Each sample lasts nine game seconds against a stationary dummy at 180 cm. The meter starts on the first hit, so these short samples exclude opening windup and have attack-phase bias. They are comparison checks, not steady-state or moving-boss DPS.

| Stance | Zero ranks, reported DPS | Three ranks, reported DPS |
| --- | ---: | ---: |
| Blood | 10.2 | 20.2 |
| Crescent | 10.8 | 16.5 |
| Iaijutsu | 12.4 | 23.5 |
| Fang | 20.7 | 33.3 |
| Barrage | 21.0 | 43.8 |
| Shuriken | 14.4 | 30.3 |

Crescent invests in wave travel speed and Fang can reach its launch cap, so equal rank counts do not promise equal single-target gains. Their coverage, range and proc routes matter. Detailed samples are saved in Saved/BalancePass/MeasuredDps.json.

For the next gameplay pass, run at least five fresh-profile survival attempts and five with current permanent upgrades. Record death time/cause, player level at 3/6/8 minutes, elite kill times, and boss kill time. Change one category at a time: early crowd density for early deaths, XP/reward pacing for insufficient build growth, and elite/boss HP for excessive fight duration. Aim initially for a 75–150 second boss fight around 40–80 effective DPS; then tune the desired first-win difficulty from real attempts.

## Validation

The analytic script checks four rank profiles and all authored enemy HP values. Native automation covers fixed HP despite stale growth values, mastery without power, threat caps, player-only grace, working XP scaling and saved starter values. Existing combat suites cover stance mechanics, status damage, rewards and upgrade conversion. Actual execution logs are under Saved/BalancePass. Proposed budgets are not measured combat results.

Original balance-pass validation (before unused-upgrade cleanup): native Development Editor build passed; all 24 headless logic tests and all 28 rendered tests passed; 192 saved-package checks passed; all 12 six-stance lab smoke samples passed; math covers 158 definitions and 26 enemies. Final logs: TestsLogicFinal.log, TestsRendered.log, VerifyComplete.log, LabSmokeFinal.log, plus Validation.json under Saved/BalancePass. The enemy-cleanup comparison baseline was refreshed after the intentional asset retuning.

The arena was also launched with rendering and its initial floor/player view inspected (Saved/BalancePass/LabPreview.png). Rendered validation used offscreen D3D12 raster mode; project ray-tracing settings remain unchanged.

Balance Lab editor panel validation: Development Editor build passed; `HeavensDivide.Balance.LabEditor.Panel`, `HeavensDivide.Balance.LabEditor.PlayControls` and the existing `HeavensDivide.Balance.Workspace` tests passed. The panel was rendered and visually inspected. Live PIE checks covered build rank assignment, individual grants, rejected grants, permanent-stat previews, incoming damage, target counts, clearing, combo filling and report writing. Evidence: Saved/BalanceLabEditor/Tests.log, Automation/index.json and Panel.png.

## Unused upgrade cleanup and Tag Team

The saved upgrade pool now contains 150 unique cards. Removed 15 unused upgrade assets and their 15 exclusively owned icon textures after checking package references and retained prerequisites. The retained cards, their ranks and tuning are unchanged.

Removed IDs: `BladeWaveHaste`, `BladeWavePower`, `BleedingEdge`, `BurstRadius`, `Crescendo`, `CrossingBlades`, `CuttingReturn`, `DeepCuts`, `FinalPursuit`, `FocusedVolley`, `HeavyShuriken`, `OverkillBurst`, `SplinterWave`, `WaveMultishot`, `WideArc`. The eight previously disabled Samurai cards are permanently retired; restoring an older run drops their ranks, magnitudes and definitions while preserving earned mastery.

Tag Team now checks the **assisting character’s stance**. Crescent fires a wave without a preceding melee hit. Iaijutsu charges and slashes its lane, with its assist visibility timer covering the charge and recovery. Blood keeps its melee setup attack. Ninja keeps the corresponding Fang, Barrage or Shuriken volley. Assists do not advance normal attack cooldowns or consume stored Crescent/Iaijutsu extra-hit counters. Duplicate animation notifies cannot add a second damage path.

`Tools/unused_upgrade_cleanup.json` records the removal manifest. `Tools/cleanup_unused_upgrades.py` applies the cleanup; run it through Unreal Python with `-ValidateUnusedUpgrades` for read-only checks. The script backs up assets before deleting and rejects unexpected references or missing retained prerequisites. Original packages are under `Saved/UpgradeCleanup/Backups/20261010_173530`; audit and validation evidence is under `Saved/UpgradeCleanup`. Historical asset-generation scripts can recreate retired cards: rerun the cleanup after using those scripts.

Cleanup validation: Development Editor build passed. All 14 relevant automation tests passed across `Automation/index.json` and the final targeted rerun in `AutomationFinal/index.json`, including all six Tag Team stances, stored extra-hit counters, cooldown preservation, duplicate notifies, old-run retirement, the balance catalog and montage-owned Ninja VFX. All 186 saved balance checks passed; the math report covers 150 cards and 26 enemies. SHA-256 comparison confirms all 150 retained upgrade packages are unchanged. See `Saved/UpgradeCleanup/ValidationSummary.json`, `BuildTests.log`, `TestsFinal.log` and `Verify.log`.

### Boss-entry assist audit

Restoring a run now cancels any old assist and refreshes Tag Team's attack bindings after the upgrade snapshot is installed. This covers boss arrivals that keep Samurai active and therefore bypass the swap refresh. Assist placement aligns each character's feet to the current floor; melee arrivals clear the combined target and assistant capsule radii, search around the full target, and stay within arena bounds. Disabling the Tag Team counter without canceling an ongoing assist preserves its cleanup timer.

Validation: Development Editor build and all 12 targeted tests pass. The Tag Team regression restores captured runs with either character active, triggers assists against a large boss, verifies melee damage and raised-floor placement, and advances an allowed assist through cleanup after deactivation. The other checks cover all stance builds, combo behavior, swap freezing and run loadouts. Evidence: `Saved/TagTeamBossAudit/ValidationSummary.json`, `BuildFinal.log`, `TestsPassed.log` and `AutomationPassed/index.json`.

## Unlocks, currencies and run preparation

New Run has two setup pages: **Arena & Stances** contains the map, five optional challenge modifiers and unlocked stance choices; **Synergies & Combo** equips up to five owned synergies and one combo ability. Level 1 is playable; two future destinations are disabled placeholders. The first profile selects Blood + Barrage with Tag Team equipped. Buying a stance or synergy makes it available in New Run; resetting progression also clears invalid selections. Run retries and boss travel keep the chosen loadout and modifiers.

Selected synergies are active from the start of the run. They never appear as a category or card in level-up, random or chest drafts. Their existing effects and trigger conditions still apply. Soul Convergence is the existing combo, with Samurai/Ninja variants selected by the active hero. Unlock it in Abilities, then equip it in New Run; **None** leaves the combo slot empty. Future combo choices are disabled placeholders. Selections persist between maps and retries during the current game session.

The Skill Tree retains its ink/gold styling and has four pages:

- **Stances:** Blood and Barrage are free. Iaijutsu, Crescent, Returning Fang and Great Shuriken each cost **18 Soul Sigils**. Open an owned stance to purchase its three-node Ember tree.
- **Synergies:** Tag Team is free; every other supported synergy costs **12 Soul Sigils**. Buying it makes it selectable in New Run. Five can be equipped at once.
- **Abilities:** Soul Convergence unlocks the combo burst for **30 Embers**. The additional dash charge costs **60**, rerolls **45** and banishes **45**. Combo meter gains and combo damage have three +5% ranks each. Swap recharge purchases also live here; basic swapping remains available from the start.
- **Stats:** health, damage, movement, XP pickup, adaptive attack cadence, adaptive attack coverage and currency gains. Soul Harvest has five +5% ranks; Expanding Presence has five +4% ranks. Existing shared and hero-wide stat investments remain valid.

Generic cadence translates to attack rate for Blood/Crescent/Barrage, charge speed for Iaijutsu, flight speed for Returning Fang, and contact/pulse frequency for Great Shuriken. Generic coverage translates to Samurai attack width/radius, Fang targeting reach, Barrage kunai and poison-pool size, or Shuriken blade radius. Bonuses follow the relevant hero's owned stance, including assists. Barrage's extra projectile remains a Barrage-specific purchase.

**Every permanent purchase costs 3× its previous price.** Skill ranks cost 3 × BaseCost × next rank, including generic stats, stance-specific skills and ability improvements. Stance and synergy unlock prices are also tripled. For example, Living Flame now costs 15 / 30 / 45 Embers for its three ranks. Refund returns the recorded Ember spend, including the original price of older purchases, and keeps purchased stance/synergy unlocks. The 45-node skill catalog is stored in DA_GlobalBalance; the price multiplier applies to its values at runtime. Use Tools/configure_meta_stance_tree.py after rebuilding to install native catalog changes without overwriting other balance rules.

All skill maps grow from top to bottom. Branches share perpendicular connectors and keep each subtree together. Stance trees are vertical three-node chains; Stats and Abilities retain their actual prerequisites. Pan, zoom and keyboard/controller navigation remain available.

Loadout revision validation: the Development Editor build and all **16 targeted automation checks** passed. Coverage includes every permanent rank's 3× price, original-price refunds, starting equipment, the five-synergy limit (including restored runs), draft exclusion, combo selection and the 20-second wall expiry. New Run and vertical skill-tree captures were visually reviewed at 1280x720 and larger sizes. Reports are under `Saved/LoadoutRevision`, with the consolidated result in `ValidationSummary.json`. Validation used isolated automation saves; no packaged build or full-run difficulty playtest was performed.

### Challenge starting values

Ranks range from 0 (off) through 5. Effects are linear and fixed for the whole run; there is still no time-based HP growth.

| Modifier | Per rank | Rank 5 |
| --- | ---: | ---: |
| Enemy HP | +15% | +75% |
| Enemy damage | +10% | +50% |
| Enemy movement speed | +4% | +20% |
| Enemy population targets/caps | +6% | +30% |
| Spawn/replenishment frequency | +8% | +40% |

Each selected rank adds **8% permanent currency rewards**, up to +200% at 25 ranks. Spawn frequency divides intervals by its multiplier. The maintained population obeys a hard ceiling of 200; the current base cap of 144 becomes 187 at maximum Count. Timed formations are separate from this maintained population. Health/damage/speed also affect combat bosses. Walls, urns, training enemies and Balance Lab are excluded. These are initial playtest values, not a claimed win-rate target.

### Currency sources

Rewards are automatically collected; no extra pickup actors are created. The run HUD shows earned Embers/Sigils. Death keeps earned currency. Writes are batched every five seconds and flushed at run end/menu/shutdown; a failed write retains both pending currencies for retry. A hard process crash can lose the last unsaved batch.

| Source | Soul Embers | Soul Sigils |
| --- | ---: | ---: |
| Every fifth ordinary eligible kill | 1 | 0 |
| Tactical kill | 2 | 0 |
| Elite kill | 3 | 1 |
| Open elite chest | Tier roll (see below) | Tier roll (see below) |
| Complete Blood Shrine | 10 | 2 |
| Complete Ember Circle | 20 | 1 |
| Survive 30 seconds | 1, paid at run end | 0 |
| Boss victory bonus | 20 | 5 |

An elite and its chest both pay their listed amounts. Currency bonuses use (1 + challenge bonus) × (1 + Soul Harvest bonus), then save fractional earnings across runs and game restarts rather than rounding every small drop up. Boss summons, despawns, walls, urns and Balance Lab grant no permanent currency. Existing XP and chest card rewards remain separate.

Version 6 preserves Ember balances, Sigils and invested skill ranks. It replaces the older blanket availability/trial discovery state with Blood/Barrage/Tag Team defaults. Existing ranks for a now-locked stance are retained but inactive until that stance is purchased; refunding ranks still returns the recorded spend. Reset Progression clears ranks, paid unlocks and both currencies.

Technique Trials and Twin Soul trials remain retired. Legacy classes/fields stay loadable for compatibility and cannot grant unlocks. Ordinary run cards and Blood Shrine pacts remain available subject to their normal stance/prerequisite rules.

### Elite chest tiers and jackpot

Each chest rolls once when initialized. XP uses the defeated elite's authored XP as its baseline; integer percentage rolls and currency rolls are inclusive. Skipping, repainting and repeated initialization cannot reroll the reward. Empty/invalid XP rewards grant no currencies.

| Tier | Chance | XP baseline multiplier | Embers | Soul Sigils |
| --- | ---: | ---: | ---: | ---: |
| Common | 70% | 0.80–1.20× | 3–5 | 1 |
| Rare | 23% | 1.25–1.75× | 6–9 | 1–2 |
| Epic | 6% | 2.00–3.00× | 12–18 | 2–3 |
| Jackpot | 1% | 5.00–8.00× | 40–60 | 6–10 |

The weighted averages before currency bonuses are 1.26× baseline XP, 5.925 Embers and 1.275 Sigils. Tuning lives in `ChestRewardRoll.cpp`. Currency bonuses use the existing shared payout calculation, including saved fractions. The reveal shows the final currency amounts; XP remains an orb shower with a default budget of 12 actors (previously 40) and exact XP conservation. Designer overrides are bounded at 64.

Jackpots extend the normal reveal by 1.2 seconds, add a distinct original bell/arpeggio fanfare, a gold radial burst, a particle shower, and pulses when each reward card settles. Common/Rare/Epic keep the short reveal and use tier accents. Effects draw only in the chest overlay while the world is paused; no extra enemy ticks or reward pickup actors are added.

Reproduce the audio with `Tools/generate_chest_jackpot_audio.py`, then run `Tools/import_chest_jackpot_audio.py` in Unreal Python. Regression tests cover 100,000 seeded rolls, tier ranges/chances, overflow, zero rewards, XP conservation, repeated completion/initialization, jackpot pause/skip timing, bonus previews across saved fractions, and rendered normal/jackpot screens.

Validation: Development Editor build and all five focused tests passed (`Saved/ChestJackpot/FinalAutomation/index.json`). The seeded sample produced 984 jackpots out of 100,000 rolls. Normal and jackpot previews were visually reviewed, and the new MetaSound rendered twice with audible unclipped PCM, different pitch variations and correct one-shot completion. The older broad CombatPalette check still has a pre-existing 56-versus-58 catalog assertion; the focused jackpot audio check is independent and runs with audio enabled. Evidence is consolidated in `Saved/ChestJackpot/ValidationSummary.json`.

### Progression validation

Development Editor build passed (`Saved/ProgressionExpansion/BuildPresentation.log`). All 17 unique targeted automation checks pass across the initial rendered suite and focused reruns, covering purchases and rollback, save migration, reward fractions across reloads, challenge application, draft eligibility, all six stance conversions, combo ownership/effects, objectives, controller navigation and the currency HUD. Final results are consolidated in `Saved/ProgressionExpansion/ValidationSummary.json`; the underlying reports are `RenderAutomation`, `FinalAutomation` and `PresentationAutomation`.

The 45-node saved catalog was installed with a backup and verified without changing unrelated balance rules. New Run and progression/reward screens were visually reviewed at 1280x720 and larger sizes. Validation used isolated `Automation_` save slots. These checks do not replace long-run difficulty, income or FPS playtesting; no packaged build was performed.

### Research and tuning rationale

Soulstone Survivors uses selectable curses to increase difficulty and reward income; its modifiers include enemy health, damage, movement and extra enemy pressure. Its older published values vary across versions, so this project's five-rank values are original tuning rather than a copy of a version-specific curse table. See the [curse overview](https://game.lb-product.com/en/games/soulstone-survivors/guides/soulstone-survivors_curse-guide).

Its common resources from regular enemies and rarer resources from bosses also support separating repeatable stat purchases from new playstyle unlocks. This implementation uses just two currencies to keep that distinction readable. The [currency guide](https://progameguides.com/soulstone-survivors/all-currencies-in-soulstone-survivors-explained/) is dated November 2022 and is used as a design reference, not an assertion of today's exact economy.

## Population and HP adjustment

Routine enemy population targets and caps are about 10% lower, with spawn and pressure-event intervals divided by 0.9 (10% fewer spawn opportunities). The hard alive limit is 144, down from 160. The opening maintained population is 36, down from 40; later phases retain their timing and enemy mix. Small tactical caps keep their integer counts.

All 21 active mob, elite and tactical enemy types have 10% more fixed HP. Their damage and XP rewards are preserved. Bosses, test dummies, walls and urns retain their HP. The approximate full-population health budget is 0.9 x 1.1 = 0.99 of the previous budget, with fewer bodies and tougher individual enemies. This is a tuning estimate, not an FPS or win-rate measurement. Targets are in Tools/enemy_pressure_tuning.json; apply them with Tools/apply_enemy_pressure_followup.py.
