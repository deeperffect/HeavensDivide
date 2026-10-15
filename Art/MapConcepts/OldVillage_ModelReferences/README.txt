OLD VILLAGE - MODEL REFERENCES

Open index.html for a searchable visual catalog, or browse the numbered folders.
Each asset has its own PNG with one isolated three-quarter view. These are concept
references for creating models, not mesh files or engineering drawings. The
exaggerated hand-painted shrine B01 defines the current art direction; the older
environment image in 00_Reference supplies the layout and asset inventory.

FOLDERS
01_Buildings: 8 complete building designs.
02_Building_Modules: 5 optional wall, door, window and roof pieces.
03_Shrine_And_Landmarks: 8 shrine and ceremonial pieces.
04_Walls_And_Boundaries: 8 reusable wall, fence and waterfront pieces.
05_Props: 14 village, lighting and training props.
06_Vegetation: 9 tree, shrub, grass and aquatic planting assets.
07_Rocks_And_Ground: 8 rock, paving and pond assets.
00_Reference: layout image, art direction, exact prompts and verification results.

ASSEMBLY
Use B01-B08 as complete models, or use M01-M05 to build variations. The module
images are design references; align their actual dimensions and snap points when
modeling. They are not mechanically verified to fit together as pictured.

Combine S02 (standing stone) with S03 (circular base) for the central shrine.
S04 belongs in the northern courtyard. Reuse S05 on both sides of the main steps.
Use S08 for separate raised approaches; complete buildings already include steps.

Repeat W01 along the waterfront, join corners with W02, add W08 where needed,
and dress the lower edge with G07. W01 and W02 show timber railings on top; model
those as separable submeshes if you want both bare and railed wall variants.
Fence panels W03 can be rotated for corners;
W04 provides end posts. W06 encloses the shrine, W07 defines low garden edges.
Keep broad openings for the circulation routes visible in the environment concept.

The pond G06 is a dry basin reference. Add a separate flat water surface, V09 lily
pads and P14 water spout. Rotate and resize G01-G05 moderately for variation.
Place vegetation mostly near boundaries; preserve large open combat clearings.
P12 sits inside B07. P05-P08 and P13 dress building walls; P10-P11 dress training
areas. Buildings may include small integral architectural fittings; separate props
provide reusable alternatives rather than requiring every copy to be identical.

NON-MODEL WORK
The connected map ground should be an Unreal Landscape or simple terrain mesh
with painted dirt, grass and stone materials. Water, candle/fire effects, foliage
wind and lighting need materials or effects in addition to these model references.
Grass coverage and most moss should be painted into materials; V07 is a sparse
accent mesh. The reference set does not generate textures or change Unreal maps.

PROVENANCE
Generated using the built-in image_gen tool. Each asset's exact prompt is in
00_Reference/Prompts and AssetManifest.json. Original generated PNGs are copied
without pixel editing. Suggested sizes in the manifest and checklist are authored
starting points and may be adjusted for the game's camera and collision scale.

Run python 00_Reference/build_catalog.py from this folder to rebuild the catalog
and verify PNG integrity. Generated source paths and SHA-256 hashes are recorded
in AssetManifest.json.
