"""Create the Ember Circle's clockwise blue-to-yellow progress material.

Run with UnrealEditor-Cmd -run=pythonscript -script=Tools/create_ember_circle_progress.py.
The shared enemy/wall ring material is preserved.
"""
from pathlib import Path
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
folder = '/Game/HeavensDivide/Materials'
name = 'M_EmberCircleProgress'
path = folder + '/' + name
source = root / 'Content/HeavensDivide/Materials' / (name + '.uasset')
backup = root / 'Saved/ObjectiveProgress/Backup' / (name + '.uasset')
if source.exists() and not backup.exists():
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, backup)

lib = u.MaterialEditingLibrary
material = u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, u.Material, u.MaterialFactoryNew())
assert material
lib.delete_all_material_expressions(material)
material.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property('two_sided', True)
uv = lib.create_material_expression(material, u.MaterialExpressionTextureCoordinate, -500, 0)
progress = lib.create_material_expression(material, u.MaterialExpressionScalarParameter, -500, 150)
progress.set_editor_property('parameter_name', 'Progress')
progress.set_editor_property('default_value', 0.0)
ring = lib.create_material_expression(material, u.MaterialExpressionCustom, -200, 0)
inputs = []
for name in ['UV', 'Progress']:
    pin = u.CustomInput()
    pin.set_editor_property('input_name', name)
    inputs.append(pin)
ring.set_editor_property('inputs', inputs)
ring.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT4)
ring.set_editor_property('code', '''
float2 p=(UV-.5)*2;
float r=length(p);
float a=atan2(p.y,p.x);
float rim=step(.81,r)*step(r,.94);
float ticks=step(.6,cos(a*8))*step(.65,r)*step(r,.8);
// Begin at twelve o'clock and replace blue with yellow clockwise.
float turn=frac((a+1.570796327)/6.283185307+1);
float fill=saturate(Progress);
float yellow=fill<=0 ? 0 : (fill>=1 ? 1 : 1-smoothstep(fill-.0015,fill+.0015,turn));
float3 color=lerp(float3(.035,.30,1),float3(1,.78,.025),yellow);
return float4(color*1.3,max(rim,ticks)*.8);
''')
assert lib.connect_material_expressions(uv, '', ring, 'UV')
assert lib.connect_material_expressions(progress, '', ring, 'Progress')
for alpha, prop in [(False, u.MaterialProperty.MP_EMISSIVE_COLOR), (True, u.MaterialProperty.MP_OPACITY)]:
    mask = lib.create_material_expression(material, u.MaterialExpressionComponentMask, 100, 150 if alpha else 0)
    for channel in ['r', 'g', 'b']:
        mask.set_editor_property(channel, not alpha)
    mask.set_editor_property('a', alpha)
    assert lib.connect_material_expressions(ring, '', mask, '')
    assert lib.connect_material_property(mask, '', prop)
lib.recompile_material(material)
assert u.EditorAssetLibrary.save_loaded_asset(material, False)
u.log('EMBER_CIRCLE_PROGRESS_MATERIAL_SAVED ' + path)
