# Connected stone ground

This later pass is retained but no longer active. Level 1 uses the user-selected
September 27 version 2 [stylized ground](../StylizedGround/README.md). See
`Saved/RestoreStylizedGroundV2` for restoration evidence.

This pass's surface is `/Game/HeavensDivide/Materials/ConnectedGround/MI_ConnectedGround`.
The source combines closely fitted grey-olive slabs, muted brown joints and
grass growing into those joints. This replaces the isolated charcoal stamps
and broad orange soil of the rejected LayeredGround pass.

`ConnectedSlabs_BaseColor.png` is the original 1,254-square generated artwork.
The prompt requested 2,048 pixels; the built-in tool returned 1,254. Unreal's
2,048-square platform conversion adds no new source detail. Exact instructions
and provenance are in [Prompts.md](Prompts.md).

The source spans 1,400 cm in world space, approximately 0.90 native texels/cm.
The prominent stone faces are roughly 2-5 m across, rather than the previous
10-15 m stamp patches. Floor scale does not affect the material coordinates.

Three offset and rotated interior samples blend on a triangular grid. This
avoids the source image's nonmatching borders and obvious repeating squares.
Transitions favor intact stone faces with narrow soft joins, reducing ghosted
overlapping slab patterns. They are an artistic approximation, not a geometric
paving layout. Separate broad variation changes color very slightly without
changing stone dimensions. Explicit derivatives select normal mip levels;
texture streaming remains enabled. The material is matte and fully lit.

`Tools/connected_ground.json` holds the editable defaults. `TileSize` controls
physical artwork scale; `Brightness`, `Saturation` and `GroundTint` compensate
for the saved level lighting. `PatchBlendWidth` and `SeamPreservation` control
patch joins. `DetailMipBias` is zero.

Run `Tools/apply_connected_ground.py` through Unreal Python to import and apply.
It backs up the map and owned assets to `Saved/ConnectedGround/Backups`, assigns
the floor, and hides the previous GroundRockStamps components in editor and game.
Their instances remain saved for reversal. It preserves the floor's dimensions,
collision, fences, props, objectives and lighting, and protects earlier material
assets with hashes.

`Tools/review_connected_ground.py` renders the actual gameplay camera with a
real-size Samurai, a top-down view and a closeup. A second closeup temporarily
doubles the floor in both directions to check that the rock scale stays fixed.
It restores that scale and does not save the review scene. Saved-asset and
scene checks are in `Tools/inspect_connected_ground.py`.

The fresh-load scene checks and six reviewed captures pass, including normal
texture streaming and the four-times-area scale comparison. All 84 protected
earlier assets retain their hashes. Evidence and the visual review qualifications
are in `Saved/ConnectedGround/ValidationSummary.json`; final views are under
`Saved/ConnectedGround/Tuned`. Packaged playtesting remains pending.
