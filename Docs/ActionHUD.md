# Dash and swap HUD

Smaller circular badges flank the combo meter during a run. Both use the combo's ivory/gold palette, Cinzel key badges and clockwise progress rings.

- **Dash:** chevrons identify the action. The count and small gold pips show available charges; the ring fills as the next charge returns. A remaining charge stays available while another recharges.
- **Swap:** the existing arrow icon sits inside the ring, with seconds remaining during cooldown. The ring fills toward readiness and turns gold when swapping is available.
- Both show LOCKED and dim during a combo. Action readiness uses a steady glow; the full combo retains its subtle pulse.

Keys follow Settings rebinding. Edit **WBP_PlayerHUD > Player HUD > Actions** for badge positions, size and the swap icon. Colors and font share **Player HUD > Combo**. The native badges use live controller state in `PlayerActionHUD.cpp` and share the drawing code in `ComboRadialWidget.cpp`. Legacy Blueprint widgets remain hidden so old event bindings continue to resolve.

`HeavensDivide.UI.ActionHUD` checks multiple dash charges, partial recharge, swap cooldown and combo locks, and exports 720p/1080p previews into `Saved/CombatPresentation/Previews`.

Editor build and focused validation passed; see `Saved/CombatPresentation/ValidationSummary.json` for results and reviewed previews.
