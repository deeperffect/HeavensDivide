# Dash and swap HUD

The bottom-center action row groups dash charges to the left of the combo meter and swap to its right. Captions use the menu's Cinzel typeface and reflect the keyboard bindings in Settings.

- **Dash:** each available charge is an ivory brush stroke. A recharging stroke fills horizontally in gold; spent slots retain a muted ink silhouette. Additional charges extend the row leftward.
- **Swap:** ivory brush arrows form a circular exchange symbol, gold when ready and muted while cooling down. The existing numeric cooldown remains in the center.
- Both indicators dim during the combo ability, when these actions are blocked.

The existing Blueprint events still drive charge counts, refill progress, and cooldown text. The style and layout are saved in `WBP_DashCharge`, `WBP_SwapCooldown`, and `WBP_PlayerHUD`. `PlayerActionHUD.cpp` updates state colors, dash caption placement, and rebound key labels.

Reapply the asset styling with `Tools/style_action_hud.py` through Unreal Python. Source artwork and the generation prompt are in [Art/ActionHUD](../Art/ActionHUD/README.md).

Run `HeavensDivide.UI.ActionHUD` with `-ActionHUDScreenshots` and a rendering backend to verify the shipped widget assets and export full HUD previews to `Saved/ActionHUD`.
