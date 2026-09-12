# Twin Soul skill tree

Open **Main Menu > Skill Tree**. Select a node to inspect its effect, prerequisites and next-rank cost, then select **Learn**. All effects are permanent passives applied in every subsequent run. The circular node map supports mouse selection, scroll-wheel zoom, drag-to-pan, arrow/D-pad navigation, and a Reset View button (Home also resets). Enter/controller A on an affordable node focuses its Learn button; Tab moves between the map and action buttons; Escape/controller Back closes the tree. Refund All asks for a second click and returns the exact paid cost.

## Node-map presentation

Four paths fan outward from a central Twin Soul seal. Every passive is a circular node with a vector symbol for its effect; outer capstones have a second ring. Small pips show purchased ranks. Learned nodes fill ivory, open paths have colored rings, and locked nodes are dim. Selection adds an outer halo and traces the selected skill's prerequisite path. Hover previews only the node ring and tooltip; it never overrides the selected connection highlight. Cross-path prerequisites use curved connections; long capstone bridges appear when their ancestry is selected to avoid obscuring other branches.

The left inspector shows the selected skill, rank, effect, prerequisites and purchase action. Hovering a node reveals its name and tooltip; mouse clicks or directional navigation select it. Existing costs, passive behavior, saves and refunds are unchanged by the visual redesign.

The map is native Slate vector drawing, so circles, connectors and symbols stay sharp when scaled or zoomed. `SMetaSkillMap.cpp` owns node positions, symbols, rendering and map interaction; `MetaSkillTreeWidget.cpp` owns the inspector and menu actions.

## Progression

- 32 nodes, 80 ranks, four paths: Shared Roots, Way of Steel, Way of Shadow and Twin Soul Bond.
- One rank in every prerequisite opens its child; maxing a parent is optional. All nodes can eventually be learned.
- Each completed 30 seconds of run time awards one Soul Ember at defeat or victory. Victory adds 20. A 10-minute defeat earns 20; a 15-minute victory earns 50. Quitting a live run does not bank a reward.
- Rewards use the existing run clock, including boss-arena travel. Existing terminal run guards prevent repeated death/victory callbacks from paying twice. Invalid time is rejected; survival rewards cap at 120 Embers, plus the victory bonus.
- Tier base prices are 5 / 10 / 15 / 20. A rank costs base price times the new rank. Tiers 1-3 have three ranks per node; the last tier has one. The full tree costs 1,600 Embers.
- Purchase and refund APIs reject gameplay controllers, including post-defeat screens: return to the main menu to change progression.
- Existing family starters, combinable branches, scalable run cards, mastery, and synergy discovery remain separate. This pass adds passives rather than retroactively locking available content.

## Nodes

Values below are per rank unless the node has one rank. Character direct damage uses the existing character-stat pipeline; Bleed and Poison receive their own listed bonuses. Basic melee area and basic projectile bonuses do not expand automatic family abilities.

### Shared Roots

| Node | Effect | Ranks | Prerequisite (one rank each) |
| --- | --- | --- | --- |
| Living Flame | +4% maximum health per rank. | 3 | None |
| Ember Reach | +8% XP pickup radius per rank. | 3 | None |
| Inner Fire | +3% damage for both characters per rank. | 3 | Living Flame |
| Pilgrim's Step | +2% movement speed per rank. | 3 | Ember Reach |
| Steady Breath | +2% basic attack speed for both characters per rank. | 3 | Inner Fire |
| Deep Reserves | +5% maximum health per rank. | 3 | Pilgrim's Step |
| Second Wind | +1 maximum dash charge. | 1 | Steady Breath |
| Far Horizon | +20% XP pickup radius. | 1 | Deep Reserves |

### Way of Steel

