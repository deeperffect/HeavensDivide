"""Replace isolated stone stamps with a continuous world-scaled painted surface.

Run in Unreal Editor Python with the interactive editor closed. Backs up the map
and owned assets. Old artwork/instances, scene geometry and lighting are retained.
"""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import runpy
import shutil
import struct
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/ConnectedGround'; OUT.mkdir(parents=True,exist_ok=True)
BACKUP=OUT/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S_%f')
FOLDER='/Game/HeavensDivide/Materials/ConnectedGround'
MAP='/Game/Maps/Lvl_B1_Lvl1'
CONFIG=json.loads((ROOT/'Tools/connected_ground.json').read_text())
LIB=u.MaterialEditingLibrary
EAL=u.EditorAssetLibrary
ASSETS=u.AssetToolsHelpers.get_asset_tools()
LEVEL=u.get_editor_subsystem(u.LevelEditorSubsystem)
ACTORS=u.get_editor_subsystem(u.EditorActorSubsystem)

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def disk(path,ext='.uasset'):return ROOT/'Content'/(path.split('.')[0].removeprefix('/Game/')+ext)
def backup(path,ext='.uasset'):
    source=disk(path,ext)
    if source.exists():
        dest=BACKUP/source.relative_to(ROOT);dest.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,dest)
def put(obj,**values):
    obj.modify(True)
    for name,value in values.items():obj.set_editor_property(name,value)
def save(obj):assert EAL.save_loaded_asset(obj,False),obj.get_path_name()
def asset(name,kind,factory):
    path=FOLDER+'/'+name;backup(path)
    return u.load_asset(path) if EAL.does_asset_exist(path) else ASSETS.create_asset(name,FOLDER,kind,factory())
def node(mat,kind,**properties):
    result=LIB.create_material_expression(mat,kind);put(result,**properties);return result
def link(a,b,pin='',out=''):assert LIB.connect_material_expressions(a,out,b,pin)
def scalar(mat,name):
    return node(mat,u.MaterialExpressionScalarParameter,parameter_name=name,
                default_value=float(CONFIG[name]),group='Connected Ground')

protected=[p for p in (ROOT/'Content/Maps').glob('*.umap') if p.name!='Lvl_B1_Lvl1.umap']
for folder in ['EnvironmentPolish','ReferenceGround','StylizedGround','TerracottaArena','ArenaVariations','LayeredGround']:
    protected+=list((ROOT/'Content/HeavensDivide/Materials'/folder).glob('*.uasset'))
hashes={str(p.relative_to(ROOT)):digest(p) for p in protected}
backup(MAP,'.umap')
EAL.make_directory(FOLDER)
source=ROOT/'Art/ConnectedGround/ConnectedSlabs_BaseColor.png'
backup(FOLDER+'/T_ConnectedSlabs')
task=u.AssetImportTask()
for key,value in dict(filename=str(source),destination_path=FOLDER,destination_name='T_ConnectedSlabs',
                       automated=True,replace_existing=True,replace_existing_settings=False,save=False).items():
    task.set_editor_property(key,value)
ASSETS.import_asset_tasks([task])
texture=u.load_asset(FOLDER+'/T_ConnectedSlabs')
assert isinstance(texture,u.Texture2D)
put(texture,srgb=True,compression_settings=u.TextureCompressionSettings.TC_DEFAULT,
    address_x=u.TextureAddress.TA_CLAMP,address_y=u.TextureAddress.TA_CLAMP,
    lod_group=u.TextureGroup.TEXTUREGROUP_WORLD,
    power_of_two_mode=u.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO,
    max_texture_size=2048,mip_gen_settings=u.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP,
    never_stream=False)
save(texture)
mat=asset('M_ConnectedGround',u.Material,u.MaterialFactoryNew)
LIB.delete_all_material_expressions(mat)
put(mat,material_domain=u.MaterialDomain.MD_SURFACE,blend_mode=u.BlendMode.BLEND_OPAQUE,
    shading_model=u.MaterialShadingModel.MSM_DEFAULT_LIT)
world=node(mat,u.MaterialExpressionWorldPosition)
xy=node(mat,u.MaterialExpressionComponentMask,r=True,g=True,b=False,a=False);link(world,xy)
uv=node(mat,u.MaterialExpressionDivide);link(xy,uv,'A');link(scalar(mat,'TileSize'),uv,'B')
# Fixed world-centimeter coordinates: scaling the floor only exposes more ground.
# Interior patches avoid nonmatching source borders; source UVs stay in [0,1].
lattice='''float2 skew=float2(UV.x-.57735027*UV.y,1.15470054*UV.y)*3.;
float2 cell=floor(skew),f=frac(skew);
float3 weights; float2 v0,v1,v2;
if(f.x+f.y<1.) {weights=float3(1.-f.x-f.y,f.x,f.y);v0=cell;v1=cell+float2(1,0);v2=cell+float2(0,1);}
else {weights=float3(f.x+f.y-1.,1.-f.x,1.-f.y);v0=cell+1.;v1=cell+float2(0,1);v2=cell+float2(1,0);}
'''
def uv_custom(code,output,derivative=None):
    custom=node(mat,u.MaterialExpressionCustom,code=code,output_type=output)
    names=['UV']+(['D'] if derivative is not None else [])
    pins=[]
    for name in names:
        pin=u.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
    custom.set_editor_property('inputs',pins);link(uv,custom,'UV')
    if derivative is not None:link(derivative,custom,'D')
    return custom
