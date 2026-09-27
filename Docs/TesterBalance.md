# Tester balance page

Open **TESTER BALANCE** at the bottom right of the main menu. Enable tester overrides, adjust the values, and choose **SAVE & BACK** before starting a new run. **BACK** or Escape discards unsaved edits. **RESET** restores the draft to disabled/default values; save to persist that reset. **COPY SETTINGS** copies the draft as a readable report for playtest feedback.

Settings persist locally in `Saved/Config/<platform>/TesterBalance.ini`. Authored Blueprints and maps are unchanged. Saved overrides remain enabled between launches until disabled or reset. The button is available in packaged builds too so testers need no console or editor.

- Each of the 14 current enemy types has independent health (0.1–10x) and movement speed (0.1–3x) multipliers. These compose with survival health growth and bloodbound modifiers. Boss and objective health assignments also respect the health multiplier. Speed affects ordinary movement, not separately authored dash/charge speeds or attack animation timing.
- **Maximum alive** sets the survival director's phase and absolute cap (1–500). Zero keeps authored phase caps. This is a ceiling, not a target population; raise population density if more enemies are wanted. Trial/boss summons are outside the director's count.
- **Population density** scales phase targets and per-type caps. With maximum alive at zero, it also scales phase/global caps, limited to 500.
- **Spawn interval** scales normal/accelerated/emergency refill intervals. Lower is faster. It does not change special-event timing or threat replacement cooldowns.
- **Health growth** scales each roster entry's health growth per minute. Zero disables that growth.
- **Disable pressure events** disables the director's scheduled events for all phases.

The current enemy roster is defined in `UTesterBalanceSettings::Roster`; add future enemy class paths there to show them in the panel. Overrides match exact generated class paths. Runtime hooks apply once at enemy/spawner BeginPlay, so normal gameplay does not accumulate multipliers each tick. No new art assets are required; the menu button uses the existing ink style, and the page uses the menu font and dark/ivory palette.
