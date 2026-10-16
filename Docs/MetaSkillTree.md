# Twin Soul skill tree

Open **Main Menu > Skill Tree**. Select a node to inspect its effect, scope, prerequisites and price; select **Learn** to buy a rank. One rank in a prerequisite opens its child. All skills can eventually be learned, and **Refund Skills** returns the exact amount paid after a second confirmation click.

## Layout and controls

The four pages are Stances, Synergies, Abilities and Stats. Stances are grouped under Samurai and Ninja. Locked stances show their Sigil price; selecting Unlock opens a Yes/No confirmation stating the exact cost. Opening the dialog or choosing No spends nothing. Yes uses the existing transactional purchase, then opens that stance's skill tree.

Abilities shows shared skills and three joint recipes together. Soul Convergence includes Crimson Tempest; Eclipse Severance and Lunar Tempest require that root and cost 18 Soul Embers each. Recipe unlocks do not grant passive damage stats. Their stable IDs are `Ability.Combo.[ID]`. Existing authored balance catalogs receive the current combo nodes at runtime while retaining unrelated saved tuning. See [Combo abilities](ComboAbilities.md).

Skill trees progress horizontally from left to right. Stance trees form one row; larger Stats and Abilities trees keep branches in separate rows, with vertical scrolling to preserve readable labels. The first skill is visible when the page opens. Rank markers sit below each symbol: learned ranks are solid gold, remaining ranks are dark with a clear light outline. Markers are 14 units across (previously 4.2), spaced 22 units apart, and can be clicked to select their skill.

The wheel or scrollbar scrolls extra branches; zoom and drag panning are disabled. Arrows or D-pad move selection and scroll it into view. Enter/controller A focuses Learn. Home scrolls to the top. Escape/controller Back closes the page or cancels a pending purchase/refund. The selected prerequisite path is gold; hovering another branch does not change it. Horizontal connectors meet node rings and leave names and rank markers unobstructed. Stats order Samurai, shared and Ninja branch rows with a color legend. Hover tooltips contain only the description in a dark, gold-framed box; the Learn button and purchase-block reason remain below the tree.

## Progression and costs

- 40 skills and 92 purchasable ranks; full completion costs **1,980 Soul Embers**.
- Shared skills cost 560 total, each hero's general trunk costs 200, and each of the six stance branches costs 170.
- Rank price is base price multiplied by the new rank. Base prices are 5, 10, 15 or 20 Embers. Most skills have three ranks; capstones and draft tools have one.
- Run rewards are unchanged: one Ember per completed 30 seconds, plus 20 for victory. Survival rewards cap at 120. Quitting a live run does not bank a reward.
- Purchases and refunds are available from the menu, outside gameplay.
- These are permanent stat upgrades purchased with Soul Embers. Blood and Barrage are the starting stances. Additional stances cost 18 Soul Sigils each. Starting synergies are selected from owned unlocks. Supported ordinary run-upgrade cards remain available without discovery or trial unlocks. In-run stance prerequisites, exclusive branches and rank limits still apply. Technique and Twin Soul trials are retired.

The general character trunk requires Living Flame, then Tempered/Hidden Edge, then the attack-speed node, then Mountain Splitter/Assassin's Patience. The three stance paths open from that last damage node. There are no dependencies on purchasing the other hero's skills.

## Shared and general skills

Full shared investment grants +27% health, +17% shared damage, +14% shared basic attack speed, +6% movement speed, +88% XP pickup radius, one dash charge, +30% swap recharge speed, three rerolls and two banishes per run. Reroll and Banish use the existing level-up tools.

Each hero's general trunk adds +24% character direct damage and +17% character basic attack speed. The existing runtime combines shared and character multipliers; these percentages are separate stat layers, not one sum. The tree no longer grants universal Poison, piercing or projectile-count bonuses.

## Stance branches

Every stance path begins with +3% direct damage per rank (three ranks). The next skill has three ranks; the final skill has one.

| Stance | Second skill, per rank | Final skill |
| --- | --- | --- |
| Blood | +4% melee and blood-burst radius | +20% Bleed damage |
| Iaijutsu | +4% slash width | +12% charge speed |
| Crescent | +4% wave and lingering-field width | +12% basic attack speed |
| Returning Fang | +4% targeting range | +12% travel speed |
| Barrage | +3% basic attack speed | +1 projectile per volley |
| Great Shuriken | +4% blade radius | +12% contact and pulse frequency |

A stance skill applies only when that hero has the matching stance in the current build. The inactive assisting hero keeps its own stance bonuses. Buying these skills does not choose or unlock a stance. The inspector states the requirement explicitly.

These effects reach actual attacks: Iaijutsu uses the charge-time calculation; Fang uses targeting range and projectile travel speed; Great Shuriken uses spawned collision radius, growth's initial radius, and contact/pulse intervals. Faster charging or damage frequency divides the interval by (1 + bonus), keeping wording and behavior consistent.

## Existing saves

