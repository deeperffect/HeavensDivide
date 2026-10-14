"""Apply the user's terracotta / purple-stone reference to Level 1.

Creates project-owned terrain, slab and perimeter materials. Preserves combat
actors, original artwork, lighting and the boss arena. Saves only Level 1 and
this pass's assets, with dated backups. Run through Unreal Python.
"""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import math
import random
import shutil
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/TerracottaArena'
OUT.mkdir(parents=True,exist_ok=True)
BACKUP=OUT/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S_%f')
FOLDER='/Game/HeavensDivide/Materials/TerracottaArena'
MAP='/Game/Maps/Lvl_B1_Lvl1'
LIB=u.MaterialEditingLibrary
ASSETS=u.AssetToolsHelpers.get_asset_tools()
LEVEL=u.get_editor_subsystem(u.LevelEditorSubsystem)
ACTORS=u.get_editor_subsystem(u.EditorActorSubsystem)
TAG='TerracottaArenaRim'

def disk(path,extension='.uasset'):
    return ROOT/'Content'/(path.split('.')[0].removeprefix('/Game/')+extension)

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def preserve(path,extension='.uasset'):
    file=disk(path,extension)
    if file.exists():
        target=BACKUP/file.relative_to(ROOT)
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(file,target)

def put(obj,**properties):
    obj.modify(True)
    for key,value in properties.items():obj.set_editor_property(key,value)

def save(obj):assert u.EditorAssetLibrary.save_loaded_asset(obj,False),obj.get_path_name()
def asset(name,kind,factory):
    path=FOLDER+'/'+name
    preserve(path)
    return u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else ASSETS.create_asset(name,FOLDER,kind,factory())

def parameter(mat,name,value,index):
    if isinstance(value,u.Texture):
        node=LIB.create_material_expression(mat,u.MaterialExpressionTextureSampleParameter2D,-900,index*170)
        put(node,parameter_name=name,texture=value,sampler_type=u.MaterialSamplerType.SAMPLERTYPE_COLOR)
    else:
        kind=u.MaterialExpressionVectorParameter if isinstance(value,tuple) else u.MaterialExpressionScalarParameter
        node=LIB.create_material_expression(mat,kind,-900,index*170)
        put(node,parameter_name=name,default_value=u.LinearColor(*value) if isinstance(value,tuple) else value)
    put(node,group='Terracotta Arena')
    return node

def material(name,code,params,masked=False):
    mat=asset(name,u.Material,u.MaterialFactoryNew)
    LIB.delete_all_material_expressions(mat)
    put(mat,material_domain=u.MaterialDomain.MD_SURFACE,
        blend_mode=u.BlendMode.BLEND_MASKED if masked else u.BlendMode.BLEND_OPAQUE,
        shading_model=u.MaterialShadingModel.MSM_DEFAULT_LIT,two_sided=False)
    custom=LIB.create_material_expression(mat,u.MaterialExpressionCustom,-200,0)
    put(custom,code=code,description='Terracotta arena painted terrain' if not masked else 'Purple painted stone slabs',
        output_type=u.CustomMaterialOutputType.CMOT_FLOAT4 if masked else u.CustomMaterialOutputType.CMOT_FLOAT3)
    nodes={name:parameter(mat,name,value,i) for i,(name,value) in enumerate(params.items())}
    position=LIB.create_material_expression(mat,u.MaterialExpressionWorldPosition,-1150,-220)
    nodes['Position']=position
    texture_name='RockAtlas' if masked else 'GroundTexture'
    sample=nodes.pop(texture_name)
    # Native texture sample/UV nodes keep automatic texture streaming accurate.
    if masked:
        uv=LIB.create_material_expression(mat,u.MaterialExpressionTextureCoordinate,-1150,-420)
        offset=LIB.create_material_expression(mat,u.MaterialExpressionComponentMask,-1000,-620)
        span=LIB.create_material_expression(mat,u.MaterialExpressionComponentMask,-1000,-780)
        put(offset,r=True,g=True,b=False,a=False);put(span,r=False,g=False,b=True,a=True)
        rect=nodes.pop('AtlasUVRect')
        for mask in [offset,span]:assert LIB.connect_material_expressions(rect,'RGBA',mask,'')
        multiply=LIB.create_material_expression(mat,u.MaterialExpressionMultiply,-750,-600)
        add=LIB.create_material_expression(mat,u.MaterialExpressionAdd,-500,-600)
        assert LIB.connect_material_expressions(uv,'',multiply,'A')
        assert LIB.connect_material_expressions(span,'',multiply,'B')
        assert LIB.connect_material_expressions(multiply,'',add,'A')
        assert LIB.connect_material_expressions(offset,'',add,'B')
        assert LIB.connect_material_expressions(add,'',sample,'UVs')
    else:
        xy=LIB.create_material_expression(mat,u.MaterialExpressionComponentMask,-1000,-620)
        put(xy,r=True,g=True,b=False,a=False)
        divide=LIB.create_material_expression(mat,u.MaterialExpressionDivide,-750,-600)
        assert LIB.connect_material_expressions(position,'',xy,'')
        assert LIB.connect_material_expressions(xy,'',divide,'A')
        assert LIB.connect_material_expressions(nodes['TileSize'],'',divide,'B')
        offset=LIB.create_material_expression(mat,u.MaterialExpressionAdd,-500,-600)
        put(offset,const_b=.5)
        assert LIB.connect_material_expressions(divide,'',offset,'A')
        assert LIB.connect_material_expressions(offset,'',sample,'UVs')
    nodes['Paint']=sample
    pins=[]
    for name in nodes:
        pin=u.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
    custom.set_editor_property('inputs',pins)
    for name,node in nodes.items():
        assert LIB.connect_material_expressions(node,'RGBA' if name=='Paint' else '',custom,name),name
    assert all(LIB.get_inputs_for_material_expression(mat,custom))
    assert LIB.connect_material_property(custom,'',u.MaterialProperty.MP_BASE_COLOR)
    if masked:
        alpha=LIB.create_material_expression(mat,u.MaterialExpressionComponentMask,120,180)
        put(alpha,r=False,g=False,b=False,a=True)
        assert LIB.connect_material_expressions(custom,'',alpha,'')
        assert LIB.connect_material_property(alpha,'',u.MaterialProperty.MP_OPACITY_MASK)
    for value,prop,y in [(1.,u.MaterialProperty.MP_ROUGHNESS,300),(.05,u.MaterialProperty.MP_SPECULAR,470)]:
        node=LIB.create_material_expression(mat,u.MaterialExpressionConstant,100,y);put(node,r=value)
        assert LIB.connect_material_property(node,'',prop)
    LIB.set_base_material_usage(mat,u.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES,True)
    LIB.recompile_material(mat)
    save(mat)
    return mat