weights=uv_custom(lattice+'return max(weights,0.);',u.CustomMaterialOutputType.CMOT_FLOAT3)
power=node(mat,u.MaterialExpressionPower)
link(node(mat,u.MaterialExpressionConstant,r=2.),power);link(scalar(mat,'DetailMipBias'),power,'Exp')
derivatives=[]
for kind in [u.MaterialExpressionDDX,u.MaterialExpressionDDY]:
    d=node(mat,kind);link(uv,d)
    scaled=node(mat,u.MaterialExpressionMultiply);link(d,scaled,'A');link(power,scaled,'B');derivatives.append(scaled)
samples=[]
for i in range(3):
    randomize=lattice+('float2 v=v%d; float2 h=frac(sin(float2(dot(v,float2(127.1,311.7)),dot(v,float2(269.5,183.3))))*43758.5453); float rotation=floor(h.x*4.);'%i)
    def orient(q):return 'float2 q='+q+'; q=rotation<.5?q:(rotation<1.5?float2(-q.y,q.x):(rotation<2.5?-q:float2(q.y,-q.x)));'
    coords=uv_custom(randomize+orient('UV-float2(v.x+.5*v.y,.8660254*v.y)/3.')+'return q+.5+(h-.5)*.24;',u.CustomMaterialOutputType.CMOT_FLOAT2)
    rotated=[uv_custom(randomize+orient('D')+'return q;',u.CustomMaterialOutputType.CMOT_FLOAT2,d) for d in derivatives]
    sample=node(mat,u.MaterialExpressionTextureSampleParameter2D,parameter_name='GroundTexture',texture=texture,
                sampler_type=u.MaterialSamplerType.SAMPLERTYPE_COLOR,mip_value_mode=u.TextureMipValueMode.TMVM_DERIVATIVE)
    link(coords,sample,'UVs');link(rotated[0],sample,'DDX(UVs)');link(rotated[1],sample,'DDY(UVs)');samples.append(sample)
inputs=dict(PaintA=samples[0],PaintB=samples[1],PaintC=samples[2],Weights=weights,Position=world)
for name in ['Brightness','Saturation','ColorVariation','PatchBlendWidth','SeamPreservation']:
    inputs[name]=scalar(mat,name)
inputs['GroundTint']=node(mat,u.MaterialExpressionVectorParameter,parameter_name='GroundTint',
                         default_value=u.LinearColor(*CONFIG['GroundTint']),group='Connected Ground')
custom=node(mat,u.MaterialExpressionCustom,code=(ROOT/'Tools/connected_ground.hlsl').read_text(),
            description='Connected olive slabs, earth and grass seams at fixed world scale',
            output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
pins=[]
for name in inputs:
    pin=u.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
custom.set_editor_property('inputs',pins)
for name,obj in inputs.items():link(obj,custom,name)
assert all(LIB.get_inputs_for_material_expression(mat,custom))
assert LIB.connect_material_property(custom,'',u.MaterialProperty.MP_BASE_COLOR)
for value,prop in [(1.,u.MaterialProperty.MP_ROUGHNESS),(.025,u.MaterialProperty.MP_SPECULAR)]:
    assert LIB.connect_material_property(node(mat,u.MaterialExpressionConstant,r=value),'',prop)
LIB.layout_material_expressions(mat);LIB.recompile_material(mat);save(mat)
instance=asset('MI_ConnectedGround',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew)
LIB.set_material_instance_parent(instance,mat)
LIB.set_material_instance_texture_parameter_value(instance,'GroundTexture',texture)
for name,value in CONFIG.items():
    if isinstance(value,list):LIB.set_material_instance_vector_parameter_value(instance,name,u.LinearColor(*value))
    else:LIB.set_material_instance_scalar_parameter_value(instance,name,float(value))
LIB.update_material_instance(instance);save(instance)
world_obj=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
u.SystemLibrary.execute_console_command(world_obj,'BuildMaterialTextureStreamingData')
save(mat);save(instance)

assert LEVEL.load_level(MAP)
assert LEVEL.set_current_level_by_name('Lvl_B1_Lvl1')
scene=runpy.run_path(str(ROOT/'Tools/inspect_layered_ground.py'),init_globals={'SNAPSHOT_ONLY':True})['REPORT']
actors=ACTORS.get_all_level_actors()
floor=next(a for a in actors if a.get_actor_label()=='Plane' and isinstance(a,u.StaticMeshActor))
stamps=next(a for a in actors if a.get_actor_label()=='GroundRockStamps')
previous=floor.static_mesh_component.get_material(0).get_path_name()
components=stamps.get_components_by_class(u.HierarchicalInstancedStaticMeshComponent)
counts=[c.get_instance_count() for c in components]
floor.modify(True);floor.static_mesh_component.modify(True)
floor.static_mesh_component.set_material(0,instance)
stamps.modify(True)
for c in components:
    c.modify(True);c.set_visibility(False,True);c.set_hidden_in_game(True,True)
assert scene==runpy.run_path(str(ROOT/'Tools/inspect_layered_ground.py'),init_globals={'SNAPSHOT_ONLY':True})['REPORT']
assert counts==[c.get_instance_count() for c in components]
assert LEVEL.save_current_level()
assert all(digest(ROOT/p)==h for p,h in hashes.items()),'A protected asset changed'
w,h=struct.unpack('>II',source.read_bytes()[16:24])
report=dict(passed=True,map=MAP,backup=str(BACKUP),previous_material=previous,material=instance.get_path_name(),
            config=CONFIG,source_sha256=digest(source),native_pixels=[w,h],source_texels_per_cm=w/CONFIG['TileSize'],
            scene_snapshot=scene,protected_assets=hashes,retained_hidden_stamp_counts=counts,
            floor_collision=str(floor.static_mesh_component.get_collision_profile_name()))
(OUT/'Applied.json').write_text(json.dumps(report,indent=2)+'\n')
u.log('CONNECTED_GROUND_APPLIED '+str(OUT/'Applied.json'))
