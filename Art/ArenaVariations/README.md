# Arena surface variations

Ten optional project-owned floor and matching rim presets for Level 1. The level currently uses the earlier grass-and-dirt material (`MI_Lvl1_Ground`); these presets are retained for later use. **01 Verdant ochre** was the original selection for this pass. Open the [comparison gallery](../../Saved/ArenaVariations/Gallery.html) for gameplay-camera and whole-arena views.

| ID | Direction | Floor material |
| --- | --- | --- |
| 01 | Verdant ochre: grass, muted earth, olive stone | MI_01_VerdantOchre |
| 02 | Frost meadow: jade, pale stone and frost | MI_02_FrostMeadow |
| 03 | Violet sandstone: flowing purple and sandy rock | MI_03_VioletSandstone |
| 04 | Moss slate: moss with cool scattered slate | MI_04_MossSlate |
| 05 | Rose courtyard: fitted dusty lilac paving | MI_05_RoseCourtyard |
| 06 | Amethyst fracture: subdued violet rock seams | MI_06_AmethystFracture |
| 07 | Overgrown ruins: broken pavers and jade moss | MI_07_OvergrownRuins |
| 08 | Ember basalt: dark earth and copper cracks | MI_08_EmberBasalt |
| 09 | Moonlit slate: blue slate and teal grass | MI_09_MoonlitSlate |
| 10 | Celestial stone: storm blue and lavender paving | MI_10_CelestialStone |

All materials are under `/Game/HeavensDivide/Materials/ArenaVariations`. Each floor has a matching `MI_Rim_<ID>` preset. `M_ArenaSurface` and `M_ArenaRim` are their shared parents. Textures are `T_<ID>`.

## Select a preset

With **Lvl_B1_Lvl1** open in Unreal, run this in its console, changing the ID:

```python
py import runpy, unreal; runpy.run_path(unreal.Paths.project_dir()+"Tools/select_arena_variation.py", init_globals={"VARIANT_ID":"09_MoonlitSlate"})
```

This changes the floor and rim materials together, backs up the map, and saves Level 1. After restoring the grass arena, also show the three `TerracottaArenaRim` mesh components and hide `GroundRockStamps` and the `SM_ShrineFence03a` components to recreate the preset's perimeter. You can assign the floor material manually to `Plane`, and the corresponding rim material to all three rim components.

## Scale and quality

The primary tile is 14 metres across rather than 110 metres. Three independently offset and rotated samples blend continuously to reduce repeated motifs; broader color variation is independent of that tiling. Smaller painted features and higher source texel density address the previous enormous, pixelated floor. Texture filtering and normal mip streaming remain active. The native artwork is 1254 square; Unreal's 2048 power-of-two build is a platform conversion, not new artwork resolution.

Editable controls: `TileSize`, `DetailMipBias`, `Brightness`, `Contrast`, `Saturation`, `GroundTint`, `ColorVariation` and `RimColor`. Lower `TileSize` makes features smaller and increases source density. Positive `DetailMipBias` softens fine texture detail. The floor UV density assumes the existing 15000 cm Plane; update that material constant if the mesh footprint changes.

## Source and reproduction

Mode: **built-in image_gen**. The ten PNGs beside this README are the preserved generated source assets. Each prompt file contains the exact generation or edit prompt. The five simplified grassy textures use `_v2.png` and `_v2_Prompt.md`; their first versions are retained. [GenerationManifest.json](GenerationManifest.json) records their native dimensions and prompt paths; [Variants.json](Variants.json) defines the preset list and rim palettes. [Research.md](Research.md) records Riot's published guidance and separates it from this project's implementation choices.

- `Tools/apply_arena_variations.py`: imports the source art, builds the owned materials and applies the selected preset. Saves only these assets and Level 1, with dated backups in `Saved/ArenaVariations/Backups`.
- `Tools/select_arena_variation.py`: switches saved floor/rim pairs without rebuilding art.
- `Tools/review_arena_variations.py`: captures all ten with matching lighting and camera. `-GroundApplyFirst` runs the apply step first; `-ArenaQuick` limits the initial review to the default.
- `Saved/ArenaVariations/ValidationSummary.json`: final evidence and limitations. Packaged gameplay is not covered by Editor captures.

The earlier terracotta artwork/materials remain in `Art/TerracottaArena` and `Materials/TerracottaArena`. Dated map backups preserve the prior assignments and stone scale.
