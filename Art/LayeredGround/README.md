# Layered reference ground

This pass is retained for reversal. It is no longer active: the user rejected
the isolated dark stones and orange soil. Technical validation below does not
establish visual fidelity. The later connected pass was also rejected; Level 1
uses the user-selected September 27 version 2
[stylized ground](../StylizedGround/README.md).
Restoration evidence is in `Saved/RestoreStylizedGroundV2`.

This retained pass uses fixed-size painted grass/dirt tiles, independently placed stone
silhouettes and a separate mask for broad composition and stone-edge blending.
The darker grey-olive stone palette follows the user's reference and correction;
the earlier pale/yellow atlas was rejected. The restored ground assets remain
available in `EnvironmentPolish` and `ReferenceGround`.

## Assets and scale

- `RockStamps_BaseDark.png` and `RockStamps_Additional.png`: eight silhouettes
  across two transparent sheets. Built-in imagegen prompts are in [Prompts.md](Prompts.md).
- `RockAtlasRegions.json`: measured alpha bounds, safe UV rectangles and source hashes.
  Source pixels and transparency are retained; metadata crops are UV rectangles.
- `RockScatterLayout.json`: deterministic positions in centimeters. Hero widths
  remain 1,000–1,500 cm and companions 420–700 cm, matching the prior size ranges.
- `GroundBlend.png`: numerical RGBA data. R is the irregular grass fringe around
  actual stone alpha silhouettes; G is narrow contact shading; B is broad color
  and sample variation; A breaks up grass/soil transitions.

Grass and dirt use the existing `ReferenceGround` artwork at 1,000 and 1,500 cm
per tile. Texture world coordinates are independent of floor size and the broad
layout mask. Two rotated/offset samples per surface reduce repeated motifs.
The material retains the restored surface colors and painted detail. Rock
materials are lit, matte and use subtle cool compensation for Level 1 lighting.
Rock brightness is 0.45: the first in-engine review showed that 0.90 made even
the corrected dark artwork too pale under the arena lighting.

The current arrangement contains 62 pieces in 16 clusters, using all eight
variants. Opaque stone coverage is estimated at 20.45%, close to the previous
19.90%. Stones are flat masked instances with no collision, navigation influence,
shadow casting or ticking. The gameplay floor, fences, props and lighting retain
their transforms and collision. This is ground presentation, not a map-boundary
or encounter-size change.

Runtime assets live in `/Game/HeavensDivide/Materials/LayeredGround`. The floor
uses `MI_Lvl1_Ground`. Its `Layered Ground` controls expose `RockEdgeGrass`,
`ContactShade`, `BlendEdgeVariation`, `BlendWidth`, `GroundColorVariation` and
`RepeatVariation`. `M_RockStamp` and eight instances control atlas crops and tone.
Normal texture streaming and mipmaps remain enabled. The 1,254-square source
atlases build to 2,048 in Unreal; this conversion does not invent source detail.

## Rebuild

With regular Python, run these in order from the project root:

```powershell
python Tools/prepare_layered_ground.py
python Tools/generate_layered_ground.py
python Tools/generate_layered_ground_masks.py
```

Run `Tools/apply_layered_ground.py` through Unreal Python with the interactive
editor closed. It backs up Level 1 and any replaced owned assets under
`Saved/LayeredGround/Backups`, builds the materials, replaces only the tagged
`GroundRockStamps` actor and assigns the floor material. Source textures and
earlier materials are protected by hashes; unrelated scene state is checked
before saving. `Tools/review_layered_ground.py` performs fresh-load checks and
captures gameplay views with a real-size Samurai, an overview and closeups.

## Larger areas

```powershell
python Tools/generate_layered_ground.py --extent-scale 2 --output-root Saved/LayeredGround/ScaleProbe
python Tools/generate_layered_ground_masks.py --layout-root Saved/LayeredGround/ScaleProbe
```

Doubling both dimensions creates four times the area. Density targets and
cluster counts scale with area; individual rock lengths, gaps, fringe widths
and grass/dirt tile sizes stay fixed. The blend mask grows from 1,024 to 2,048
pixels per side to retain 14.84375 cm per data texel. The broad authored layout
is expanded separately. At larger extents, split masks into tiles rather than
exceeding the generator's 8,192 limit.

The expansion command writes a separate layout and mask for review; it does not
resize the gameplay map or its fences. Applying it to a larger level also requires
an authored floor, matching layout origin/size and deliberate gameplay boundaries.
The current Level 1 importer intentionally consumes the default `Art/LayeredGround`
layout only. Fresh Editor loading, saved scene checks, the expanded layout/mask
check and six reviewed previews pass. Evidence is recorded in
`Saved/LayeredGround/ValidationSummary.json`; packaged playtesting is pending.