| Node | Effect | Ranks | Prerequisite (one rank each) |
| --- | --- | --- | --- |
| Tempered Edge | +4% Samurai direct damage per rank. | 3 | Living Flame |
| Crimson Oath | +5% Bleed damage per rank. Does not grant Bleed. | 3 | Living Flame |
| Sweeping Steel | +5% Samurai basic melee area scale per rank. | 3 | Tempered Edge |
| Draw and Cut | +3% Samurai basic attack speed per rank. | 3 | Crimson Oath |
| Mountain Splitter | +4% Samurai direct damage per rank. | 3 | Sweeping Steel |
| Unclosing Wounds | +5% Bleed damage per rank. | 3 | Draw and Cut |
| Heaven's Arc | +15% Samurai basic melee area scale. | 1 | Mountain Splitter |
| Perfect Form | +8% Samurai basic attack speed. | 1 | Unclosing Wounds |

### Way of Shadow

| Node | Effect | Ranks | Prerequisite (one rank each) |
| --- | --- | --- | --- |
| Hidden Edge | +4% Ninja direct damage per rank. | 3 | Ember Reach |
| Jade Venom | +5% Poison damage per rank. Does not grant Poison. | 3 | Ember Reach |
| Silent Flight | +6% Ninja basic projectile speed per rank. | 3 | Hidden Edge |
| Flickering Hands | +3% Ninja basic attack speed per rank. | 3 | Jade Venom |
| Assassin's Patience | +4% Ninja direct damage per rank. | 3 | Silent Flight |
| Black Lotus | +5% Poison damage per rank. | 3 | Flickering Hands |
| Through the Veil | Ninja basic projectiles pierce one additional enemy. | 1 | Assassin's Patience |
| Twin Fangs | +1 Ninja basic projectile. | 1 | Black Lotus |

### Twin Soul Bond

| Node | Effect | Ranks | Prerequisite (one rank each) |
| --- | --- | --- | --- |
| Crossing Souls | +5% swap recharge speed per rank. | 3 | Inner Fire |
| Lingering Intent | Family synergy preparation lasts +0.5 seconds per rank. | 3 | Pilgrim's Step |
| Answered Challenge | +5% family partner-reaction damage per rank. | 3 | Crossing Souls |
| Seamless Relay | +5% swap recharge speed per rank. | 3 | Lingering Intent |
| Unbroken Promise | Family synergy preparation lasts +0.5 seconds per rank. | 3 | Answered Challenge |
| Converging Blades | +5% family partner-reaction damage per rank. | 3 | Seamless Relay |
| Two Souls, One Will | +8% damage for both characters. | 1 | Unbroken Promise, Mountain Splitter, Assassin's Patience |
| Heaven Undivided | +8% basic attack speed for both characters. | 1 | Converging Blades, Unclosing Wounds, Black Lotus |

## Stacking and runtime behavior

Bonuses with the same effect add across tree nodes. Shared and character stat bonuses use Add Flat on existing multiplier stats. Full investment gives +27% max health, +17% shared damage, +14% shared basic attack speed, +6% movement speed, +44% pickup radius and one extra dash charge. Each character receives +24% direct damage and +17% basic attack speed. Samurai gains +30% basic melee area scale; Ninja gains +18% basic projectile speed, one projectile and one pierce. Bleed and Poison each gain a separate 30% multiplier.

Swap cooldown is `base / (1 + swap bonus)`: a 3-second cooldown becomes approximately 2.31 seconds at +30% recharge speed. The timer and HUD use the same effective duration. Family synergy preparation lasts up to three additional seconds; partner-reaction damage gains a separate 30% multiplier. Same-character restrictions, victim re-prime lockouts and recursion guards are preserved.

Stats are rebuilt using stable `MetaSkill.<node ID>` modifier IDs. Modifier replacement is atomic and skips identical values, preventing temporary stat drops and dash refills during rebuilds. Passive effects are cached when progression changes; status ticks and family hits use constant-time lookups. No new per-frame gameplay work is added.

## Saving and maintenance

