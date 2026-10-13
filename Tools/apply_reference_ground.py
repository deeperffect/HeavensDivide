"""Focused Level 1 ground update. Run in Unreal Python; no map/lighting writes.

Source textures live in Art/ReferenceGround; authored weights are baked with
Tools/generate_level1_ground.py. Shared arena/wall materials remain independent.
"""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
out = root / 'Saved/GroundReferencePass'
out.mkdir(parents=True, exist_ok=True)
backup = out / 'Backups' / datetime.now().strftime('%Y%m%d_%H%M%S_%f')
folder = '/Game/HeavensDivide/Materials/EnvironmentPolish'
textures_folder = '/Game/HeavensDivide/Materials/ReferenceGround'
lib = u.MaterialEditingLibrary
assets = u.AssetToolsHelpers.get_asset_tools()


def disk(asset):
    return root / 'Content' / (asset.removeprefix('/Game/') + '.uasset')


def preserve(asset):
    source = disk(asset)
    if source.exists():
        dest = backup / source.relative_to(root)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, dest)


def put(obj, **properties):
    obj.modify(True)
    for key, value in properties.items():
        obj.set_editor_property(key, value)


def save(obj):
    assert u.EditorAssetLibrary.save_loaded_asset(obj, False), obj.get_path_name()


def import_texture(source, name, destination, data=False):
    path = destination + '/' + name
    preserve(path)
    task = u.AssetImportTask()
    for key, value in dict(filename=str(source), destination_path=destination,
                           destination_name=name, automated=True, replace_existing=True,
                           replace_existing_settings=False, save=False).items():
        task.set_editor_property(key, value)
    assets.import_asset_tasks([task])
    texture = u.load_asset(path)
    assert isinstance(texture, u.Texture2D), path
    put(texture, srgb=not data,
        compression_settings=(u.TextureCompressionSettings.TC_MASKS if data else
                              u.TextureCompressionSettings.TC_DEFAULT),
        address_x=u.TextureAddress.TA_CLAMP if data else u.TextureAddress.TA_WRAP,
        address_y=u.TextureAddress.TA_CLAMP if data else u.TextureAddress.TA_WRAP,
        lod_group=u.TextureGroup.TEXTUREGROUP_WORLD,
        power_of_two_mode=u.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO,
        max_texture_size=1024 if data else 2048,
        mip_gen_settings=u.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP,
        never_stream=False)
    save(texture)
    return texture


# Hash unrelated saved assets so the report can prove this update is scoped.
protected = list((root / 'Content/HeavensDivide/Materials/StylizedGround').glob('*.uasset'))
protected += list((root / 'Content/Maps').glob('Lvl_B1_*.umap'))
protected += [disk(folder + '/MI_Arena_Stone')]
before = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
          for path in protected if path.exists()}

surfaces = {layer: import_texture(root / 'Art/ReferenceGround' / (layer + '_BaseColor.png'),
                                  'T_ReferenceGround_' + layer, textures_folder)
            for layer in ['Grass', 'Dirt', 'Stone']}
mask = import_texture(root / 'Art/EnvironmentPolish/Lvl1_GroundWeights.png',
                      'T_Lvl1_GroundWeights', folder, data=True)

preserve(folder + '/M_Lvl1_Ground')
preserve(folder + '/MI_Lvl1_Ground')
material = u.load_asset(folder + '/M_Lvl1_Ground')
instance = u.load_asset(folder + '/MI_Lvl1_Ground')
assert material and instance
expressions = lib.get_material_expressions(material)
placed_rocks = lib.get_material_instance_scalar_parameter_value(instance, 'PlacedRockStamps')
blend = next(node for node in expressions if isinstance(node, u.MaterialExpressionCustom)
             and ('automaticWeights' in node.get_editor_property('code') or
                  'REFERENCE_GROUND' in node.get_editor_property('code')))
