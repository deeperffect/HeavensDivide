# Terracotta arena artwork

Replaces the Level 1 grass-and-dirt look with the user's warm painted arena
reference: terracotta ground, purple slabs and a chunky purple stone frame.
Characters, creatures, symbols and counters from the reference are excluded.

- [Terracotta_BaseColor.png](Terracotta_BaseColor.png): original generated artwork, produced with the built-in image_gen tool. No pixel postprocessing was applied.
- [Prompt.md](Prompt.md): complete generation prompt. The user's attached screenshot supplied the style reference.
- Unreal assets: `/Game/HeavensDivide/Materials/TerracottaArena`.
- Saved map: `/Game/Maps/Lvl_B1_Lvl1`.
- `Tools/apply_terracotta_arena.py`: imports the artwork, creates materials and applies the saved Level 1 visuals with backups.
- `Tools/review_terracotta_arena.py`: renders the saved level using its gameplay camera and lighting.

The purple slabs reuse the existing project-owned rock atlas through a new
material. The perimeter reuses the Japanese Shrine rock meshes with project-owned
purple materials. Their source assets remain unchanged. The older grass and
dirt material assets remain available for restoration.

The ground displays one 11,000 cm artwork tile across the combat region, avoiding
repeated motifs inside it. Ground tint and broad variation are material controls;
the source pixels are unchanged. Interior slab widths and heights are reduced to
76%. All 65 slab placements are retained. Two rows of 155 instanced perimeter
rocks replace the visible fence; its existing boundary collision is retained.
The new rocks have no collision or navigation influence.

- `Tools/inspect_terracotta_arena.py`: checks saved slab alignment, assigned
  materials, instance counts, hidden fence visuals and floor collision.

Validation and rendered views: `Saved/TerracottaArena`.
