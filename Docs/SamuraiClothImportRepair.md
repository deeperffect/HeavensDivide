# Samurai two-material mesh repair (2026-09-28)

Update 2026-10-08: cloth simulation has since been authored on the repaired mesh.
See [CharacterClothAndHair.md](CharacterClothAndHair.md) for the active setup.
The notes below describe the earlier import repair.

Active mesh: `/Game/Assets/PlayerCharacters/Samurai/fdsafdsa`.
Retained slots: `Fbx_Default_Material_0` and `SamuraiClothes`.
Animation skeleton: `SamuraiCharacterV4_Skeleton`.

The Maya round trip introduced `SamuraiCharacterV4` as an extra root above
the original `root`, and a second `SamuraiCharacterV41` bone. The 28 actual
rig bones retained their original bind transforms. Replacing skeleton references
with this different hierarchy broke the existing animations.

Removed the two added bones through SkeletonModifier (including its skin-weight
index remapping), promoted the original root, assigned the original skeleton and
physics asset, and restored the original animation/AnimBlueprint references.
The new geometry and material-slot split remain. No cloth asset was present on
the imported mesh; cloth simulation still needs to be created/configured separately.

Saved asset reload and Blueprint compilation passed. Idle, forward-run and
counterattack animation samples on the new mesh matched the old mesh's head,
hands, feet and hips within 0.004 cm, and changed correctly over time. Ninja and
shared-character assets were verified unchanged by hash. This validates skeletal
playback; cloth simulation itself was not added or tested.

For subsequent imports, preserve the original root hierarchy, exclude the Maya
wrapper/mesh transforms from the exported rig, and reuse the existing animation
skeleton instead of deleting/replacing it. A material-slot split does not require
a new animation skeleton.

Pre-repair backups: `Saved/Backups/SamuraiClothRepair20260928`.
Inspection and pose-test reports: `Saved/SamuraiClothRepair`.
The temporary original comparison mesh was removed after validation.
