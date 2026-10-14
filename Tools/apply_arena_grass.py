"""Tune Level 1 grass, earth and organic transitions. Run in Unreal Python.

Generate Lvl1_GroundVariation with generate_arena_ground_variation.py first.
Backs up and saves only the floor material, its instance and numerical mask.
"""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import re
import shutil
import unreal as u

root=Path(u.Paths.project_dir()).resolve()
out=root/'Saved/ArenaGrass'
out.mkdir(parents=True,exist_ok=True)
backup=out/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S_%f')
folder='/Game/HeavensDivide/Materials/EnvironmentPolish/'
lib=u.MaterialEditingLibrary
material=u.load_asset(folder+'M_Lvl1_Ground')
instance=u.load_asset(folder+'MI_Lvl1_Ground')
assert material and instance

def disk(path):return root/'Content'/(path.removeprefix('/Game/')+'.uasset')
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
protected=list((root/'Content/Maps').glob('Lvl_B1_*.umap'))
protected+=list((root/'Content/HeavensDivide/Materials/ReferenceGround').glob('*.uasset'))
protected+=list((root/'Content/HeavensDivide/Materials/StylizedGround').glob('*.uasset'))
protected+=[disk(folder+'T_Lvl1_GroundWeights'),disk(folder+'MI_Arena_Stone')]
hashes={str(path.relative_to(root)):digest(path) for path in protected if path.exists()}
for name in ('M_Lvl1_Ground','MI_Lvl1_Ground','T_Lvl1_GroundVariation'):
    path=disk(folder+name)
    if not path.exists():continue
    dest=backup/path.relative_to(root)
    dest.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(path,dest)
expressions=lib.get_material_expressions(material)
blend=next(e for e in expressions if isinstance(e,u.MaterialExpressionCustom) and 'REFERENCE_GROUND' in e.get_editor_property('code'))
code=blend.get_editor_property('code')
original=code
patch="""// SOFT_ARENA_GRASS_BEGIN
// Coarser mips suppress small blades/flowers; luminance only retains broad
// painted variation. A separate color avoids the old olive/yellow flecks.
float3 grassSample = Texture2DSampleBias(GrassTexture, GrassTextureSampler, grassUV, max(0.0, GrassDetailMipBias)).rgb;
float grassShade = clamp(dot(grassSample, float3(.2126,.7152,.0722)) / .115, .55, 1.5);
float3 grass = GrassColor.rgb * GrassTint.rgb * lerp(1.0, grassShade, saturate(GrassDetailStrength));
grass *= 1.0 - (1.0 - originalGrass) * RockGrassShade;
// SOFT_ARENA_GRASS_END"""
if '// SOFT_ARENA_GRASS_BEGIN' in code:
    code=re.sub(r'// SOFT_ARENA_GRASS_BEGIN.*?// SOFT_ARENA_GRASS_END',lambda _:patch,code,flags=re.S)
else:
    line='float3 grass = Texture2DSample(GrassTexture, GrassTextureSampler, grassUV).rgb * GrassTint.rgb;'
    assert code.count(line)==1,'Grass sample changed; inspect the saved material before applying.'
    code=code.replace(line,patch)
# Replace only the earth sample and add blend controls after the saved weights.
dirt_patch="""// SOFT_ARENA_DIRT_BEGIN
float3 dirtSample = Texture2DSampleBias(DirtTexture, DirtTextureSampler, dirtUV, 1.0).rgb;
float dirtShade = clamp(1.0 + (dot(dirtSample, float3(.2126,.7152,.0722)) / .187 - 1.0) * 1.6, .60, 1.45);
float3 dirt = DirtColor.rgb * DirtTint.rgb * lerp(1.0, dirtShade, saturate(DirtDetailStrength));
// SOFT_ARENA_DIRT_END"""
line='float3 dirt = Texture2DSample(DirtTexture, DirtTextureSampler, dirtUV).rgb * DirtTint.rgb;'
if '// SOFT_ARENA_DIRT_BEGIN' in code:
    code=re.sub(r'// SOFT_ARENA_DIRT_BEGIN.*?// SOFT_ARENA_DIRT_END',lambda _:dirt_patch,code,flags=re.S)
else:
    assert code.count(line)==1
    code=code.replace(line,dirt_patch)
blend_patch="""// ORGANIC_ARENA_BLEND_BEGIN
float3 variation = Texture2DSample(VariationTexture, VariationTextureSampler, layoutUV).rgb;
float edgeNoise = variation.b * 2.0 - 1.0;
float originalGrass = weights.x;
// Break up mid-weight boundaries while leaving established meadow cores clear.
float transition = 4.0 * originalGrass * (1.0 - originalGrass);
float mixedGrass = saturate(originalGrass + edgeNoise * BlendEdgeVariation * transition);
// Irregular patches follow the placed stone footprints; never tint the stone.
float edgeGrass = smoothstep(.10, .80, variation.r) * RockEdgeGrass * smoothstep(.18, .62, variation.b);
float scatteredGrass = smoothstep(.60, .90, variation.g + edgeNoise * .12) * GrassScatter;
mixedGrass = max(mixedGrass, max(edgeGrass, scatteredGrass));
float softened = smoothstep(-BlendWidth, 1.0 + BlendWidth, mixedGrass);
weights = float3(softened, 1.0 - softened, 0.0) * (1.0 - weights.z) + float3(0,0,weights.z);
// ORGANIC_ARENA_BLEND_END"""
anchor='weights.z *= 1.0 - saturate(PlacedRockStamps);'
if '// ORGANIC_ARENA_BLEND_BEGIN' in code:
    code=re.sub(r'// ORGANIC_ARENA_BLEND_BEGIN.*?// ORGANIC_ARENA_BLEND_END',lambda _:blend_patch,code,flags=re.S)
