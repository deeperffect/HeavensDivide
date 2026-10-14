"""Build the layered material in its own folder, retaining the restored originals.

Called by apply_layered_ground.py after its backups; no map is saved here.
"""
from pathlib import Path
import hashlib
import json
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve()
FOLDER='/Game/HeavensDivide/Materials/LayeredGround'
SOURCE='/Game/HeavensDivide/Materials/EnvironmentPolish'
LIB=u.MaterialEditingLibrary
EAL=u.EditorAssetLibrary
ASSETS=u.AssetToolsHelpers.get_asset_tools()
layout=json.loads((ROOT/'Art/LayeredGround/RockScatterLayout.json').read_text())
meta=json.loads((ROOT/'Art/LayeredGround/GroundBlend.json').read_text())
assert meta['layout_sha256']==hashlib.sha256((ROOT/'Art/LayeredGround/RockScatterLayout.json').read_bytes()).hexdigest()
assert meta['sha256']==hashlib.sha256((ROOT/'Art/LayeredGround/GroundBlend.png').read_bytes()).hexdigest()
EAL.make_directory(FOLDER)

def copy_asset(name):
    path=FOLDER+'/'+name
    if not EAL.does_asset_exist(path):
        assert EAL.duplicate_asset(SOURCE+'/'+name,path)
    return u.load_asset(path)

material=copy_asset('M_Lvl1_Ground')
instance=copy_asset('MI_Lvl1_Ground')
LIB.set_material_instance_parent(instance,material)
expressions=LIB.get_material_expressions(material)
blend=next(e for e in expressions if isinstance(e,u.MaterialExpressionCustom) and
           ('REFERENCE_GROUND' in e.get_editor_property('code') or 'LAYERED_GROUND' in e.get_editor_property('code')))
task=u.AssetImportTask()
for key,value in dict(filename=str(ROOT/'Art/LayeredGround/GroundBlend.png'),destination_path=FOLDER,
        destination_name='T_GroundBlend',automated=True,replace_existing=True,save=False).items():
    task.set_editor_property(key,value)
ASSETS.import_asset_tasks([task])
texture=u.load_asset(FOLDER+'/T_GroundBlend')
for key,value in dict(srgb=False,compression_settings=u.TextureCompressionSettings.TC_MASKS,
        compression_no_alpha=False,address_x=u.TextureAddress.TA_CLAMP,address_y=u.TextureAddress.TA_CLAMP,
        max_texture_size=meta['size'],mip_gen_settings=u.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP,
        never_stream=False).items():
    texture.set_editor_property(key,value)
assert EAL.save_loaded_asset(texture,False)

settings={'RockEdgeGrass':.92,'ContactShade':.22,'BlendEdgeVariation':.27,
          'BlendWidth':.16,'GroundColorVariation':.12,'RepeatVariation':.65}
pins=list(blend.get_editor_property('inputs'))
for index,(name,value) in enumerate(settings.items()):
    node=next((e for e in expressions if isinstance(e,u.MaterialExpressionScalarParameter) and str(e.get_editor_property('parameter_name'))==name),None)
    if not node:
        node=LIB.create_material_expression(material,u.MaterialExpressionScalarParameter,-1600,3100+index*180)
        node.set_editor_property('parameter_name',name)
    node.set_editor_property('group','Layered Ground')
    node.set_editor_property('default_value',value)
    if not any(str(p.get_editor_property('input_name'))==name for p in pins):
        pin=u.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
    blend.set_editor_property('inputs',pins)
    assert LIB.connect_material_expressions(node,'',blend,name)
    pins=list(blend.get_editor_property('inputs'))
    LIB.set_material_instance_scalar_parameter_value(instance,name,value)
name='BlendTexture'
node=next((e for e in expressions if isinstance(e,u.MaterialExpressionTextureObjectParameter) and str(e.get_editor_property('parameter_name'))==name),None)
if not node:
    node=LIB.create_material_expression(material,u.MaterialExpressionTextureObjectParameter,-1600,4600)
    node.set_editor_property('parameter_name',name)
node.set_editor_property('texture',texture)
if not any(str(p.get_editor_property('input_name'))==name for p in pins):
    pin=u.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
blend.set_editor_property('inputs',pins)
assert LIB.connect_material_expressions(node,'',blend,name)
LIB.set_material_instance_texture_parameter_value(instance,name,texture)
code=(ROOT/'Tools/layered_ground.hlsl').read_text()
blend.set_editor_property('code',code)
blend.set_editor_property('description','LAYERED_GROUND: fixed world tiles; silhouette grass and contact shade')
assert all(LIB.get_inputs_for_material_expression(material,blend))
for name,value in [('GrassTileSize',1000.),('DirtTileSize',1500.),('PlacedRockStamps',1.)]:
    LIB.set_material_instance_scalar_parameter_value(instance,name,value)
for name,value in [('LayoutOrigin',(*meta['world_origin'],0,0)),('LayoutSize',(*meta['world_size'],0,0))]:
    LIB.set_material_instance_vector_parameter_value(instance,name,u.LinearColor(*value))
LIB.recompile_material(material)
LIB.update_material_instance(instance)
assert EAL.save_loaded_asset(material,False)
assert EAL.save_loaded_asset(instance,False)
