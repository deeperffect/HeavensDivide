# Level 1 reference ground

The current pass places complete painted slabs over the authored dirt and grass
floor. The artwork was created with the built-in imagegen tool using the user's
`Untitled.png` as the style and colour reference: broad olive/khaki stone, ochre
soil and clustered green grass.

## Sources and saved assets

- `RockStamps_Atlas.png`: transparent 2x2 atlas of four intact slab variants.
- [RockStamps_Prompt.md](RockStamps_Prompt.md): atlas generation prompt.
- `RockAtlasRegions.json`: per-variant alpha bounds, two-pixel padding, cropped UV
  rectangles and opaque coverage. Cropping reduces empty space around each plane.
- `RockScatterLayout.json`: generated placements, counts, coverage and placement checks.
- `Dirt_BaseColor.png`, `Grass_BaseColor.png`, `Stone_BaseColor.png`: floor texture
  sources; their generation prompts remain in [Prompts.md](Prompts.md).

The atlas imports to
`/Game/HeavensDivide/Materials/ReferenceGround/T_RockStamps_Atlas`.
`M_RockStamp` and `MI_RockStamp_0` through `MI_RockStamp_3` live in the same folder.
The Level 1 floor still uses
`/Game/HeavensDivide/Materials/EnvironmentPolish/MI_Lvl1_Ground`, with parent
`M_Lvl1_Ground` and the authored RGB mask in
`Art/EnvironmentPolish/Lvl1_GroundWeights.png`.

`GroundRockStamps` is one saved Level 1 actor with four static hierarchical
instanced static mesh components, one per atlas variant. The layout has 65 pieces
in 20 clusters: 23 large slabs and 42 companions, with at most three companions
per cluster. Large slabs span 1,025-1,444 cm against a 1,000-1,500 cm target;
companions use a 420-700 cm range. Variant crop proportions are preserved with
0.9-1.1 aspect variation. These masked planes have no collision, navigation
influence, shadows or ticks. Their world Z is
1.3 cm, just above the floor at Z 1 cm. The material remains lit so the artwork
responds to scene lighting. Textures use sRGB, mipmaps and a power-of-two build
resize; generated PNG source pixels remain unchanged. Material tint compensates
for Level 1's warm lighting.

The slab material uses native texture-sample nodes. Each HISM component's
streaming-distance multiplier accounts for its largest instance relative to the
100 cm source plane. This keeps the large slabs detailed with normal texture
streaming; the atlas is not forced resident and global streaming stays enabled.

`PlacedRockStamps=1` on the floor instance substitutes soil for the repeated stone
texture beneath the placed slabs. Setting it to `0` restores the legacy tiled
floor surface; hide the `GroundRockStamps` actor as well when comparing that
older appearance. The layout mask's stone weights and the scatter's quad
coverage are not measurements of visible rock coverage: atlas transparency also
matters. Quad footprints cover 34.18% of the arena; the estimated opaque rock
footprint is 19.90%. Counts and placement checks are recorded in
`RockScatterLayout.json`.

## Rebuild and review

1. For terrain layout changes, edit `Tools/level1_ground_layout.json` and run
   `Tools/generate_level1_ground.py` to rebake grass/dirt/stone weights.
2. Edit `Tools/ground_rock_scatter.json` and run
   `Tools/generate_ground_rock_scatter.py` with Python to rebuild deterministic
   slab placements. The generator checks arena bounds, spacing and clearances.
3. Run `Tools/apply_ground_rock_scatter.py` through Unreal Python to import the
   atlas/materials and save the tagged actor in Level 1. It backs up replaced
   assets and the map under `Saved/GroundRockScatter/Backups`.
4. Run `Tools/review_ground_rock_scatter.py` in a full editor with
   `-ExecutePythonScript` and `-RenderOffscreen`. Normal texture streaming remains
   enabled. Its six views include gameplay framing, overview, close-up, low-angle
   and telegraph readability probes. The latter are static visual probes, not a
   gameplay simulation. Reports and captures go to `Saved/GroundRockScatter`;
   see the script docstring for same-session review commands.

The previous material-only pipeline, `Tools/apply_reference_ground.py` and
`Tools/review_reference_ground.py`, remains available, with review evidence under
`Saved/GroundReferencePass`. Floor rebuilds preserve `PlacedRockStamps`.

Shared original textures remain in `Art/StylizedGround`; boss-arena and encounter
wall materials retain their original references.