def instance(name,parent,params):
    obj=asset(name,u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew)
    LIB.set_material_instance_parent(obj,parent)
    for key,value in params.items():
        if isinstance(value,u.Texture):LIB.set_material_instance_texture_parameter_value(obj,key,value)
        elif isinstance(value,tuple):LIB.set_material_instance_vector_parameter_value(obj,key,u.LinearColor(*value))
        else:LIB.set_material_instance_scalar_parameter_value(obj,key,value)
    LIB.update_material_instance(obj)
    save(obj)
    return obj

protected=[disk('/Game/Maps/Lvl_B1_Arena','.umap'),disk('/Game/Maps/Lvl_B1_Lvl1_Lighting','.umap')]
for folder in ['ReferenceGround','StylizedGround','EnvironmentPolish']:
    protected+=list((ROOT/'Content/HeavensDivide/Materials'/folder).glob('*.uasset'))
hashes={str(p.relative_to(ROOT)):digest(p) for p in protected if p.exists()}
preserve(MAP,'.umap')
assert LEVEL.load_level(MAP)
assert LEVEL.set_current_level_by_name('Lvl_B1_Lvl1')
actors=ACTORS.get_all_level_actors()
floor=next(a for a in actors if a.get_actor_label()=='Plane' and isinstance(a,u.StaticMeshActor))
slabs=next(a for a in actors if a.get_actor_label()=='GroundRockStamps')
old_rim=[a for a in actors if a.actor_has_tag(TAG)]
assert all(a.get_actor_label()==TAG for a in old_rim)
# Existing gameplay actors and every unrelated visual actor retain their transforms.
def snapshot(items):
    return {a.get_path_name():[a.get_actor_location().x,a.get_actor_location().y,a.get_actor_location().z,a.get_actor_rotation().pitch,a.get_actor_rotation().yaw,a.get_actor_rotation().roll,a.get_actor_scale3d().x,a.get_actor_scale3d().y,a.get_actor_scale3d().z] for a in items if a not in old_rim and not a.actor_has_tag(TAG)}
before=snapshot(actors)
collision=str(floor.static_mesh_component.get_collision_profile_name())
# Replace the old fence appearance while retaining its existing collision.
fence_count=0
for existing in actors:
    for component in existing.get_components_by_class(u.StaticMeshComponent):
        mesh=component.static_mesh
        if mesh and mesh.get_name()=='SM_ShrineFence03a':
            profile=component.get_collision_profile_name()
            component.set_visibility(False,True)
            assert component.get_collision_profile_name()==profile
            fence_count+=1

