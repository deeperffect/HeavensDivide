"""Import ten arena floor/rim presets and apply one to Level1, with backups.
Run in Unreal Editor Python. Optional VARIANT_ID chooses a preset.
Source textures are preserved at their native pixel count in Art/ArenaVariations.
"""
from pathlib import Path
from datetime import datetime
import hashlib, json, math, random, shutil, struct
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/ArenaVariations'; OUT.mkdir(parents=True,exist_ok=True)
BACKUP=OUT/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S_%f')
FOLDER='/Game/HeavensDivide/Materials/ArenaVariations'
MAP='/Game/Maps/Lvl_B1_Lvl1'
CONFIG=json.loads((ROOT/'Art/ArenaVariations/Variants.json').read_text())
SELECTED=globals().get('VARIANT_ID',CONFIG['default'])
assert SELECTED in [v['id'] for v in CONFIG['variants']]
LIB=u.MaterialEditingLibrary
ASSETS=u.AssetToolsHelpers.get_asset_tools()
LEVEL=u.get_editor_subsystem(u.LevelEditorSubsystem)
ACTORS=u.get_editor_subsystem(u.EditorActorSubsystem)
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def disk(path,ext='.uasset'):return ROOT/'Content'/(path.split('.')[0].removeprefix('/Game/')+ext)
def backup(path,ext='.uasset'):
    file=disk(path,ext)
    if file.exists():
        dest=BACKUP/file.relative_to(ROOT);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,dest)
def put(obj,**kwargs):
    obj.modify(True)
    for k,v in kwargs.items():obj.set_editor_property(k,v)
def save(obj):assert u.EditorAssetLibrary.save_loaded_asset(obj,False),obj.get_path_name()
def asset(name,kind,factory):
    path=FOLDER+'/'+name;backup(path)
    return u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else ASSETS.create_asset(name,FOLDER,kind,factory())
def node(mat,kind,**kwargs):
    obj=LIB.create_material_expression(mat,kind);put(obj,**kwargs);return obj
