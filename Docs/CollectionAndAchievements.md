# Collection and Steam achievements

The collection is now a lifetime journal of upgrades and stances actually acquired in runs. Retired and non-pool entries are excluded, and rank displays use the same effective cap as upgrade acquisition. It includes All, Samurai, Ninja, Synergies, Shared / Pacts and Achievements tabs, discovery/mastery filters, category completion counts, highest acquired rank, prerequisite hints and progress toward achievement goals. The collection has no search bar. Multi-rank upgrades are mastered by reaching their maximum rank in one run. Unlocking a stance or synergy in the skill tree makes it available; actually using it records discovery. No historical acquisitions are invented from existing purchase saves.

Design references: [Vampire Survivors' collection](https://vampire-survivors.fandom.com/wiki/Collection) uses an item catalogue and completion tracking; [Halls of Torment's quests](https://hot.fandom.com/wiki/Quest) connect explicit goals to progression. This implementation uses those broad patterns, with this game's own assets and menu styling. It does not copy their artwork or layouts.

## Steamworks setup still required

App ID: **5283180**. The matching 12 definitions are in [SteamAchievements.csv](SteamAchievements.csv). This is a setup/reference sheet, not a claim that Steamworks supports importing this particular CSV format.

1. In Steamworks App Admin, open Stats & Achievements / Achievements. Add each exact API name, display name and description from the CSV. Set By = Client, Hidden = false. No Steam stat dependencies are needed; the game evaluates the goals.
2. Supply the required unlocked/locked achievement icons and publish the configuration changes.
3. Run a packaged development build under Steam with this app ID. The game reads the user's achievement flags, merges existing Steam unlocks into the journal, and calls `SetAchievement` and `StoreStats` for earned local goals. A successful `UserStatsStored_t` callback confirms the submitted batch. Check the Steam overlay and relaunch to verify persistence.
4. Test an achievement earned offline on a known Steam account, reconnect, and verify synchronization. Also test a second Steam account and ensure the first account's local achievements are not awarded to it.

The installed Steam SDK is v1.64: current-user stats are synchronized before process launch; the removed `RequestCurrentStats` API is deliberately not called. The existing OnlineSubsystemSteam owns initialization and callback pumping. See [Steam's stats and achievements documentation](https://partner.steamgames.com/doc/features/achievements).

## Persistence and eligibility

- Storage: `HeavensDivide_Collection_v1_<SteamID>`; standalone play without a Steam identity uses `..._Local`. Local-only progress is not automatically transferred to a different Steam account. Cloud-earned achievement flags can be restored, but Steam does not contain individual upgrade ranks.
- Skill-tree refunds/reset do not erase lifetime discovery or earned achievements.
- Upgrade acquisitions and starting loadouts are recorded during eligible ranked runs. Run goals accumulate on victory/death completion; repeated Finish calls cannot award a run twice. Practice/unsupported maps and tester-balance overrides do not award achievements.
- The journal batches acquisition saves at two-second intervals and saves at run completion/shutdown. Failed saves remain dirty for retry. Unreadable/newer save files are preserved instead of overwritten.
- Steam sync retries every 30 seconds, does not mark pending writes confirmed until acknowledged, and reports unavailable/unconfigured states in the collection footer. The Steam button opens the achievement overlay when Steam's overlay is available, with the app's public Steam achievements page as a browser fallback.
- Editor, commandlet, NullRHI and `-DisableSteamAchievements` sessions use memory-only journals and cannot write to real profiles or unlock Steam achievements. This keeps automation/visual previews isolated.

## Menu changes

New Run is sequential: choose map/challenges, press **Next**, review loadout, then **Begin Run**. Back/Escape on the loadout returns to the map page without losing choices. The old top page-selection buttons are removed.

Upgrade cards show solid owned diamonds, a 50%-opacity offered diamond, and hollow future ranks. The separate **OWNED X / Y** text is removed. Skill-tree graphs use a dark description-only tooltip with an inset gold frame that remains complete at fractional DPI. The right inspector is removed from all graph pages; the Learn action and purchase-block reason remain in the footer.

Skill trees progress left to right, with larger gold filled dots for learned ranks and outlined empty dots for remaining ranks below each upgrade. Extra branches scroll vertically so densely populated pages retain readable symbols and labels. Stance trees fit on one horizontal row.

## Validation

`Tools/Run-CollectionCombatAutomation.ps1 -Render` runs the focused combat/progression/menu tests and saves results under `Saved/CollectionCombat/Automation`. New tests cover bleed radius boundaries, boss charge timing/contact/collision/trails, collection serialization and account isolation, and owned-versus-offered card ranks. Rendered collection, achievement and rank-card previews are saved under `Saved/CollectionCombat/Previews`; existing menu tests also render the New Run and skill-tree pages.

Live Steam unlock/overlay verification requires the unpublished achievement definitions and a Steam-launched game build; local automation does not substitute for that final backend check.


Verified 2026-10-16: Unreal Editor Development build succeeded. The full focused suite passed all 14 tests (`Saved/CollectionCombat/FullSuite.json`). After the final rank-label and Steam acknowledgement changes, all three affected tests passed (`FinalCardAndSteamChecks.json`); the final 150-entry collection render/filter test also passed (`FinalCollectionVisual.json`). Collection, achievements, skill tooltip, owned-rank cards and the two New Run steps were visually inspected. Live Steam backend verification remains pending the setup above.

Later UI/combat polish on 2026-10-16: the Editor build and all 12 selected checks passed (`Saved/FeedbackPolish/Validation.json`). Rendered previews confirm the complete tooltip frame, collection without search, upgrade cards without the OWNED label, horizontal skill trees and larger rank dots. Combat checks cover the saved 0.25-second blue echo, damage-number suppression for status ticks, larger initial critical pop and unchanged slash area scaling.