source=ROOT/'Art/TerracottaArena/Terracotta_BaseColor.png'
assert source.is_file()
preserve(FOLDER+'/T_Terracotta_BaseColor')
task=u.AssetImportTask()
for key,value in dict(filename=str(source),destination_path=FOLDER,destination_name='T_Terracotta_BaseColor',automated=True,replace_existing=True,replace_existing_settings=False,save=False).items():task.set_editor_property(key,value)
ASSETS.import_asset_tasks([task])
texture=u.load_asset(FOLDER+'/T_Terracotta_BaseColor')
assert isinstance(texture,u.Texture2D)
put(texture,srgb=True,compression_settings=u.TextureCompressionSettings.TC_DEFAULT,
    address_x=u.TextureAddress.TA_WRAP,address_y=u.TextureAddress.TA_WRAP,
    lod_group=u.TextureGroup.TEXTUREGROUP_WORLD,
    power_of_two_mode=u.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO,
    max_texture_size=2048,mip_gen_settings=u.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP,never_stream=False)
save(texture)

stone_dark=(.065,.045,.105,1.)
stone_light=(.32,.235,.40,1.)
ground_params=dict(GroundTexture=texture,TileSize=11000.,Brightness=.64,
                   GroundTint=(.78,.97,1.23,1.),ColorVariation=.10,
                   BorderStart=4150.,BorderEnd=5550.,BorderStrength=.92,
                   StoneShadow=stone_dark,StoneHighlight=stone_light)
ground_code="""// TERRACOTTA_ARENA_GROUND
float2 p=Position.xy;
float3 paint=Paint.rgb;
float macro=sin(p.x*.0009+sin(p.y*.0013))*sin(p.y*.0007+.6);
float3 earth=paint*GroundTint.rgb*Brightness*(1.0+macro*ColorVariation);
float distance=max(abs(p.x),abs(p.y));
float edge=smoothstep(BorderStart,BorderEnd,distance+macro*160.0)*BorderStrength;
float shade=saturate((dot(paint,float3(.2126,.7152,.0722))-.08)/.5);
float3 stone=lerp(StoneShadow.rgb,StoneHighlight.rgb,shade)*.85;
return lerp(earth,stone,edge);
"""
ground_parent=material('M_TerracottaGround',ground_code,ground_params)
ground=instance('MI_TerracottaGround',ground_parent,ground_params)
floor.static_mesh_component.set_material(0,ground)

atlas=u.load_asset('/Game/HeavensDivide/Materials/ReferenceGround/T_RockStamps_Atlas')
regions=json.loads((ROOT/'Art/ReferenceGround/RockAtlasRegions.json').read_text())['variants']
slab_params=dict(RockAtlas=atlas,AtlasUVRect=(0.,0.,.5,.5),
                 StoneShadow=stone_dark,StoneHighlight=stone_light,StoneBrightness=.90)
slab_code="""// TERRACOTTA_ARENA_SLABS
float4 tex=Paint;
float shade=saturate((dot(tex.rgb,float3(.2126,.7152,.0722))-.07)/.28);
float3 stone=lerp(StoneShadow.rgb,StoneHighlight.rgb,shade)*StoneBrightness;
return float4(stone,tex.a);
"""
slab_parent=material('M_TerracottaSlab',slab_code,slab_params,masked=True)
slab_materials=[]
for index,region in enumerate(regions):
    params={**slab_params,'AtlasUVRect':tuple(region['normalizedUVRect'])}
    slab_materials.append(instance('MI_TerracottaSlab_'+str(index),slab_parent,params))
layout=json.loads((ROOT/'Art/ReferenceGround/RockScatterLayout.json').read_text())
slab_count=0
for component in slabs.get_components_by_class(u.HierarchicalInstancedStaticMeshComponent):
    original_name=component.get_material(0).get_name()
    index=int(original_name.rsplit('_',1)[1])
    component.set_material(0,slab_materials[index])
    placements=[p for p in layout['placements'] if int(p['variant'])==index]
    assert len(placements)==component.get_instance_count()
    for i,p in enumerate(placements):
        # Open the center; keep the large edge stones and preserve all locations.
        factor=.76 if max(abs(p['x']),abs(p['y']))<3300 else 1.0
        transform=u.Transform(location=u.Vector(p['x'],p['y'],p['z']),rotation=u.Rotator(pitch=0,yaw=p['yaw'],roll=0),scale=u.Vector(p['width']/100*factor,p['height']/100*factor,1))
        assert component.update_instance_transform(i,transform,True,True,True)
    slab_count+=len(placements)