else:
    assert code.count(anchor)==1
    code=code.replace(anchor,anchor+'\n'+blend_patch)
macro='ground *= 1.0 + (variation.g - .5) * GroundColorVariation;'
if macro not in code:
    code=code.replace('return max(0.0, lerp(',macro+'\nreturn max(0.0, lerp(')
blend.modify(True)
blend.set_editor_property('code',code)

settings={'GrassDetailStrength':.65,'GrassDetailMipBias':3.0,
          'DirtDetailStrength':.85,'BlendEdgeVariation':.55,'BlendWidth':.12,
          'RockGrassShade':.18,'RockEdgeGrass':.94,'GrassScatter':.28,'GroundColorVariation':.28}
colors={'GrassColor':(.07,.50,.13,1.0),'DirtColor':(.33,.26,.115,1.0)}
pins=list(blend.get_editor_property('inputs'))
for index,name in enumerate((*settings,*colors)):
    vector=name in colors
    kind=u.MaterialExpressionVectorParameter if vector else u.MaterialExpressionScalarParameter
    node=next((e for e in expressions if isinstance(e,kind) and str(e.get_editor_property('parameter_name'))==name),None)
    if not node:
        node=lib.create_material_expression(material,kind,-1550,3100+index*180)
        node.set_editor_property('parameter_name',name)
    node.set_editor_property('group','Grass Appearance' if name.startswith('Grass') else 'Earth and Blending')
    node.set_editor_property('default_value',u.LinearColor(*colors[name]) if vector else settings[name])
    if not any(str(pin.get_editor_property('input_name'))==name for pin in pins):
        pin=u.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
    blend.set_editor_property('inputs',pins)
    assert lib.connect_material_expressions(node,'RGB' if vector else '',blend,name)
    pins=list(blend.get_editor_property('inputs'))

source=root/'Art/EnvironmentPolish/Lvl1_GroundVariation.png'
metadata=json.loads(source.with_suffix('.json').read_text())
assert metadata['layout_sha256']==digest(root/'Art/ReferenceGround/RockScatterLayout.json'),'Regenerate the variation mask after moving slabs'
assert metadata['texture_sha256']==digest(source)
task=u.AssetImportTask()
for key,value in dict(filename=str(source),destination_path=folder.rstrip('/'),destination_name='T_Lvl1_GroundVariation',automated=True,replace_existing=True,replace_existing_settings=False,save=False).items():
    task.set_editor_property(key,value)
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture=u.load_asset(folder+'T_Lvl1_GroundVariation')
assert isinstance(texture,u.Texture2D)
for key,value in dict(srgb=False,compression_settings=u.TextureCompressionSettings.TC_MASKS,
                      address_x=u.TextureAddress.TA_CLAMP,address_y=u.TextureAddress.TA_CLAMP,
                      lod_group=u.TextureGroup.TEXTUREGROUP_WORLD,max_texture_size=1024,
                      mip_gen_settings=u.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP,never_stream=False).items():
    texture.set_editor_property(key,value)
assert u.EditorAssetLibrary.save_loaded_asset(texture,False)
node=next((e for e in expressions if isinstance(e,u.MaterialExpressionTextureObjectParameter) and str(e.get_editor_property('parameter_name'))=='VariationTexture'),None)
if not node:
    node=lib.create_material_expression(material,u.MaterialExpressionTextureObjectParameter,-1550,5200)
    node.set_editor_property('parameter_name','VariationTexture')
    node.set_editor_property('group','Earth and Blending')
node.set_editor_property('texture',texture)
if not any(str(pin.get_editor_property('input_name'))=='VariationTexture' for pin in pins):
    pin=u.CustomInput();pin.set_editor_property('input_name','VariationTexture');pins.append(pin)
blend.set_editor_property('inputs',pins)
assert lib.connect_material_expressions(node,'',blend,'VariationTexture')
# Setting the input array copies structs; retain every connection before compile.
assert all(lib.get_inputs_for_material_expression(material,blend)), 'An input is disconnected'
lib.recompile_material(material)
for name,value in settings.items():
    lib.set_material_instance_scalar_parameter_value(instance,name,value)
    assert abs(lib.get_material_instance_scalar_parameter_value(instance,name)-value)<.0001,name
for name,value in colors.items():
    lib.set_material_instance_vector_parameter_value(instance,name,u.LinearColor(*value))
lib.set_material_instance_texture_parameter_value(instance,'VariationTexture',texture)
assert lib.get_material_instance_texture_parameter_value(instance,'VariationTexture')==texture
lib.update_material_instance(instance)
assert u.EditorAssetLibrary.save_loaded_asset(material,False)
assert u.EditorAssetLibrary.save_loaded_asset(instance,False)
assert all(digest(root/path)==value for path,value in hashes.items()),'Protected scene or source assets changed'
report={'saved':True,'backup':str(backup),'material':material.get_path_name(),'instance':instance.get_path_name(),'settings':settings,'colors':colors,'protected_assets':hashes,'scope':'Grass and dirt appearance with numerical rock-edge and transition masks; source artwork, stone stamps, authored layout, maps and lighting preserved.'}
(out/'Applied.json').write_text(json.dumps(report,indent=2))
u.log('ARENA_GRASS_APPLIED '+json.dumps(settings))
