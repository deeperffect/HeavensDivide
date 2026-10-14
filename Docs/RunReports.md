# Run menus, damage and scores

Pause, defeat and victory now use the same procedural gold frame, ink surfaces, paper text and action buttons as the main-menu subpages. Their fixed navigation column exposes Overview, Run Build, Damage and Score. The report scrolls independently of the navigation and footer actions, and scales down with the viewport. Pause starts on Run Build; results start on Overview. Settings still opens the actual main-menu settings and returns to the paused run.

Run Build lists acquired cards under Samurai, Ninja and Shared & Synergies, with saved artwork, ranks, maximum-rank markers, stance/weapon labels and evolution markers from the asset's actual taxonomy. Hovering shows the asset's resolved balance description. Mastery, rerolls and banishes are included. Results keep a frozen copy of the final build.

Defeat results follow the death montage and final hold: the silhouette and YOU DIED fade to black over 0.45 seconds, then the report is revealed over 0.65 seconds. The transition uses real time while gameplay is paused; cursor and menu input become active once the report is fully visible. The controller's `Death Presentation` settings expose these durations and the slower 1.2-second title fade.

## Research and design choices

- Vampire Survivors uses weapon/evolution identity and per-weapon results to encourage build comparison. Player reports also illustrate why damage/DPS requires a clear definition: [overkill discussion](https://www.reddit.com/r/VampireSurvivors/comments/snhd1a), [different weapons compete for the same enemies](https://www.reddit.com/r/VampireSurvivors/comments/1dqwjo1). Our display measures actual health removed and labels the DPS denominator.
- Soulstone Survivors explicitly added skill damage breakdown to pause, plus match history with skills, times and per-skill damage. Its anniversary event ranked speedrun times rather than prescribing a universal point formula. [Developer announcement](https://store.steampowered.com/news/posts/?enddate=1699303654&feed=steam_community_announcements).
- League of Legends Swarm presents weapons and their linked evolution passives in its Tab menu. [Swarm evolution guide](https://mobalytics.gg/lol/guides/swarm-how-to-evolve-weapons). This informs the separation of owned character attacks and support choices, without importing Swarm's weapon-slot or evolution requirements into this game's stance system.
- Funguys Swarm's publisher emphasizes mid-run stacking upgrades, builds and biomes. [Official Steam page](https://store.steampowered.com/app/3371770/Funguys_Swarm/). Public material reviewed did not establish an exact score formula or detailed results attribution, so none is claimed or copied.

The score formula below is a new Bloodshift design. It combines survival, kills, successful objectives and victory, using the existing map-modifier reward bonus. It is not represented as any comparison game's proprietary formula.

## Damage accounting

`FRunDamageScope` identifies the attack or proc around the real damage call. `UHealthComponent` records accepted enemy HP loss before damage/death delegates can end the run. Rejected damage, overkill, enemy self-damage, training dummies, stress enemies, urns and walls do not count. Each accepted event belongs to its source once; nested calls restore the parent source, and delayed callbacks establish their own source.

Direct attacks, Iaijutsu, Crescent waves/fields, Ninja weapons, death scatter, bleed, poison, Blood Detonation, Blood Echo, Double Cut, Shadow Clone, inactive assists, Tag Team and combo damage are identified at their execution sites. Poison ticks shared by direct Barrage hits and pools split accepted HP loss according to each source's additive tick contribution; the larger share receives the killing blow. This bookkeeping does not change poison balance. The chart sorts by damage, shows shares, full-run DPS, hits and killing blows, and retains equipped direct effects with zero damage. Stat upgrades, attack cadence, area, critical amplification and other buffs are included in their resulting attacks. The support list explains this instead of inventing damage totals that would double-count the same damage. Utility value cannot be inferred from damage alone.

`URunReportSubsystem` lives in the GameInstance. A fresh run resets it; boss-arena restoration keeps it. Controller ticks accumulate active game time, including boss combat. Pause, objective intros and upgrade selection do not add survival points. Defeat/victory freezes the report before showing results. Returning to the menu from pause abandons the run without recording a leaderboard result.

## Score rules v1

| Component | Points |
| --- | ---: |
| Active survival | 10 per completed second, capped at 30 minutes |
| Common enemy killed by player damage | 10 |
| Elite | 250 |
| Boss | 5,000 |
| Completed Blood Shrine, Ember Circle or character/Twin Soul trial | 500 |
| Victory | 5,000 |

`Final score = round(Base points × (1 + 0.08 × total modifier ranks))`.

Each map starts with rank 0 available on all five modifier tracks. A victory opens the next rank above the rank used in that run, independently for each track, up to rank 5. Repeating an easier rank cannot advance beyond that next rank. Defeats and abandoned runs do not unlock difficulty. Unlocks persist in the meta profile and retry if saving fails. Tester balance overrides still allow difficulty completion while excluding leaderboard submissions.

The five challenge tracks are snapshotted at run start. Each has 0–5 ranks; the multiplier ranges from 1.00 to 3.00. Integer hundredths and nearest-integer rounding keep scores consistent across platforms. Boss summons contribute damage statistics but cannot farm kill points. Objectives count once per instance. Unsupported/practice maps and runs with tester balance overrides remain unranked; their score is still displayed.

Example: 125.7 seconds, 100 common kills, two elites, one boss, three objectives and victory produce 14,250 base points. Three modifier ranks give ×1.24, for **17,670**.

## Leaderboards and Steam handoff

The main-menu top-right panel shows the ten highest local runs, with Grove/Citadel/Marsh tabs and the selected map's personal best underneath. Unused ranks are empty. Only Fallen Grove is currently playable; the other two maps retain their existing coming-soon status. No global players or scores are fabricated.

Local data uses its own `HeavensDivide_Leaderboards_v1` save, separate from permanent progression. Each map keeps ten entries. Scores are sorted descending, with stable tie ordering. Duplicate run IDs and inconsistent totals are rejected. Failed saves retry, and an unreadable/newer save is preserved. `-LeaderboardSaveSlot=...` isolates tests; `-RunReportPreview` disables writes.

`USteamLeaderboardSubsystem` connects ranked results to Steam. `RunReports::Maps()` owns stable map IDs and versioned Steam board names:

| Map | Steam board name |
| --- | --- |
| Fallen Grove | `bloodshift_fallen_grove_score_v1` |
| Cinder Citadel | `bloodshift_cinder_citadel_score_v1` |
| Hollow Marsh | `bloodshift_hollow_marsh_score_v1` |

Configure each Steam board as descending, numeric, with unrestricted reads and client writes (not Trusted). The game finds the existing boards asynchronously, caches handles, uploads with KeepBest and downloads ranks 1–10 plus the current user's best/rank. Steam's global top ten contains **players**; the fallback local panel contains **runs**. The source label is omitted from the player-facing panel. Names truncate visually with full-name tooltips. Uploaded details are score version, completed duration, normal/elite/boss kills, objectives, total modifier ranks and victory. Start new versioned boards if the score formula changes materially.

OnlineSubsystemSteam is enabled for Windows using App ID **5283180**, from the project's Steamworks screenshots. Shipping builds also define `UE_PROJECT_STEAMSHIPPINGID=5283180` in the game target. OSS owns Steam initialization and callback pumping; leaderboard call results are polled on the game thread. No game server or multiplayer net-driver configuration is needed. No publisher Web API key is used. [Steam leaderboard setup](https://partner.steamgames.com/doc/features/leaderboards/guide), [Steam leaderboard overview](https://partner.steamgames.com/doc/features/leaderboards).

Each ranked run completion submits independently of the device's local personal best, so another account's higher local score cannot suppress the current user's Steam score. Pending scores coalesce by Steam user and map, save to `HeavensDivide_SteamQueue_v1`, and retry after connection/request failures. A failed save is retried too. Uploads are serialized and spaced by 65 seconds, below Steam's ten-per-ten-minute limit. Pending scores are removed only after a successful Steam acknowledgment. Account changes clear displayed global data, and upload only that account's queue. Historical device scores are never automatically attributed to the currently signed-in Steam account. If Steam has no usable account identity at completion, the run is saved locally only. Preview/automation runs never load or write the real Steam queue. [Steam upload API](https://partner.steamgames.com/doc/api/ISteamUserStats#UploadLeaderboardScore).

For verification through Steam: create the three exact board names above, package the Windows game, upload to depot **5283182**, set the build live on the desired branch, and launch from Steam on an account that owns the app. Finish a ranked run with tester overrides disabled. Check that the panel changes to STEAM and displays the submitted best/rank, then repeat on a second account. Editor previews intentionally use LOCAL. `-DisableSteamLeaderboards` disables the connection for isolated tests. This does not release the store page.

## Verification

`HeavensDivide.Progression.RunReports` checks real accepted damage, nested attribution, blocked/overkill/self-damage exclusions, summon scoring, one-shot objective completion, modifier snapshots, score math, top-ten trimming, duplicate rejection, independent maps and save serialization. It also renders shipped menus and all report tabs at multiple viewport sizes for review under `Saved/RunReports/Previews`.

The Development Editor build and all nine targeted checks passed: RunReports, PauseMenu, TrialChoiceLayout, MenuFeedback, RunCurrencyRewards, RunChallenges, RunPreparation, BarrageBuilds and TagTeamRegression. The final renders were reviewed at 1600×900, 1280×720 and 960×540. Tests used isolated saves. Evidence is in `Saved/RunReports/ValidationSummary.json`, `Build.log`, `Automation/index.json` and `Previews`. No packaged build or full-run score-balance playtest was performed.

## Damage attribution and presentation fixes

Great Shuriken contacts/pulses now record at `ShurikenHit`, and Returning Fang hits record at `FangHit`. The newer weapon hit paths previously fell back to the base Ninja kunai category. Breaking Wheel, Fang Resonance and Fang Splinter receive their own source IDs.

Tag Team uses the actual enemy health removed by the attack it requests. Its source is captured before spawning kunai, returning weapons, shurikens, Iaijutsu slashes and Crescent waves, including split waves and delayed chain slashes. Cleanup or a later assist cannot change an in-flight attack's source. Route-specific assist upgrades retain their own trigger source, and Shadow Clone remains separate. Passive buffs stay included in their attack totals; damage is never added twice.

Results and leaderboards omit the implementation footnotes, sidebar instructions, local-save caption and per-entry defeat/victory suffix. Score components, modifiers, damage totals, ranks and gameplay descriptions remain available.

Combat/menu/audio fixes: the Development Editor build and all 14 focused checks passed. Evidence is in `Saved/MenuCombatAudio/ValidationSummary.json`; reports retain the initial failed checks and their successful reruns. Updated result, leaderboard and audio-settings renders are in `Saved/RunReports/Previews`. No packaged build or full live-run/audio audition was performed.

Steam/Barrage integration: the Development Editor and Windows Shipping builds passed, and all eight focused checks passed (SteamScoreQueue, RunReports, BarrageBuilds, MenuBugRegressions, NinjaBuilds, FangBuilds, ShurikenBuilds and TagTeamRegression). The saved projectile test verifies frontward release, actual travel-driven beam growth, immediate gameplay shutdown, midpoint/zero opacity and cleanup. The project-owned Niagara copy compiled with 12 fading emitters and four travel-clamped beams; the source effect is preserved. The local menu render was reviewed. Evidence is in `Saved/SteamBarrage/ValidationSummary.json` and its build, asset and automation logs. The automation report retains existing material/fixture warnings. No full package/cook, live Steam score upload or full gameplay VFX playtest was performed; verify those using the packaging steps above.
