# Bullet-heaven feature audit — September 29, 2026

This is a source/content inventory and design comparison, not a full-run playtest. Native implementation, project configuration, current asset filenames and existing implementation documentation were inspected. Saved Blueprint overrides, level geometry, moment-to-moment balance, visual readability and performance were not freshly verified in Unreal. “Not found” means not found in those inspected surfaces, not proof that no experimental Blueprint implementation exists. No gameplay changes were made.

## Assessment

Heavens Divide has a substantial combat and progression foundation. Its clearest opportunity is to make choosing, understanding and replaying runs as compelling as performing attacks. The Samurai/Ninja swap system is a differentiator worth developing; matching another game's character or weapon count is not a prerequisite.

Current content includes 57 upgrade asset files: 22 Samurai, 24 Ninja, six global and five synergy. That count is an inventory, not a fresh verification that every card is offered in a live run. Existing code supports prerequisites, mutually exclusive stances, category weighting, investment mastery and upgrade roles including Evolution. Previous automatic ability families were deliberately retired; their remaining catalog entries must not be counted as playable content.

Already implemented: auto-attacks, active abilities, dash, character swapping, inactive-character support, bleeding/poison, branching weapons, timed objectives, character trials, Twin Soul discovery, elite reward chests, minimap infrastructure, phased population pressure and rushes, boss-arena travel, a multipattern final boss, defeat/victory screens, persistent skill progression and collection browsing. The skill-tree documentation describes 28 nodes/68 ranks with refunds. Menus already support keybindings, auto-targeting, master volume and camera-shake intensity.

## Comparison basis

