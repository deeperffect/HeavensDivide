# Stylized grass, dirt, and stone ground

Assets are under `/Game/HeavensDivide/Materials/StylizedGround`.

- **MI_StylizedGround**: assign to a horizontal ground mesh for an automatic grass,
  dirt and worn flagstone blend. It uses world XY coordinates, so adjacent floor
  meshes align without requiring custom UVs.
- **MI_StylizedGround_Landscape**: assign to a Landscape. In Landscape Paint,
  create **Weight-Blended Layer Info** assets for **Grass**, **Dirt**, and **Stone**.
  Fill Grass first, then paint Dirt paths and Stone paving with the other layers.
  Set **UsePaint = 0** if you want the automatic blend on a Landscape instead.

Duplicate an instance before making alternate biome presets. Controls:

| Parameter | Effect |
| --- | --- |
| GrassCoverage / StoneCoverage | Automatic coverage thresholds; remaining ground is dirt. These are artistic controls, not exact area percentages. |
| PatchSize | Size of automatic patches, in centimeters. |
| BlendSoftness | Width of transitions between patches. |
| GrassTileSize / DirtTileSize / StoneTileSize | World centimeters covered by each source texture. |
| GrassTint / DirtTint / StoneTint | Multiply each layer's painted color. |
| Brightness / Saturation / MacroVariation | Overall color and broad tonal variation. |
| Roughness / Specular | Surface response; defaults are matte. |
| UsePaint | 0 = automatic coverage; 1 = painted coverage. |

On static meshes, UsePaint=1 uses vertex colors: black = grass, red = dirt,
green = stone. Fill vertices black first; imported meshes often start white.
Smooth painted transitions require enough mesh vertices. Landscape uses its
named layers instead of vertex colors.

Textures use mirrored addressing to keep repeat boundaries continuous and
power-of-two build resizing for mipmaps. Source PNGs are in `Art/StylizedGround`.
The material is opaque and lit, with painted base color and no displacement or
normal-map detail. It is intended for ground surfaces, not vertical cliffs.

`M_StylizedGround_Preview` is an unlit diagnostic version used to render a 45m
sample under `Saved/StylizedGround/blend_preview.png`; use the instances above
for gameplay. Lighting will affect the lit material's appearance in your level.

`Tools/create_stylized_ground.py` creates missing assets and preserves existing
ones. No existing level or material assignment is replaced.

Level 1 currently uses the user's `M_StylizedGround_Inst` in the StylizedGround
folder, with the existing `T_StylizedGround_Grass`, `Dirt` and `Stone` textures.
Its dedicated parent is `M_StylizedGround_SoftBlend`. Grass tiles now cover
1,050 cm (previously 350); dirt remains 1,000 cm and stone remains 1,400 cm.
Three smoothly joined, independently rotated/offset samples vary grass and dirt
without resizing the stones. Explicit UV derivatives preserve mip selection at
sample boundaries. `TextureVariation` controls this variation (1 enabled,
0 regular sampling). The material uses seven texture samples instead of three.

`BlendSoftness` is 0.14 (previously 0.075); `BlendBreakup` is 0.08 and introduces
small irregularities along transitions. Coverage, patch layout, color controls,
texture asset adjustments and surface response retain the user's current values.
`Tools/tune_ground_blending.py` applies this tuning from the captured baseline;
`Tools/review_ground_blending.py` records settings and four views. Backups and
before/after evidence are under `Saved/GroundBlending`. No maps or textures are
saved by this pass. The shared original parent and historical V2 instance remain
available. The unlit preview material is only for texture diagnostics.