The existing `HeavensDivide_MetaProgression` save advances to version 3. Old synergy unlocks and Twin Soul discovery progress survive migration; older saves begin with an empty tree and wallet. Node IDs are permanent save identifiers. Invalid ranks are clamped and unknown/orphaned nodes removed on load. Refunds use the recorded actual expenditure, independent of subsequent balance price edits.

Purchases and refunds revert their in-memory changes if saving fails. Failed run rewards remain pending in the game instance and retry when the main menu opens. They cannot survive exiting the application before a successful save. Reset Progress now explicitly includes skills and currency.

- `MetaSkillTree.cpp`: authored nodes, progression prices, prerequisites, reward calculation and pure model operations.
- `SynergyMetaProgressionSubsystem`: transactional purchases/refunds, wallet, migration and cached effects.
- `MetaSkillTreeWidget`: native Slate graph, details, purchases and refunds, opened by `MainMenuWidget`.
- `PlayerUpgradeComponent`: shared and character passive modifiers.
- `SurvivorPlayerController`, `EnemyStatusEffectComponent`, `SurvivorBuildFamilies`: rewards, swap timing, statuses and reactions.
- `MetaSkillTreeTests.cpp`: catalog, disk persistence, rollback, reward retry, migration, runtime stats, status/reaction damage and tree construction. Tests use disposable `Automation_` save slots, never the player's progression slot.

This is an initial balance pass. Assess progression speed and combat difficulty in playtests before changing the node prices or magnitudes.

## Validation

Win64 Development Editor build succeeds. All ten `HeavensDivide` automation suites pass with NullRHI (seven clean, three with fixture warnings). The skill-tree suite also passes with D3D12 rendering enabled and exports `Saved/MetaSkillTree.png` for layout inspection.

Reports: `Saved/Automation/MetaSkillTreeHeadless/index.json` and `Saved/Automation/MetaSkillTreePreview/index.json`. The broad D3D12 run exposed a pre-existing transient Niagara-script assertion in `Abilities.EditorTuning`; that same suite passes headlessly. The new skill-tree suite passes under both render modes. Existing missing-material startup diagnostics are unrelated to this pass.

Node-map visual pass: the Win64 editor build and rendered `HeavensDivide.Meta.SkillTree` check pass. Highlight regression coverage includes hovering another branch, directional navigation, view reset and capstones with prerequisites across multiple paths.

Latest UI report: `Saved/Automation/MetaSkillNodeMap/index.json`; rendered preview: `Saved/MetaSkillTree.png`.

## Menu visual style

Collection, Skill Tree, Settings, and Reset Progress use the corrected Ascension panel through the main menu's **Shared Secondary Page Style → Page Background Texture** setting. The skill tree reads this setting from its menu owner. `Tools/configure_shared_menu_background.py` updates the authored Blueprint and resets legacy background scale/offset compensation.

The skill tree uses the main menu's Cinzel Medium and Regular fonts, ivory lettering, InkHover brush buttons, and MenuBrushStroke separators. Its generated charcoal ink-wash panel has a distressed ivory frame and a quiet center for readable connections. Subtle branch colors distinguish paths against neutral ink nodes.

The source artwork and generation prompt are in `Art/SkillTree/`. `Tools/import_skill_tree_art.py` imports the panel into `/Game/HeavensDivide/Blueprints/UI/SkillTree/AscensionPanel` with UI texture settings. Native reflected brushes and font properties retain the assets for garbage collection and cooking.

Style validation report: `Saved/Automation/MetaMenuStyle/index.json`; rendered preview: `Saved/MetaSkillTree.png`.

The skill tree is embedded in the main menu overlay, reserving the same 410-unit navigation column as Collection. It fills the remaining page area and scales its 1440 x 920 layout to fit, without the former 1030 x 780 size cap. Opening Collection, Settings, reset confirmation, or returning to the main menu hides it; it no longer creates a separate full-screen viewport widget.

Secondary menu pages no longer add corner brush overlays; the shared background supplies the frame.
