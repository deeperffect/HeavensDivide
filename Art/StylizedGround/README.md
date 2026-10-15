# Ground texture sources

Level 1 currently uses the user's `Materials/StylizedGround/M_StylizedGround_Inst`
with the existing `T_StylizedGround_*` textures and their user-edited settings.
The dedicated `M_StylizedGround_SoftBlend` parent enlarges grass to 1,050 cm,
varies grass/dirt repeats and feathers their transitions. Dirt/stone scale and
the texture artwork remain intact. See [ground controls](../../Docs/StylizedGround.md)
and `Saved/GroundBlending` for the current tuning and before/after evidence.

The earlier restored **September 27 version 2** was selected from
`Saved/StylizedGround/blend_preview_v2.png`: simple grass with small tufts and
broad angular stones. Its sources are `Previous/Grass_BaseColor_v2.png`,
`Previous/Dirt_BaseColor_v2.png` and `Previous/Stone_BaseColor_v2.png`.
The dedicated `Materials/StylizedGroundV2/MI_StylizedGroundV2` instance uses the
original `StylizedGround/M_StylizedGround` automatic blend, 1,000 cm grass/dirt
tiles and 1,400 cm stone tiles. The top-level PNGs below remain the retained
third-pass sources. `Tools/restore_stylized_ground_v2.py` restores the selection;
`Tools/review_stylized_ground_v2.py` captures it in the saved Level 1 map.
Restoration backups and validation are in `Saved/RestoreStylizedGroundV2`.

Original bitmap textures generated with the built-in imagegen tool for this
project, guided by the user's reference for a simple painted MOBA ground style.

- Grass_BaseColor.png: olive/jade grass and broad painted tufts.
- Dirt_BaseColor.png: warm packed earth with sparse embedded pebbles.
- Stone_BaseColor.png: broad irregular worn grey/olive flagstones.

All were requested as straight overhead, evenly lit albedo surfaces without UI,
props, perspective, or strong cast shadows. Unreal mirrors the imported textures
at repeat boundaries and builds mipmaps. They are original generated textures,
not textures extracted from the reference game.

The second pass simplifies grass into broad olive/jade color fields with sparse
angular blade clusters, and stone into smooth large slabs with painted bevels
and a few deliberate cracks. First-pass source files are retained under `Previous`.
The user's Summoner's Rift screenshot and online references guided the shape and
detail choices, including [Ranko Prozo's SR texture study](https://www.rankoprozo.com/portfolio-1/league-of-legends-nexus-blitz-ykkdr).
`Tools/update_stylized_ground_textures.py` updates the three texture assets in place
while preserving sampler settings and material-instance tuning.

The third pass follows the three additional user-provided terrain references:
long fractured gray-olive slabs embedded in ochre earth, dense directional grass
brushwork, and quiet painted dirt without photographic gravel. This replaces all
three source textures; second-pass sources are retained as `Previous/*_v2.png`.
The existing materials and instances keep their tuning. Built-in imagegen prompts
are recorded in [Prompts_v3.md](Prompts_v3.md). The blended diagnostic render is
`Saved/StylizedGround/blend_preview_v3.png`.
