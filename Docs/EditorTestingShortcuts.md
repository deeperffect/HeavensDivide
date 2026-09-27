# Editor testing shortcuts

During Play In Editor, focus the game viewport and use the top-row number keys:

| Key | Action |
| --- | --- |
| 1 | Gain the XP needed for exactly one level and open the normal upgrade selection. |
| 2 | Fill the shared character ability meter. |
| 3 | Double both characters' movement speed. Repeated presses stack: 2x, 4x, 8x. |
| 4 | Deal 100 damage to the player through the normal health system; this can kill the player. |

Shortcuts require a living player in an active run and are blocked while paused or choosing an upgrade. The speed multiplier survives character swaps and stat updates, but is temporary controller state and is not saved. Stop Play In Editor to reset it.

Bindings, handlers, and the speed multiplier are compiled only with `WITH_EDITOR` and additionally require a PIE world. They are unavailable in standalone games and all packaged builds, including Development builds. They are not exposed in player keybinding settings.