code = '''// REFERENCE_GROUND: authored broad paths; three surface samples + layout.
// World XY maintains scale independently of the floor mesh UVs.
float2 p = Position.xy;
float2 layoutUV = (p - LayoutOrigin.xy) / max(LayoutSize.xy, 1.0);
float3 weights = max(Texture2DSample(LayoutTexture, LayoutTextureSampler, layoutUV).rgb, 0.0);
weights /= max(dot(weights, 1.0), .001);
// Independently placed slabs can supply stone while this surface supplies soil.
weights.y += weights.z * saturate(PlacedRockStamps);
weights.z *= 1.0 - saturate(PlacedRockStamps);

float2 grassUV = float2(p.x * .939693 - p.y * .342020,
                       p.x * .342020 + p.y * .939693) / max(GrassTileSize, 1.0);
float2 dirtUV = float2(p.x * .857167 + p.y * .515038,
                      -p.x * .515038 + p.y * .857167) / max(DirtTileSize, 1.0);
float2 stoneUV = float2(p.x * .956305 - p.y * .292372,
                       p.x * .292372 + p.y * .956305) / max(StoneTileSize, 1.0) + .173;
// Complete slabs stay inside a soil-edged source tile. Stable quarter-turns
// break visible repetition without mirrored shapes or translucent stone blends.
float2 stoneCell = floor(stoneUV);
float turn = floor(frac(sin(dot(stoneCell, float2(127.1,311.7))) * 43758.5453) * 4.0);
float2 localStone = frac(stoneUV);
float2 stoneDX = ddx(stoneUV), stoneDY = ddy(stoneUV);
localStone = turn < 1.0 ? localStone : (turn < 2.0 ? float2(1.0-localStone.y,localStone.x) :
             (turn < 3.0 ? 1.0-localStone : float2(localStone.y,1.0-localStone.x)));
stoneDX = turn < 1.0 ? stoneDX : (turn < 2.0 ? float2(-stoneDX.y,stoneDX.x) :
          (turn < 3.0 ? -stoneDX : float2(stoneDX.y,-stoneDX.x)));
stoneDY = turn < 1.0 ? stoneDY : (turn < 2.0 ? float2(-stoneDY.y,stoneDY.x) :
          (turn < 3.0 ? -stoneDY : float2(stoneDY.y,-stoneDY.x)));
float3 grass = Texture2DSample(GrassTexture, GrassTextureSampler, grassUV).rgb * GrassTint.rgb;
float3 dirt = Texture2DSample(DirtTexture, DirtTextureSampler, dirtUV).rgb * DirtTint.rgb;
// Explicit gradients preserve mip selection across wrapped/rotated tile borders.
float3 stone = Texture2DSampleGrad(StoneTexture, StoneTextureSampler, localStone, stoneDX, stoneDY).rgb * StoneTint.rgb;
// Level-specific compensation for the existing warm light/grade. The original
// source pixels are untouched; the material remains fully lit and shadowed.
float3 ground = grass * weights.x + dirt * weights.y + stone * weights.z;
return max(0.0, lerp(dot(ground, float3(.2126,.7152,.0722)).xxx, ground, Saturation) * Brightness);
'''
put(blend, code=code, description='Reference ground: broad painted slabs, ochre earth and grass islands')
placed_parameter = next((e for e in expressions if isinstance(e, u.MaterialExpressionScalarParameter)
                        and str(e.get_editor_property('parameter_name')) == 'PlacedRockStamps'), None)
if not placed_parameter:
    placed_parameter = lib.create_material_expression(material, u.MaterialExpressionScalarParameter, -1400, 2850)
    put(placed_parameter, parameter_name='PlacedRockStamps', default_value=0.0, group='Level layout')
pins = list(blend.get_editor_property('inputs'))
if not any(str(pin.get_editor_property('input_name')) == 'PlacedRockStamps' for pin in pins):
    pin = u.CustomInput()
    pin.set_editor_property('input_name', 'PlacedRockStamps')
    pins.append(pin)
    blend.set_editor_property('inputs', pins)
assert lib.connect_material_expressions(placed_parameter, '', blend, 'PlacedRockStamps')

# Texture defaults and instance overrides both point at project-local sources.
for name, texture in [('GrassTexture', surfaces['Grass']), ('DirtTexture', surfaces['Dirt']),
                      ('StoneTexture', surfaces['Stone']), ('LayoutTexture', mask)]:
    node = next(e for e in expressions if isinstance(e, u.MaterialExpressionTextureObjectParameter)
                and str(e.get_editor_property('parameter_name')) == name)
    put(node, texture=texture)
    lib.set_material_instance_texture_parameter_value(instance, name, texture)
    assert lib.get_material_instance_texture_parameter_value(instance, name) == texture

scalars = dict(GrassTileSize=1000.0, DirtTileSize=1500.0, StoneTileSize=2200.0,
               PlacedRockStamps=placed_rocks,
               Brightness=.65, Saturation=.90, MacroVariation=0.0,
               LayoutEdgeVariation=0.0, Roughness=1.0, Specular=.10)
vectors = dict(GrassTint=(.82, .90, 1.08, 1), DirtTint=(.80, .92, 1.14, 1),
               StoneTint=(.80, .95, 1.15, 1),
               LayoutOrigin=(-7600, -7600, 0, 0), LayoutSize=(15200, 15200, 0, 0))
for key, value in scalars.items():
    lib.set_material_instance_scalar_parameter_value(instance, key, value)
    assert abs(lib.get_material_instance_scalar_parameter_value(instance, key)-value) < .0001, key
for key, value in vectors.items():
    lib.set_material_instance_vector_parameter_value(instance, key, u.LinearColor(*value))
lib.recompile_material(material)
lib.update_material_instance(instance)
save(material)
save(instance)

after = {relative: hashlib.sha256((root / relative).read_bytes()).hexdigest() for relative in before}
assert before == after, 'Unexpected change to shared materials or saved maps'
report = dict(passed=True, backup=str(backup), instance=instance.get_path_name(),
              material=material.get_path_name(), textures={k: v.get_path_name() for k, v in surfaces.items()},
              mask=mask.get_path_name(), scalars=scalars, vectors=vectors,
              sample_count=4, previous_sample_count=6, protected_assets_unchanged=list(before),
              note='Texture sample counts from shader source, not a GPU timing measurement.')
(out / 'Applied.json').write_text(json.dumps(report, indent=2))
u.log('REFERENCE_GROUND_APPLY_OK ' + str(out / 'Applied.json'))
