BLOODSHIFT - NINJA V6 TEXTURE AND MATERIAL PASS

LATEST: UE5 PLAYABLE CHARACTER VERSION (2026-10-08)
Bloodshift_NinjaV6_UE5.blend - packed textures, 70-bone animation rig, two material slots.
Bloodshift_NinjaV6_UE5.fbx - skeletal export matching the existing NinjaCharacterV3 skeleton.
Bloodshift_NinjaV6_ScarfSeparated.blend - original A-pose, unrigged, two material slots.
The older *_Textured files below are preserved as the original surface-only version.

UE content: /Game/Assets/PlayerCharacters/Ninja/V6/
Playable blueprint: /Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja
Mesh: SK_NinjaV6
Slot 0: M_NinjaV6_Body (includes neck wrap, sleeve and waist sash)
Slot 1: M_NinjaV6_ScarfRibbons (ONLY the two long trailing neck scarf ribbons)
The original UV layout and painted textures are retained.
V6 was fitted and skinned to the existing rig; its export uses the original T-pose.
ABP_Ninja, the original skeleton, gameplay scale, sockets and animations are reused.
Chaos cloth is bound only to slot 1; both ribbons are pinned at their neck attachments.
The original V3 asset remains available. The previous BP_Ninja package is backed up in
Saved/NinjaV6Replacement/Backup/Content/HeavensDivide/Blueprints/PlayerCharacters/.
Import/rig/material reports are in Saved/NinjaV6Replacement/.

ORIGINAL TEXTURE PASS

Open Bloodshift_NinjaV6_Textured.blend in Blender 5.2 or newer.
The project includes packed textures, a front material-preview view, and studio lighting.
The original Desktop/NinjaV6 FBX and JPG were left unchanged.

DELIVERABLES
Bloodshift_NinjaV6_Textured.blend - editable Blender project with packed textures
Bloodshift_NinjaV6_Textured.glb - portable model with embedded PBR textures
Bloodshift_NinjaV6_Textured.fbx - model with embedded textures
NinjaV6_Textured_Front.png / Back.png / ThreeQuarter.png - rendered previews
Textures/ - all texture maps at 2048 x 2048, using the original UV layout
Validation.json - saved-texture and export checks

CHANGES
Deeper purple-black hair and charcoal clothing; retained painted flowers and garment details.
Warmer gold edging, corrected pale obi trim, and protected the original facial-feature placement.
Different roughness and reflectivity for cloth, skin, hair, leather/lacquer, and metal.
The input mesh's vertices, UV coordinates, proportions, and pose are preserved.

TEXTURE MAPS
T_NinjaV6_BaseColor.png - sRGB color
T_NinjaV6_Roughness.png - linear / Non-Color
T_NinjaV6_Metallic.png - linear / Non-Color
T_NinjaV6_Specular.png - linear / Non-Color; controls dielectric highlight strength
T_NinjaV6_ORM.png - linear / Non-Color; R = neutral occlusion (1), G = roughness, B = metallic
T_NinjaV6_SurfaceMasks.png - editable masks: R = skin, G = hair, B = leather/lacquer

For Unreal, connect BaseColor to Base Color and use ORM Green for Roughness and Blue for Metallic.
Disable sRGB on the ORM, roughness, metallic, specular, and mask textures.
The ORM red channel is neutral; ambient occlusion was not baked into that channel.
The existing mesh normals are retained; this package does not add a normal-map texture.

MODEL SCOPE
16,162 vertices and 17,497 source polygons. No armature was present in the supplied model.
This pass edits surface appearance. The scarf's backward angle and existing sculpted details
remain part of the input geometry and would need mesh edits to change their shapes.

REFERENCES
../Bloodshift_Ninja_Front_v2.png
../Bloodshift_Ninja_Back_v1.png

The supporting Blender scripts are in Tools/Blender/NinjaV6 within the game project.