rim_params=dict(GroundTexture=texture,TileSize=3200.,StoneShadow=stone_dark,StoneHighlight=stone_light,StoneBrightness=.85)
rim_code="""// TERRACOTTA_ARENA_RIM
float3 brush=Paint.rgb;
float shade=saturate((dot(brush,float3(.2126,.7152,.0722))-.06)/.5);
return lerp(StoneShadow.rgb,StoneHighlight.rgb,shade)*StoneBrightness;
"""
rim_parent=material('M_TerracottaRim',rim_code,rim_params)
rim_material=instance('MI_TerracottaRim',rim_parent,rim_params)
for actor in old_rim:assert ACTORS.destroy_actor(actor)
actor=ACTORS.spawn_actor_from_class(u.Actor,u.Vector(),transient=False)
actor.set_actor_label(TAG);actor.set_folder_path('Environment/Terracotta Arena')
actor.set_actor_tick_enabled(False);put(actor,tags=[u.Name(TAG)],can_be_damaged=False)
SDS=u.get_engine_subsystem(u.SubobjectDataSubsystem)
SD=u.SubobjectDataBlueprintFunctionLibrary
handles=SDS.k2_gather_subobject_data_for_instance(actor)
actor_handle=next(h for h in handles if SD.is_actor(SD.get_data(h)))
root_handle,error=SDS.add_new_subobject(u.AddNewSubobjectParams(parent_handle=actor_handle,new_class=u.SceneComponent,blueprint_context=None))
assert not str(error)
put(SD.get_associated_object(SD.get_data(root_handle)),mobility=u.ComponentMobility.STATIC)
rng=random.Random(20261014)
rocks=['SM_Rock_Ground_01a','SM_Rock_Ground_02a','SM_Rock_Ground_03a']
rim_counts=[]
for variant,name in enumerate(rocks):
    mesh=u.load_asset('/Game/Assets/Environment/JapaneseShrine/Meshes/'+name)
    assert mesh
    bounds=mesh.get_bounds(); extent=bounds.box_extent; origin=bounds.origin
    handle,error=SDS.add_new_subobject(u.AddNewSubobjectParams(parent_handle=root_handle,new_class=u.HierarchicalInstancedStaticMeshComponent,blueprint_context=None))
    assert not str(error)
    component=SD.get_associated_object(SD.get_data(handle))
    component.set_static_mesh(mesh)
    for slot in range(len(mesh.get_editor_property('static_materials'))):component.set_material(slot,rim_material)
    component.set_collision_profile_name('NoCollision');component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    component.set_editor_property('generate_overlap_events',False);component.set_component_tick_enabled(False)
    put(component,mobility=u.ComponentMobility.STATIC,can_ever_affect_navigation=False,cast_shadow=True)
    transforms=[]
    for row in range(2):
        for side in range(4):
            for i in range(20):
                if (i+side+row)%3!=variant:continue
                along=-5275+i*555+(-140 if row else 0)
                if side==1 and abs(along)<800:continue # shrine approach remains open
                edge=5350+row*450+rng.uniform(-45,45)
                width=rng.uniform(550,740);depth=rng.uniform(440,590);height=rng.uniform(140,230)+(75 if row else 0)
                center=[along,edge] if side==0 else ([-edge,along] if side==1 else ([along,-edge] if side==2 else [edge,along]))
                # Local mesh X follows the perimeter; subtract imported pivot offset.
                yaw=0. if side in (0,2) else 90.
                yaw+=rng.uniform(-5,5);radians=math.radians(yaw)
                scale=u.Vector(width/(2*extent.x),depth/(2*extent.y),height/(2*extent.z))
                ox,oy=origin.x*scale.x,origin.y*scale.y
                location=u.Vector(center[0]-(ox*math.cos(radians)-oy*math.sin(radians)),center[1]-(ox*math.sin(radians)+oy*math.cos(radians)),height*.5-45-origin.z*scale.z)
                transforms.append(u.Transform(location=location,rotation=u.Rotator(pitch=0,yaw=yaw,roll=0),scale=scale))
    component.add_instances(transforms,True,True,False)
    assert component.get_instance_count()==len(transforms)
    rim_counts.append(dict(mesh=mesh.get_path_name(),instances=len(transforms)))
assert snapshot(ACTORS.get_all_level_actors())==before,'An existing actor transform changed'
assert str(floor.static_mesh_component.get_collision_profile_name())==collision
assert LEVEL.save_current_level()
assert all(digest(ROOT/path)==value for path,value in hashes.items()),'Protected assets changed'
report=dict(passed=True,backup=str(BACKUP),map=MAP,ground=ground.get_path_name(),
            floor_collision=collision,existing_actor_transforms_preserved=True,
            slabs=slab_count,slab_center_scale=.76,rim=rim_counts,fences_hidden=fence_count,
            rim_collision=False,rim_navigation=False,rim_tick=False,
            ground_parameters={key:(value.get_path_name() if isinstance(value,u.Object) else value) for key,value in ground_params.items()},
            protected_assets=hashes,art_source=str(source.relative_to(ROOT)),art_sha256=digest(source))
(OUT/'Applied.json').write_text(json.dumps(report,indent=2))
u.log('TERRACOTTA_ARENA_APPLIED '+json.dumps(rim_counts))
