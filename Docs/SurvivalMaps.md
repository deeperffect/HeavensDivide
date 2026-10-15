# Additional survival maps

Four destinations are selectable in New Run. Begin Run, Restart Run and the victory screen use the selected destination. Old Village remains the initial selection and its level is not modified by this rework.

## Revision two

The previous Moonlit Forest, Sunken Sanctuary and Ember Quarry layouts are replaced in their existing level packages. Their destination IDs, difficulty ranks, run reports and packaging entries remain valid. Previous level files are backed up under `Saved/SurvivalMaps/Backups`; the first-pass previews are archived in `Saved/SurvivalMaps/BeforeRework`.

| Destination | Unreal level | New layout and theme |
| --- | --- | --- |
| Moonlit Forest | `/Game/Maps/Lvl_MoonlitForest` | Three braided trails through broadleaf groves, with a moon temple, ranger cottage, wayside shrines, supply hut and pilgrims' rest. Blue moonlight and warm rest-stop lamps. |
| Sunken Sanctuary | `/Game/Maps/Lvl_SunkenSanctuary` | Staggered courts around three differently proportioned reservoirs. Roofed arcades, ruined bell towers, curved shrines, archaeological camps and overgrown garden walls. |
| Ember Quarry | `/Game/Maps/Lvl_EmberQuarry` | A broad asymmetric canyon with winding haul routes, sandstone ridges, a branching railway and four mining work yards. Mine faces, timber entrances, shelters, carts, tools and storage are grouped by site. |

The rework uses JapaneseShrine, Ancient_Ruins, GreenCave and Stylized_Village, together with imported Tripo buildings from the OldVillage asset collection. The original vendor models are preserved; changes to collision and tree LODs use local copies. Props are positioned by their bounds so off-centre source pivots do not disrupt arrangements. Small tools are placed on tables/crates, planting follows trees and masonry, and larger objects use collision. Embedded decorative tracks and low groundcover do not snag movement.

Three new hand-painted diffuse textures are stored in `Art/Environment/SurvivalMaps/Textures`: forest moss, limestone flagstones and quarry earth. `GenerationPrompts.json` records the exact prompts and built-in image generation provenance. Unreal copies live in `/Game/HeavensDivide/Environment/SurvivalMaps/Shared/Textures`; the texture build stretches them to a power of two for mipmaps and uses mirrored addressing. Each map's ground material combines world-aligned paint textures with authored trails, clearings and broad colour variation. No displacement is used, keeping the combat surface smooth.

Far terrain uses an inexpensive unlit colour gradient based on altitude, avoiding grid-like lighting on the coarse distant geometry. `review_survival_backdrop_materials.py` checks the material assignments and refreshes the overview captures after a cold load; its report is `Saved/SurvivalMaps/BackdropArtValidation.json`.

All three maps retain the survivor game mode, authored waves, shared run clock, objective and encounter directors, eight objective clearings, Mitko Igra music and a boss torii gate linked to `Lvl_B1_Arena` at 600 seconds. Boundary collision, static navigation, budgeted enemy path requests and camera dissolve carry over. Enclosed structures and tree trunks use simple proxies where appropriate. Distant scenery and small groundcover use hierarchical instancing, no navigation contribution and no dynamic shadows. Fog is non-volumetric; local forest accent lights do not cast shadows.

## Editing and rebuilding

- Layouts, routes, clearings and palette: `Tools/survival_map_layout.py` (centimetres).
- Authored districts, landmarks and planting: `Tools/survival_map_dressing.py`.
- Terrain generation: Blender `--background --python Tools/generate_survival_terrain.py`.
- Build: `Tools/Run-OldVillage.ps1 -Script build_survival_maps.py`. This replaces only the three new maps. Do not run it after manual edits unless replacement is intended.
- Review: `Tools/Run-OldVillage.ps1 -Script review_survival_maps.py -Render`.
- Source geometry, textures and previews: `Art/Environment/SurvivalMaps`.
- Unreal content: `/Game/HeavensDivide/Environment/SurvivalMaps`.
- Build and validation reports: `Saved/SurvivalMaps`.

New Run uses a two-by-two destination grid. The saved `FallenGrove` ID identifies Old Village; `HollowMarsh` identifies the sanctuary and `CinderCitadel` the quarry. Moonlit Forest has its own `MoonlitForest` ID. All three replacement levels remain in the packaging list.

## Revision two verification

The saved replacement maps passed all 24 objective navigation routes and 24 gameplay ground traces, plus 25 boundary capsule sweeps. Real PIE runs advanced the shared clock and spawned 37 enemies in each recorded 10.5-second sample. Accelerated unsaved schedules produced two shrines in each map; all saved gate timings remain 600 seconds and the gates stayed locked during these short runs.

`HeavensDivide.Progression.RunPreparation`, `HeavensDivide.Progression.RunReports` and `HeavensDivide.UI.RunPreparationAndMinimap` all passed. The final gameplay review also checks groundcover dimensions, prop grounding, camera-fade coverage and that the replacement materials compile without errors. These are startup, route and collision checks, not a full ten-minute run or a frame-time benchmark.

Counts before the targeted corrections below:

| Map | Level actors | Instanced plants and distant scenery | Authored mesh sources |
| --- | ---: | ---: | ---: |
| Moonlit Forest | 687 | 1,976 | 27 |
| Sunken Sanctuary | 709 | 2,382 | 32 |
| Ember Quarry | 602 | 1,671 | 23 |