def link(a,b,pin='',out=''):assert LIB.connect_material_expressions(a,out,b,pin)
def scalar(mat,name,value):return node(mat,u.MaterialExpressionScalarParameter,parameter_name=name,default_value=value,group='Arena Surface')
def vector(mat,name,value):return node(mat,u.MaterialExpressionVectorParameter,parameter_name=name,default_value=u.LinearColor(*value),group='Arena Surface')
def build(name,texture,rim=False):
    mat=asset(name,u.Material,u.MaterialFactoryNew); LIB.delete_all_material_expressions(mat)
    put(mat,material_domain=u.MaterialDomain.MD_SURFACE,blend_mode=u.BlendMode.BLEND_OPAQUE,shading_model=u.MaterialShadingModel.MSM_DEFAULT_LIT)
    tile=scalar(mat,'TileSize',1400.)
    uv=node(mat,u.MaterialExpressionTextureCoordinate)
    size=node(mat,u.MaterialExpressionConstant,r=15000.)
    tiling=node(mat,u.MaterialExpressionDivide);link(size,tiling,'A');link(tile,tiling,'B')
    uv_a=node(mat,u.MaterialExpressionMultiply);link(uv,uv_a,'A');link(tiling,uv_a,'B')
    # Three independently offset patches blend on a triangular lattice. No
    # repeated mirror grid: each patch stays inside the source image. Explicit
    # derivatives keep cell changes from causing mip seams.
    lattice="""float2 skew=float2(UV.x-.57735027*UV.y,1.15470054*UV.y)*3.;
float2 cell=floor(skew),f=frac(skew);
float3 weights;
float2 v0,v1,v2;
if(f.x+f.y<1.) {weights=float3(1.-f.x-f.y,f.x,f.y);v0=cell;v1=cell+float2(1,0);v2=cell+float2(0,1);}
else {weights=float3(f.x+f.y-1.,1.-f.x,1.-f.y);v0=cell+1.;v1=cell+float2(0,1);v2=cell+float2(1,0);}
"""
    def uv_custom(code,output,derivative=None):
        custom=node(mat,u.MaterialExpressionCustom,code=code,output_type=output)
        pin=u.CustomInput();pin.set_editor_property('input_name','UV');pins=[pin]
        if derivative is not None:
            d=u.CustomInput();d.set_editor_property('input_name','D');pins.append(d)
        custom.set_editor_property('inputs',pins);link(uv_a,custom,'UV')
        if derivative is not None:link(derivative,custom,'D')
        return custom
    weights=uv_custom(lattice+'weights=pow(max(weights,0.0001),5.); return weights/(weights.x+weights.y+weights.z);',u.CustomMaterialOutputType.CMOT_FLOAT3)
    bias=scalar(mat,'DetailMipBias',.5)
    power=node(mat,u.MaterialExpressionPower);two=node(mat,u.MaterialExpressionConstant,r=2.);link(two,power);link(bias,power,'Exp')
    derivatives=[]
    for kind in [u.MaterialExpressionDDX,u.MaterialExpressionDDY]:
        derivative=node(mat,kind);link(uv_a,derivative)
        scaled=node(mat,u.MaterialExpressionMultiply);link(derivative,scaled,'A');link(power,scaled,'B');derivatives.append(scaled)
    samples=[]
    for i in range(3):
        randomize=lattice+('float2 v=v%d; float2 h=frac(sin(float2(dot(v,float2(127.1,311.7)),dot(v,float2(269.5,183.3))))*43758.5453); float rotation=floor(h.x*4.);'%i)
        def orient(q):return 'float2 q='+q+'; q=rotation<.5?q:(rotation<1.5?float2(-q.y,q.x):(rotation<2.5?-q:float2(q.y,-q.x)));'
        coords=uv_custom(randomize+orient('UV-float2(v.x+.5*v.y,.8660254*v.y)/3.')+'return q+.5+(h-.5)*.28;',u.CustomMaterialOutputType.CMOT_FLOAT2)
        rotated_derivatives=[uv_custom(randomize+orient('D')+'return q;',u.CustomMaterialOutputType.CMOT_FLOAT2,d) for d in derivatives]
        sample=node(mat,u.MaterialExpressionTextureSampleParameter2D,parameter_name='GroundTexture',texture=texture,sampler_type=u.MaterialSamplerType.SAMPLERTYPE_COLOR,mip_value_mode=u.TextureMipValueMode.TMVM_DERIVATIVE)
        link(coords,sample,'UVs');link(rotated_derivatives[0],sample,'DDX(UVs)');link(rotated_derivatives[1],sample,'DDY(UVs)');samples.append(sample)
    inputs={'PaintA':samples[0],'PaintB':samples[1],'PaintC':samples[2],'Weights':weights,
            'Position':node(mat,u.MaterialExpressionWorldPosition),
            'Brightness':scalar(mat,'Brightness',.74),
            'Contrast':scalar(mat,'Contrast',.78),
            'Saturation':scalar(mat,'Saturation',.84),
            'ColorVariation':scalar(mat,'ColorVariation',.14),
            'GroundTint':vector(mat,'GroundTint',(.92,1.0,1.1,1.)),
            'RimColor':vector(mat,'RimColor',(.21,.25,.16,1.))}
    code="""float2 p=Position.xy;
float3 paint=PaintA.rgb*Weights.x+PaintB.rgb*Weights.y+PaintC.rgb*Weights.z;
float value=dot(paint,float3(.2126,.7152,.0722));
paint=lerp(value.xxx,paint,Saturation);
paint=lerp(float3(.23,.23,.23),paint,Contrast);
float macro=sin(p.x*.0007+sin(p.y*.0006))*cos(p.y*.00085+.4);
float edge=smoothstep(3600.,5400.,max(abs(p.x),abs(p.y)));
return max(0.,paint*GroundTint.rgb*Brightness*(1.+macro*ColorVariation)*(1.-edge*.13));"""
    if rim:code="""float value=dot(PaintA.rgb,float3(.2126,.7152,.0722));
return RimColor.rgb*(.65+saturate(value)*.65);"""
    custom=node(mat,u.MaterialExpressionCustom,code=code,description='Arena painted surface with independent broad variation',output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
    pins=[]
    for name in inputs:
        pin=u.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
    custom.set_editor_property('inputs',pins)
    for name,obj in inputs.items():link(obj,custom,name)
    assert all(LIB.get_inputs_for_material_expression(mat,custom))
    assert LIB.connect_material_property(custom,'',u.MaterialProperty.MP_BASE_COLOR)
    for value,prop in [(1.,u.MaterialProperty.MP_ROUGHNESS),(.025,u.MaterialProperty.MP_SPECULAR)]:
        constant=node(mat,u.MaterialExpressionConstant,r=value);assert LIB.connect_material_property(constant,'',prop)
    LIB.set_base_material_usage(mat,u.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES,True)
    LIB.layout_material_expressions(mat);LIB.recompile_material(mat);save(mat);return mat
def instance(name,parent,params):
    obj=asset(name,u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew);LIB.set_material_instance_parent(obj,parent)
    for key,value in params.items():
        if isinstance(value,u.Texture):LIB.set_material_instance_texture_parameter_value(obj,key,value)
        elif isinstance(value,tuple):LIB.set_material_instance_vector_parameter_value(obj,key,u.LinearColor(*value))
        else:LIB.set_material_instance_scalar_parameter_value(obj,key,value)
    LIB.update_material_instance(obj);save(obj);return obj
protected=[disk('/Game/Maps/Lvl_B1_Arena','.umap'),disk('/Game/Maps/Lvl_B1_Lvl1_Lighting','.umap')]
for folder in ['TerracottaArena','ReferenceGround','StylizedGround','EnvironmentPolish']:
    protected+=list((ROOT/'Content/HeavensDivide/Materials'/folder).glob('*.uasset'))
hashes={str(p.relative_to(ROOT)):digest(p) for p in protected if p.exists()}
backup(MAP,'.umap')
textures={}; records=[]
for v in CONFIG['variants']:
    source=ROOT/'Art/ArenaVariations'/v['source']; assert source.is_file()
    name='T_'+v['id'];backup(FOLDER+'/'+name)
    task=u.AssetImportTask()
    for key,value in dict(filename=str(source),destination_path=FOLDER,destination_name=name,automated=True,replace_existing=True,replace_existing_settings=False,save=False).items():task.set_editor_property(key,value)
    ASSETS.import_asset_tasks([task]);texture=u.load_asset(FOLDER+'/'+name);assert isinstance(texture,u.Texture2D)
    put(texture,srgb=True,compression_settings=u.TextureCompressionSettings.TC_DEFAULT,address_x=u.TextureAddress.TA_CLAMP,address_y=u.TextureAddress.TA_CLAMP,lod_group=u.TextureGroup.TEXTUREGROUP_WORLD,power_of_two_mode=u.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO,max_texture_size=2048,mip_gen_settings=u.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP,never_stream=False)
    save(texture);textures[v['id']]=texture
    w,h=struct.unpack('>II',source.read_bytes()[16:24])
    records.append(dict(id=v['id'],native_pixels=[w,h],tile_cm=v['tile_cm'],native_texels_per_cm=w/v['tile_cm'],source_sha256=digest(source)))
parent=build('M_ArenaSurface',textures[SELECTED]);rim_parent=build('M_ArenaRim',textures[SELECTED],True)
floors={};rims={}
for v in CONFIG['variants']:
    params=dict(GroundTexture=textures[v['id']],TileSize=float(v['tile_cm']),RimColor=tuple(v['rim_color']),Brightness=.74,Contrast=.78,Saturation=.84,ColorVariation=.14,GroundTint=(.92,1.,1.10,1.),DetailMipBias=.5)
    floors[v['id']]=instance('MI_'+v['id'],parent,params)
    rims[v['id']]=instance('MI_Rim_'+v['id'],rim_parent,params)
assert LEVEL.load_level(MAP);assert LEVEL.set_current_level_by_name('Lvl_B1_Lvl1')
actors=ACTORS.get_all_level_actors()
floor=next(a for a in actors if a.get_actor_label()=='Plane' and isinstance(a,u.StaticMeshActor))
slabs=next(a for a in actors if a.get_actor_label()=='GroundRockStamps')
rim=next(a for a in actors if a.get_actor_label()=='TerracottaArenaRim')
def snapshot():
    return {a.get_path_name():[a.get_actor_location().x,a.get_actor_location().y,a.get_actor_location().z,a.get_actor_rotation().pitch,a.get_actor_rotation().yaw,a.get_actor_rotation().roll,a.get_actor_scale3d().x,a.get_actor_scale3d().y,a.get_actor_scale3d().z] for a in ACTORS.get_all_level_actors()}
before=snapshot();collision=str(floor.static_mesh_component.get_collision_profile_name())
floor.modify(True);floor.static_mesh_component.modify(True);floor.static_mesh_component.set_material(0,floors[SELECTED])
# Painted stones are now human-scale within the tile. Retain the old oversized
# stamp placements for reversibility, but hide them in editor and game.
slabs.modify(True)
for comp in slabs.get_components_by_class(u.MeshComponent):
    comp.modify(True);comp.set_visibility(False,True);comp.set_hidden_in_game(True,True)
# Reuse the owned rim's components, reducing its 5.5-7.4m boulders to 3-4m rocks.
rim.modify(True);rng=random.Random(20261014);rim_records=[]
for variant,component in enumerate(rim.get_components_by_class(u.HierarchicalInstancedStaticMeshComponent)):
    component.modify(True);mesh=component.static_mesh;bounds=mesh.get_bounds();extent=bounds.box_extent;origin=bounds.origin
    component.clear_instances()
    for slot in range(len(mesh.get_editor_property('static_materials'))):component.set_material(slot,rims[SELECTED])
    transforms=[]
    for row in range(2):
        for side in range(4):
            for i in range(36):
                if (i+side+row)%3!=variant:continue
                along=-5250+i*300+(-75 if row else 0)
                if side==1 and abs(along)<850:continue
                edge=5300+row*300+rng.uniform(-30,30)
                width=rng.uniform(320,405);depth=rng.uniform(310,400);height=rng.uniform(110,170)+(45 if row else 0)
                center=[along,edge] if side==0 else ([-edge,along] if side==1 else ([along,-edge] if side==2 else [edge,along]))
                yaw=(0. if side in (0,2) else 90.)+rng.uniform(-8,8);radians=math.radians(yaw)
                scale=u.Vector(width/(2*extent.x),depth/(2*extent.y),height/(2*extent.z))
                ox,oy=origin.x*scale.x,origin.y*scale.y
                location=u.Vector(center[0]-(ox*math.cos(radians)-oy*math.sin(radians)),center[1]-(ox*math.sin(radians)+oy*math.cos(radians)),height*.5-35-origin.z*scale.z)
                transforms.append(u.Transform(location=location,rotation=u.Rotator(pitch=0,yaw=yaw,roll=0),scale=scale))
    component.add_instances(transforms,True,True,False)
    assert component.get_instance_count()==len(transforms)
    assert component.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION
    assert not component.get_editor_property('can_ever_affect_navigation')
    rim_records.append({'mesh':mesh.get_path_name(),'instances':len(transforms)})
assert snapshot()==before,'An actor transform changed'
assert str(floor.static_mesh_component.get_collision_profile_name())==collision
assert LEVEL.save_current_level()
assert all(digest(ROOT/p)==h for p,h in hashes.items())
report=dict(passed=True,selected=SELECTED,backup=str(BACKUP),map=MAP,variants=records,rim=rim_records,floor_collision=collision,existing_actor_transforms_preserved=True,oversized_stamps_hidden=True,protected_assets=hashes)
(OUT/'Applied.json').write_text(json.dumps(report,indent=2)+'\n')
u.log('ARENA_VARIATIONS_APPLIED '+SELECTED)