- [Vampire Survivors](https://store.steampowered.com/app/1794680/Vampire_Survivors/) emphasizes snowballing weapon choices and carrying progression between runs; its store also documents multiple input methods and refundable power-ups.
- [Soulstone Survivors' official site](https://soulstonesurvivors.com/) emphasizes build combinations, character/weapon progression, bosses, curses, maps and multiple modes.
- [Brotato](https://store.steampowered.com/app/1942280/Brotato/) demonstrates another successful structure: short waves, between-wave purchases, distinct starting identities, and adjustable enemy health/damage/speed.

These are examples of genre design approaches, not causal evidence that copying a feature makes a game successful. A shop, six simultaneous weapons or co-op is optional. Heavens Divide can deliver build agency and replay goals through its own trials and dual-character system.

## Recommended priorities

### 1. Turn the existing maps into player-facing run choices

**Confirmed gap:** `MainMenuWidget.cpp::HandleNewRun`, `GameOverWidget.cpp::HandleRestartRun` and `VictoryWidget.cpp::HandleNewRun` directly open `/Game/Maps/Lvl_B1_Lvl1`. The content folder contains the other B1 variants, an arena and a B2 map, but file presence does not establish a complete playable route. Existing travel is a configured boss-gate destination, not a general map-selection or campaign system.

Add a small run-setup screen with map identity, threat preview, expected structure and completion record. Preserve the chosen map on restart. After the first clear, unlock a few difficulty modifiers that change encounters and rewards, rather than only increasing health. Keep map selection separate from any later campaign sequencing decision.

### 2. Make builds inspectable and runs explainable

**Confirmed gap in current native menus:** Pause provides Resume, Settings and Main Menu. Defeat presents time survived and Soul Embers; victory presents Soul Embers. Neither is a detailed run report.

Add a pause build page showing acquired cards/ranks, both characters' effective stats, stance, status effects and prerequisites still needed. Add a terminal report with kills, level, damage taken, last damaging source, rewards, completed objectives, and damage by meaningful source. Attribute bleed/poison, clones and chained effects correctly; simply counting a projectile hit is insufficient. Separate damage actually removed from enemy health from overkill so statistics do not mislead.

This is valuable to players and also supplies the measurements needed for balancing.

### 3. Add limited control over upgrade randomness

**Not found:** player-facing reroll, banish or skip controls. `PlayerUpgradeSelection.cpp::RollUpgradeChoices` already filters eligibility and offers role diversity; category weighting also exists. This is not an entirely uncontrolled draft.

Start with a small reroll allowance and one deliberate way to remove an unwanted card for the current run. Preserve trial guarantees, stance restrictions and meta-unlock rules. Explain prerequisites on cards and distinguish “not unlocked,” “not eligible” and “maxed.” Do not silently present an upgrade that cannot benefit the current build.

### 4. Teach the game's distinctive decisions

**Not found:** a dedicated first-run onboarding flow. Existing controls, prompts and trial instructions are useful, but do not by themselves establish that a new player understands the overall run.

Teach movement/pickups, dash, swapping, ability charge and the first objective in short contextual steps. Explain why investing in both characters or specializing changes a run. Offer skip/replay. Show the next meaningful objective and what success rewards, without turning play into a permanent tutorial overlay.

### 5. Expand specific replay goals and unlock information

**Partial foundation exists:** Soul Embers, persistent skill ranks, refunds, synergy unlocks and Twin Soul discovery. The save class stores these; it does not currently hold a broad challenge/completion history. Locked collection entries use the generic text “Find this upgrade during a run to unlock it.”

Add visible objectives such as a first map clear, a stance-specific clear or a trial challenge. Prefer rewards that open choices—new encounter modifiers, variants or sidegrades—alongside permanent stats. Track progress and reveal actionable unlock hints. Build on the existing discovery system rather than replacing it.

### 6. Strengthen late-run combinations and encounter identities

**Design/playtest question, not a proven missing mechanic:** the game already has branching builds, statuses, elites and a boss with multiple attacks and phase transition. The important question is whether different builds produce noticeably different decisions and satisfying late-run payoffs.

Prioritize a small set of tested Samurai/Ninja combinations and clear payoff moments. Develop threats that ask for distinct responses—repositioning, target priority, timing, or a swap—rather than relying only on higher HP and faster pursuit. Map-specific encounter schedules already exist; verify that players can feel the difference. Do not restore intentionally removed automatic families just to increase the content count.

### 7. Finish practical run and presentation options

- **Suspend/continue:** `RunTravelSubsystem.h` keeps a transient game-instance snapshot for level travel. That is not a disk-backed saved run. Add versioned suspend/resume if desired, with a clear policy for consuming resumed saves and preventing duplicate rewards.
- **Readability controls:** extend the existing shake slider with friendly-effect intensity/opacity, flashes, damage-number options and UI scale. Keep enemy attack warnings readable independently of friendly effects.
- **Settings:** native settings currently expose master volume, shake, auto-targeting and keybindings. Separate music/SFX/UI levels and player-facing display/quality options would round this out. Inherited engine graphics settings are not the same as a usable menu.
- **Music and polish:** no dedicated music system was identified in inspected native code or clearly named assets. Verify actual level/Blueprint/media playback before treating music as absent.
- **Release layer:** localization, achievements/cloud integration and packaged save/recovery behavior require a separate release audit. Co-op and daily seeded runs are optional later scope, not immediate blockers.

## Work that needs playtesting rather than feature guessing

Do not declare the game balanced or performant from asset counts or automation tests. Record level and upgrade count at 3/6/9 minutes, first elite kill time, boss entry time, damage sources, pickup burden, offer quality, and whether each phase creates the intended pressure/recovery rhythm. Compare fresh profiles with progressed profiles and several plausible builds.

Measure frame time and effect readability during the busiest supported encounter, including clones, status procs, projectiles and chest orbs. The existing enemy caps, movement optimizations and effect throttles are useful safeguards; they do not establish a measured FPS target.

## Suggested implementation order

1. Run metrics plus pause/build and result screens.
2. Run setup using the existing maps, with restart preserving selection.
3. Limited reroll/banish controls and clearer prerequisites.
4. First-run onboarding and objective/reward explanations.
5. A small challenge/difficulty progression loop, tuned using the collected metrics.
6. Additional encounters, combination payoffs and quality-of-life options based on observed player friction.

This sequence uses existing content, helps expose balance problems, and gives players more reasons to start another run before expanding the roster substantially.