The counts include gameplay/collision helpers in the actor column; source counts exclude the generated ground and engine helper geometry. Small repeated detail has no dynamic shadows or navigation contribution. Groundcover horizontal size is capped at 160 cm, with blue flower heads capped at 60 cm. Decorative rails remain nonblocking, with the wooden ties visible above the ground.

Reports: `Saved/SurvivalMaps/BuildReport.json`, `Validation.json` and `FinalArtValidation.json`. Rendered overview, clearing, district and gameplay images are in `Art/Environment/SurvivalMaps/Previews`. `Tools/finish_survival_rework.py` applies the final raised-roof camera fade and visual scale corrections without rebuilding the layouts; `render_survival_rework_final.py` reapplies those idempotently and refreshes the art review. The terrain exporter compensates for FBX's Y reflection so reservoir openings, collision and authored coordinates agree.

## Perimeter, collision and ruins correction

`Tools/polish_survival_obstacles.py` updates the saved maps without replacing their layouts. Map and sandstone asset backups are in `Saved/SurvivalMaps/ObstaclePolish/Backups`. The full builder also carries these corrections forward. Old Village is unchanged and its file hash is checked by the patch.

- Quarry canyon escarpments are now 6.28–8 m high, down from roughly 24–34 m, with their bases still below the play floor. Each is 10.5 m wide along the boundary and only 3.5 m deep, with its centre 6 m outside the playable edge. Their overlapping width keeps a continuous cliff line while the shallow depth keeps scenery out of the camera corridor. Near background formations are reduced, and distant formations whose bounds overhang the edge are moved back. The independent invisible play boundary stays sealed.
- Moonlit woodland perimeter trees are now 7.64–10.98 m high, approximately 60% shorter, and shifted slightly outward. Nearby background trees are capped at 14 m. Far scenery still provides a forest horizon.
- All 85 sandstone obstacles use one shared, reduced collision mesh with 412 triangles, compared with 2,064 triangles in the visible model. The old axis-aligned boxes are removed. Convex decomposition bridged the deep crevices even at higher hull counts; the reduced static triangle mesh preserves those spaces. Unreal uses its static triangle acceleration structure for movement queries. Collision is prepared offline and navigation remains static, with no new runtime generation or ticking. This is a complexity reduction against full visual-mesh collision, not a measured frame-time comparison against the old boxes.
- The original cloister support was `Ancient_Ruins/Meshes/SM_Column`, an open-backed wall pilaster incorrectly used freestanding. It was not a newly modelled asset. All 20 supports now use `SM_Corner_Pillar` from the same pack, verified closed at all four render LODs. Vendor assets are unmodified.
- `Tools/survival_ruin_details.py` adds Ancient_Ruins windowed tower walls, shrine coping, a layered flooded altar, carved wall-mounted fountain masks, paired tree trunks/canopies, root gardens, buttresses and vines. Archaeology camps use the pack's crates, barrels and bags. Four instanced courtyard paving patches add 84 tiles; their tops are under 1 cm above the existing collision floor, with no collision, navigation or dynamic shadows.

The imported `Ancient_Ruins/Levels` folder contains landscape layer information but no example `.umap`. No example map was available in the project, searched project/download folders or installed Epic vault cache; the additions use the inspected models and matching kit pivots directly. Source inspection and topology reports are in `Saved/SurvivalMaps/ObstaclePolish`.

Review command: `Tools/Run-OldVillage.ps1 -Script review_survival_obstacle_polish.py -Render`. It checks the perimeter dimensions, closed pillar LODs, gameplay and boundary collision, objective navigation, and simple-versus-rendered sandstone surfaces at character height. It also captures close views of the perimeter, sandstone, cloister and flooded altar alongside the usual map previews.

The collider source and measurements are in `Art/Environment/SurvivalMaps/Collision`. `build_sandstone_collision_blender.py` simplifies the source mesh and compares 2,315 character-height rays against the original surface. For the selected mesh, 95% of sampled collision points are within 5.82 cm of visible geometry, and the largest nearest-surface distance is 16.93 cm. Long ray-distance differences at grazing angles are reported separately, since a tiny displacement at the lip of a crevice can move the first intersection to another rock face. The Unreal review compares against an independent copy of the original vendor mesh and tests a player capsule on any grazing-ray misses. `apply_quarry_collision_mesh.py` assigns the custom collider, preserves the low cliffs' width and rebakes the eight quarry objective routes. Temporary hull trial assets are removed.

The final PIE check compared 2,313 rock-surface hits. Both grazing rays that missed the reduced mesh still blocked a player capsule. All 24 objective routes and 25 boundary sweeps passed across the three maps; each 10.5-second gameplay sample advanced the shared clock, spawned 37 enemies and spawned two test-scheduled shrines, with the 600-second gate still locked. The aggregate report is `Saved/SurvivalMaps/ObstaclePolish/Validation.json`; the final quarry pass is `QuarryFinalValidation.json` in the same directory. This does not replace a full-combat frame-time benchmark.

`finish_quarry_boundary.py` applies the shallow, edge-aligned cliff placement and moves distant overhangs out of the camera corridor. `quarry_edge_apron.py` adds a ten-triangle, nonblocking ground extension beneath the lower wall, matching the existing painted ground material and concealing the exposed stair-stepped terrain seam. `finish_quarry_edge_art.py` saves that visual-only addition and renders the final overview. None of these decorative edge pieces contribute to navigation or gameplay collision.
