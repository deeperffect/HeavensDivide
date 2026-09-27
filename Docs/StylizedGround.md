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
