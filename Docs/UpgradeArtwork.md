# Build family artwork

All 160 build-family cards have generated illustrations assigned: 20 abilities, 60 branches, 60 scaling upgrades, and 20 synergies. A separate editor commandlet reload verified every saved Card Artwork and Icon reference, unique source PNG, and UI texture size setting using `Tools/validate_upgrade_art.py`.

New card illustrations use the existing CardArt2 assets as style references: crimson ink for Samurai, violet ink for Ninja, and both colors over muted parchment for synergies. Artwork contains no labels or frames; the card widget supplies those.

Source PNGs are in `Art/UpgradeCards/Generated`. The complete 160-card job list, art direction, target DataAssets, and texture paths are in `Art/UpgradeCards/manifest.json`. Images are generated individually with the built-in image generation tool. The reference exports in `Art/UpgradeCards/References` preserve the original assets.

Imported textures live in `/Game/HeavensDivide/Blueprints/UI/BuildFamilyArt`. Each matching upgrade DataAsset uses its texture in both **Card Artwork** and **Icon**. To substitute an illustration, change these fields on the DataAsset. The new textures use sRGB, UI compression, no mipmaps, and a 512-pixel maximum in-game size; full-resolution PNG sources remain available.

`Tools/import_upgrade_art.py` imports available images and assigns only artwork fields. It preserves balance settings, rarity, prerequisites, and gameplay behavior. It can be run again to pick up missing images; existing imported textures are not replaced. Reimport a texture explicitly in the editor if its source PNG changes after import.

The latest import result is `Art/UpgradeCards/import_report.json`, with explicit lists of assigned and pending cards. Original DataAssets are backed up before their first artwork change under `Saved/Backups/UpgradeArtwork`.
