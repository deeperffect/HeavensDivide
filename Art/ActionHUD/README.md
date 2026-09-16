# Action HUD artwork

`SwapInkArrows.png` is a transparent ivory dry-brush swap emblem generated with the built-in imagegen tool. The original alpha is preserved. It is imported as `/Game/HeavensDivide/Blueprints/UI/Swap/T_SwapInkArrows`.

Dash charges reuse `Art/ComboMeter/ComboInkSlash.png` so the action indicators share the same painted texture. `Tools/style_action_hud.py` imports the swap image and styles the existing dash, swap, and player HUD Blueprints. Original widget assets are backed up once under `Saved/Backups/ActionHUD`.

## Final generation prompt

Use case: stylized-concept. Production game HUD icon, a single swap symbol made of TWO opposing curved brush arrows forming an open circular cycle. Japanese sumi-e dry-brush ink style, warm ivory-white monochrome pigment, bold simple silhouette readable at 48 pixels, tapered bristle tips, tiny restrained organic wear. Square canvas, centered icon fills 85 percent of canvas with generous interior negative space. Both arrows clearly indicate clockwise exchange, balanced top-right and bottom-left arrowheads. Match a refined dark samurai fantasy UI with ivory brush strokes on black and antique gold tint applied later by game. Genuine transparent PNG alpha background. No lettering, no border, no solid disc, no backdrop, no glow, no shadow, no photograph, no extra motifs. Neutral ivory texture only; isolated reusable icon, not a UI screenshot.
