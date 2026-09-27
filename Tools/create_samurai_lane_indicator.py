"""Restore the Samurai memory lane material and bind it to the trial Blueprint.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
Procedural, texture-free surface material for the existing lane footprint meshes.
"""
from pathlib import Path
import json
import shutil
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
FOLDER = '/Game/HeavensDivide/Materials'
NAME = 'M_SamuraiLaneIndicator'
BP_PATH = '/Game/HeavensDivide/Blueprints/Objectives/BP_SamuraiTechniqueTrial'
MI_PATH = FOLDER + '/MI_SamuraiTrialIndicator'
lib = u.MaterialEditingLibrary

def backup(path):
    source = ROOT / 'Content' / (path.removeprefix('/Game/') + '.uasset')
    dest = ROOT / 'Saved/Backups/SamuraiLaneIndicator' / source.name
    if source.exists() and not dest.exists():
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, dest)

for path in [FOLDER + '/' + NAME, MI_PATH, BP_PATH]:
    backup(path)

material = u.load_asset(FOLDER + '/' + NAME) if u.EditorAssetLibrary.does_asset_exist(FOLDER + '/' + NAME) else None
if not material:
    material = u.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, u.Material, u.MaterialFactoryNew())
assert material
lib.delete_all_material_expressions(material)
material.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property('two_sided', True)

def node(cls, x, y):
    return lib.create_material_expression(material, cls, x, y)

def scalar(name, value, y):
    n = node(u.MaterialExpressionScalarParameter, -600, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', value)
    return n

uv = node(u.MaterialExpressionTextureCoordinate, -600, -200)
time = node(u.MaterialExpressionTime, -600, -50)
fill = scalar('FillAmount', 0.0, 100)
aspect = scalar('LaneAspect', 380.0 / 1120.0, 250)
color = node(u.MaterialExpressionVectorParameter, -600, 400)
color.set_editor_property('parameter_name', 'FillColor')
color.set_editor_property('default_value', u.LinearColor(1, .02, .01, 1))
custom = node(u.MaterialExpressionCustom, -100, 0)
custom.set_editor_property('description', 'Black ink lane / curling crimson rim / advancing red fill')
custom.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT4)
inputs = []
for name in ['UV', 'Clock', 'Fill', 'Aspect', 'Tint']:
    item = u.CustomInput()
    item.set_editor_property('input_name', name)
    inputs.append(item)
custom.set_editor_property('inputs', inputs)
custom.set_editor_property('code', r'''
float aspect = max(Aspect, 0.05);
float2 p = (UV - 0.5) * float2(aspect, 1.0);
float t = Clock * 1.5;
// Unequal traveling waves produce a curling ink boundary, not a rigid box.
float side = 0.0045 * sin(p.y * 47.0 + t * 2.1)
           + 0.0025 * sin(p.y * 103.0 - t * 1.7 + sin(p.y * 29.0 + t));
float cap = 0.0035 * sin(p.x * 83.0 - t * 1.8)
          + 0.0015 * sin(p.x * 161.0 + t * 2.7);
float d = max(abs(p.x) - (aspect * 0.5 - 0.013) + side,
              abs(p.y) - 0.484 + cap);
float aa = max(fwidth(d), 0.0008);
float silhouette = 1.0 - smoothstep(-aa, aa, d);
float rim = 1.0 - smoothstep(0.003, 0.009, abs(d + 0.004));
float fringe = exp(-abs(d + 0.011) * 140.0)
             * pow(0.5 + 0.5 * sin(p.y * 76.0 + p.x * 101.0 - t * 3.0), 5.0);
float progress = saturate(Fill);
float front = progress + 0.006 * sin(p.x * 110.0 + t) * sin(progress * 3.141593);
float flooded = 1.0 - smoothstep(front - 0.004, front + 0.004, UV.y);
flooded *= step(0.0001, progress);
flooded = lerp(flooded, 1.0, step(0.9999, progress));
float leadingEdge = exp(-abs(UV.y - front) * 200.0) * step(0.001, progress) * (1.0 - step(0.999, progress));
float3 ink = float3(0.001, 0.0001, 0.0002);
float3 red = Tint.rgb;
float3 border = float3(1.0, 0.004, 0.085);
float3 rgb = lerp(ink, red * 0.65, flooded);
rgb = lerp(rgb, border * 2.5, rim);
rgb += border * fringe * 0.45 + red * leadingEdge * 0.8;
return float4(rgb, silhouette * 0.98);
''')
for source, pin in [(uv, 'UV'), (time, 'Clock'), (fill, 'Fill'), (aspect, 'Aspect'), (color, 'Tint')]:
    assert lib.connect_material_expressions(source, '', custom, pin)
rgb = node(u.MaterialExpressionComponentMask, 220, 0)
alpha = node(u.MaterialExpressionComponentMask, 220, 160)
for channel in ['r', 'g', 'b', 'a']:
    rgb.set_editor_property(channel, channel != 'a')
    alpha.set_editor_property(channel, channel == 'a')
assert lib.connect_material_expressions(custom, '', rgb, '')
assert lib.connect_material_expressions(custom, '', alpha, '')
assert lib.connect_material_property(rgb, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
assert lib.connect_material_property(alpha, '', u.MaterialProperty.MP_OPACITY)
lib.recompile_material(material)
assert u.EditorAssetLibrary.save_loaded_asset(material, False)

instance = u.load_asset(MI_PATH)
if not instance:
    instance = u.AssetToolsHelpers.get_asset_tools().create_asset('MI_SamuraiTrialIndicator', FOLDER, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
lib.set_material_instance_parent(instance, material)
lib.set_material_instance_scalar_parameter_value(instance, 'FillAmount', 0.0)
lib.set_material_instance_scalar_parameter_value(instance, 'LaneAspect', 380.0 / 1120.0)
lib.update_material_instance(instance)
assert u.EditorAssetLibrary.save_loaded_asset(instance, False)

bp = u.load_asset(BP_PATH)
defaults = u.get_default_object(u.EditorAssetLibrary.load_blueprint_class(BP_PATH))
defaults.set_editor_property('lane_indicator_material', instance)
tick = defaults.get_editor_property('primary_actor_tick')
tick.set_editor_property('start_with_tick_enabled', True)
defaults.set_editor_property('primary_actor_tick', tick)
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
assert u.get_default_object(bp.generated_class()).get_editor_property('lane_indicator_material') == instance

report = {'material': material.get_path_name(), 'instance': instance.get_path_name(), 'blueprint': BP_PATH}
output = ROOT / 'Saved/SamuraiLaneIndicator'
output.mkdir(parents=True, exist_ok=True)
(output / 'setup.json').write_text(json.dumps(report, indent=2))
u.log('SAMURAI_LANE_INDICATOR_READY ' + json.dumps(report))
