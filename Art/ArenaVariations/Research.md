# Riot environment research and this arena pass

Research date: 14 October 2026. These are public production principles, not a claim to reproduce Riot's proprietary shaders or its current internal pipeline. The supplied screenshots guide the palettes, painted shapes and surface treatment. No screenshot pixels, characters, counters or UI are imported into the game.

## Published guidance

- Riot's [Environment Art education page](https://www.riotgames.com/en/artedu/environment-art) describes environments as gameplay communication: the art should help players understand combat space, interest and context while supporting the fantasy. Applied here: keep the floor subordinate to characters and combat effects, with restrained contrast and matte response.
- In the [TFT Fates environment-team breakdown](https://magazine.artstation.com/2020/10/riot-games-teamfight-tactics-fates-league-of-legends-art-blast/), senior environment artist Jeremy Page separates fixed board anatomy, supporting natural elements, cultural props and points of interest. The board template remains consistent across themes, and game clarity remains a priority. Applied here: ten surface/rim treatments share the same combat footprint, shrine approach and gameplay camera. Decorative theme changes do not add obstacles.
- Riot's [Bringing the Festival to Life](https://teamfighttactics.leagueoflegends.com/en-us/news/dev/bringing-the-festival-to-life/) discusses concept-to-3D collaboration and keeping decorative elements from obstructing important gameplay, including on mobile. Applied here: no floor-sized bright emblem or glowing fissure; ember and amethyst variants use subdued painted accents. This does not assert that these materials have been tested on mobile.
- [TFT Reckoning: Arenas from Concept to Life](https://teamfighttactics.leagueoflegends.com/en-us/news/dev/tft-reckoning-arenas-from-concept-to-life/) emphasizes repeated feedback and a substantial beauty pass. Applied here: compare all variants under identical saved lighting, at the real gameplay-camera pose, and also inspect their whole-arena repetition.
- Riot's older [A Trip Down the LoL Graphics Pipeline](https://technology.riotgames.com/node/55) describes grass shadows already present in static terrain artwork and separate animated grass geometry. Search-indexed excerpts were available; the full page timed out. This is historical evidence for separating painted terrain cues from actual geometry, not documentation of today's TFT material stack. Our new floor remains flat artwork; the arena rim uses real geometry and scene lighting.

- Environment artist Anessa Silzer describes her [Summoner's Rift Update work](https://asilzer.artstation.com/projects/JkYzZ) as terrain texture painting, polishing hand-painted environment textures, and ground-lane layout work. This firsthand account supports the emphasis on painted color and layout, rather than inferring that a photorealistic material stack produces the reference style. Our source art is generated in that broad painted visual direction.

## Our Unreal implementation choices

These numeric choices are project-specific, not rules attributed to Riot:

- Ten independent base-color textures, each **1254 x 1254 native pixels**, generated with built-in imagegen. Exact prompts are beside the files. Unreal builds power-of-two platform textures and mipmaps; the 2048 import ceiling does not create new source detail.
- A tile covers **1400 cm** instead of the previous **11000 cm**. Native source density rises from **0.114 to 0.896 texels/cm**, a **7.86x** increase before filtering. This addresses stretching without pretending that upscaling increases detail.
- Three independently offset and quarter-turned native texture samples blend on a triangular lattice. Explicit rotated UV derivatives avoid mip seams at patch boundaries. Independent broad color variation breaks up the map-wide value pattern. Each patch samples inside the source image with clamp addressing, avoiding the repeated diamonds caused by mirror tiling. The blending lattice is finer than the texture scale; a full source image still corresponds to 14 metres. This is our material solution, not a claimed Riot shader technique.
- Matte lit surfaces, very low specular response, adjustable contrast/saturation and mip bias. No high-frequency normal or roughness textures. Five grassy sources were simplified in a second imagegen pass to remove fine stippling; original versions remain beside them. Material controls also suppress fine detail.
- The old 10-14 m painted floor stamps are hidden. The new tile contains much smaller stone shapes. Perimeter stones are reduced to roughly 3.2-4.05 m across, with a lower profile, instanced through the existing three components.
- Review through the game's actual camera rig at 1920 x 1080 with a temporary Samurai scale reference, plus whole-floor overviews. Normal texture streaming stays enabled. Review actors and material swaps are transient.

These choices approximate the reference's readable painted surfaces. They do not reproduce a complete TFT arena composition or its game engine.
