# Combo meter artwork

`ComboInkSlash.png` was generated with the built-in imagegen tool. It is a 2172 x 724 RGBA PNG with genuine transparency, preserved without postprocessing. It matches the existing menu's ivory dry-brush strokes; the game applies the charge/ready colors and a shader-generated soft halo.

Imported assets:

- `/Game/HeavensDivide/Blueprints/UI/Combo/T_ComboInkSlash`
- `/Game/HeavensDivide/Blueprints/UI/Combo/M_ComboInkGlow`

Reimport/build the UI material with `Tools/import_combo_meter_art.py` in Unreal Python. The script reuses existing material nodes and connects the project's Substrate UI output. The material's `InkTexture` parameter contains the source artwork; `GlowStrength` controls the sampled soft halo. The HUD clips the brush horizontally to show charge, preserving its painted shape.

## Final generation prompt

Use case: stylized-concept. Asset type: production transparent PNG texture for a samurai dark-fantasy game's horizontal ability-charge UI bar. Generate ONE isolated ivory-white sumi-e dry-brush sword slash on a genuinely transparent background with alpha. Canvas wide 3:1, preferably 1536x512. Single continuous horizontal brush stroke extending almost edge to edge, with broad readable opaque central body, fine parallel bristle striations, restrained worn ink gaps, ragged hand-painted edges and sharply tapered ends. The stroke should occupy the central half of canvas height, flat straight horizontal silhouette suitable for left-to-right progress clipping. Match refined Japanese ink-wash UI: aged ivory on near-black interfaces with muted antique-gold highlights applied later by the game. Texture itself should be neutral warm white so it can be tinted. No text, no glyphs, no symbols, no frame, no separate objects, no cast shadow, no bloom, no colored background, no checkerboard baked into pixels. Keep all fringes within image bounds. This is a reusable 2D brush mask, not a screenshot or mockup.