The earlier save-version **5** migration performed a one-time full respec for earlier profiles because effects and prerequisite paths changed. All previously spent skill Embers are returned using the recorded historical spend, existing unspent Embers remain, and skill ranks are cleared. Legacy discovery history is preserved for save compatibility but no longer restricts availability. Reloading cannot grant the refund twice. The migration takes place when the game next loads the profile; the configuration script does not open player saves.

Purchases and refunds roll back if saving fails. Failed run rewards remain pending in the game instance for retry on returning to the menu. Invalid ranks and orphaned nodes are sanitized. Stable modifier IDs prevent repeated stat rebuilds from stacking bonuses or refilling health/dashes. Stance effects are cleared when the build changes.

## Editing and maintenance

The shipped catalog lives in **/Game/HeavensDivide/Balance/DA_GlobalBalance**, under Skills. Each node exposes Map Position, Required Stance and Passive Effect in addition to its existing price, ranks, magnitude and prerequisites. The horizontal layout derives columns and branch spacing from prerequisites, fitting width to the page and keeping extra rows scrollable. Node IDs are persistent save identifiers; changing the layout does not change purchases or dependencies.

After compiling changed native defaults, run **Tools/configure_meta_stance_tree.py** through Unreal Python to install the default catalog. This backs up the asset and changes only Skills, preserving the other balance settings. Do not rerun it over intentional designer edits unless restoring the native catalog is desired.

- MetaSkillTree.cpp: default catalog, descriptions, scope labels, prices, prerequisites, migration and model operations.
- SMetaSkillMap.cpp: vector rendering, prerequisite layout and mouse/controller navigation.
- MetaSkillTreeWidget.cpp: inspector, purchases, refund and menu actions.
- SynergyMetaProgressionSubsystem: transactional saves and cached generic/stance passives.
- PlayerUpgradeStats.cpp: scoped modifiers, runtime bonus lookup and BalanceLab profile previews.
- IaijutsuBuild.h, AutoAttackComponent.cpp, NinjaBuildComponent.cpp and ShurikenBuild.cpp: stance mechanic hooks.

The map retains the existing native menu panel, fonts and buttons. It scales to fit the skill page beside the main-menu navigation. No generated textures are needed for the new layout.

## Verification

Automation coverage includes catalog costs and prerequisites, persistence and failure rollback, exact one-time refunds, node positions and width-fit hit tests, wheel bubbling and disabled drag panning, selection highlighting, all six stance effects, actual charge/range/shuriken behavior, inactive-assistant scope, and removal of stale bonuses after changing builds. Tests use disposable automation saves or BalanceLab preview profiles.

The Win64 Development Editor build passes. All 15 selected gameplay/progression suites pass across the initial run and corrected-fixture rerun; the combined result is Saved/MetaTreeRedesign/GameplayVerification.json. These cover the meta tree, all six combat builds, Tag Team, balance data, run preparation, and draft tools. Five of the existing suites report fixture/asset warnings.

D3D12-rendered SkillTree and DraftUnlocks suites also pass (Saved/MetaTreeRedesign/Rendered/index.json). Tree previews cover 1920 x 1080, 1280 x 720, and the 1444 x 976 menu page bounds, including long stance descriptions. Saved/MetaSkillTree.png is the overview; Saved/MetaTreeRedesign contains the stance previews.

The larger tree extends progression length. Its final difficulty and Ember pacing still need playtesting against real run outcomes.

The subsequent upgrade-availability pass also passes 13 selected checks (`Saved/TrialRemoval/Verification.json`), including the meta-tree suites. Final rendered layout inspection is recorded in `Saved/MetaTreeRedesign/FinalVisual/index.json`. All supported run upgrades are available independently of permanent skill purchases; Technique and Twin Soul trials are retired.

The Loadout and difficulty-unlock pass builds successfully and passes all nine selected menu/progression checks. Rendered previews include locked stance dropdowns, Samurai/Ninja categories, the Yes/No purchase dialog, and the larger vertical trees. Evidence is `Saved/LoadoutMenus/ValidationSummary.json`; automation uses disposable profiles.

Horizontal-layout update, 2026-10-16: Development Editor build succeeded and all 12 focused UI/combat checks passed (`Saved/FeedbackPolish/Validation.json`). Geometry checks cover every page's left-to-right dependencies, ring-to-ring connectors, separate labels/branches, hit targets including rank dots, and keyboard/controller navigation. Rendered Stats, Abilities and stance pages were inspected at 720p and 1080p; partial and complete ranks are visible in `Saved/MenuTreePolish/HorizontalRanks1280.png`. Tooltip borders passed pixel checks at 100% and 125% scaling. Validation used isolated automation profiles.

The menu/tree polish pass also builds and passes eleven focused checks, including continuous ring endpoints, clear label routing, all page layouts, Samurai/shared/Ninja ordering, combo assist charge and Display controls. D3D12 previews were inspected; evidence is `Saved/MenuTreePolish/ValidationSummary.json`.

The current Abilities page shows the shared root and three joint recipes together. Crimson Tempest is included with Soul Convergence; the other two recipes cost 18 Soul Embers each. Solo choices migrate/refund in save version 7; see [Combo abilities](ComboAbilities.md).
