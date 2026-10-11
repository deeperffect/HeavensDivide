# Environment polish

The gameplay maps use a warm daylight key light, cooler sky fill, fixed exposure, restrained bloom, and one authoritative unbound grade. Level 1 has a connected shrine approach, narrower meadow paths, worn objective clearings, broken stone at the shrine approach, and small grass islands. The arena uses a lit stone floor.

## Ground controls

Level 1's visible Plane previously used the unlit diagnostic `M_StylizedGround_Preview`. It now uses `/Game/HeavensDivide/Materials/EnvironmentPolish/MI_Lvl1_Ground`. Its source material uses world coordinates and a linear RGB weight texture: red = grass, green = dirt, blue = stone. This keeps routes anchored to landmarks regardless of the floor mesh UVs. The existing painted surface textures and the 350 cm grass scale are retained. A second, rotated sample breaks up repeated grass and dirt brush strokes without changing the source images.

Edit route points, widths, clearing ellipses and grass islands in `Tools/level1_ground_layout.json`, then run `python Tools/generate_level1_ground.py`. The standard-library-only generator writes the mask and a colored layout preview to `Art/EnvironmentPolish`. Run `Tools/apply_environment_polish.py` through Unreal Python to import the mask, update this pass's materials and save the maps. Every apply backs up the maps and any existing pass-owned materials under `Saved/EnvironmentPolish/Backups`.

The level-specific material instance exposes texture size, tints, saturation, brightness, roughness, and Layout Edge Variation. The routes are intentionally authored; small noise only softens their edges and breaks up the meadow surface.

## Lighting controls

Level 1 streams its own always-loaded `Lvl_B1_Lvl1_Lighting` sublevel, copied from the vendor lighting level. This is necessary because its original lights and heavy cyan fog lived outside the main map. The vendor level remains unchanged. The arena keeps its existing lighting actors. The volume named **Bloodshift Combat Grade** owns the grade at priority 50 and full blend weight. The older unbound volume is disabled to prevent its bloom, vignette and blur settings from mixing back in. Four short-range, shadowless lights accent the existing shrine lanterns in Level 1.

Geometry, collision, gameplay actors, objective positions, enemy balance, camera behavior and the main menu are preserved. Original ground textures and shared materials are hash-checked. The two gameplay maps, the new Level 1 lighting sublevel, and four new material/texture assets contain the visual changes.

## Visual review

Run `Tools/capture_environment_polish.py` with full-editor `-ExecutePythonScript` and `-RenderOffscreen`. Captures go to `Saved/EnvironmentPolish/After`; `-EnvBefore` selects Before. `-EnvBefore -EnvApply` captures the baseline, applies the pass, then captures the result. Four camera views cover the whole field, the player start, the shrine approach and the arena. No review cameras are saved.

## Current settings

| Control | Level 1 | Arena |
| --- | --- | --- |
| Sun intensity / temperature | 7.5 lux / 5800 K | 7 lux / 5100 K |
| Sun pitch / yaw | -43 / -38 degrees | -48 / 130 degrees |
| Sky intensity | 1.75 | 1.3 |
| Exposure | Fixed EV100 1, compensation +0.8 | Same |
| Bloom / vignette | 0.16 / 0.15 | Same |
| Fog density / maximum opacity | 0.005 / 0.12 | 0.007 / 0.12 |

These light intensities are authored for the project's fixed exposure, rather than a physically calibrated outdoor camera. The extra dense fog layer is disabled; regular fog begins 65 m away. Motion blur, chromatic aberration and lens flares are disabled in the grade.

Final review images are in [Saved/EnvironmentPolish/FinalPolish](../Saved/EnvironmentPolish/FinalPolish); the original views are in [Before](../Saved/EnvironmentPolish/Before). The overhead view includes staging objects outside the playable fence; those objects were not moved.

Run `Tools/verify_environment_polish.py` through Unreal Python to reload both saved maps and check actor transforms, lit materials, the lighting sublevel, sun directions, haze, post-processing and lantern counts. It writes `Saved/EnvironmentPolish/Verification.json`.

The original pre-pass gameplay maps are backed up at `Saved/EnvironmentPolish/Backups/20261010_213913/Content/Maps`. Later backup folders contain intermediate polish iterations. To revert this pass's map changes, close the editor and restore those two original maps; their original lighting reference is included.

Validation completed on 2026-10-10: all 11 saved-map checks passed, all four final editor views were inspected, and Python syntax plus `git diff --check` passed. Review rendering used software Lumen with ray tracing disabled only for the capture process; project renderer settings were not changed. This pass was not performance-benchmarked.

A separate, fresh Unreal commandlet also reopened the saved maps and passed all 11 checks, exiting with code 0.
