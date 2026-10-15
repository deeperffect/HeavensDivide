# Old Village environment sources

The 21 TripoAI archives from `C:/Users/deepe/OneDrive/Desktop/NewLevel` have been extracted, visually identified, and prepared for Unreal. The Desktop ZIPs are unchanged.

- `AssetCatalog.png`: labeled visual inventory of the supplied models.
- `SourceManifest.json`: original archive names, SHA-256 hashes, original files, working files, and intended dimensions.
- `_Original/<Asset>/`: untouched files extracted from each ZIP, including the original FBX texture folder.
- `Buildings`, `Architecture`, `Landmarks`, `Boundaries`, `Props`: named working FBXs and their base-color textures.
- `Ground`: authored island mesh, waterfront surface, and ground layout mask.
- `LevelPreviews`: reviewed Unreal screenshots of the finished environment and blockout.

Prepared models have a centered ground-level pivot, consistent real-world dimensions, UVs, and a single painted material. The normalization is uniform per model; the original files are retained for any future changes. Working meshes use `SM_OV_`; textures use `T_OV_..._BaseColor`.

Names describe the actual mesh. In particular, the archive called “torii gate” contains a gatehouse; “wooden bridge” is a waterfront walkway; “wooden crate” is an offering box. Several “gate” and “house” archives are architectural modules.

Imported assets live under `/Game/HeavensDivide/Environment/OldVillage`, arranged into meshes by category, textures, and materials. New maps: `/Game/Maps/Lvl_OldVillage` and `/Game/Maps/Lvl_OldVillage_Blockout`.

See `Docs/OldVillage.md` for layout, editing, and rebuild instructions.
