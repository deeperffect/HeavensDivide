"""Build two new maps from the reviewed kit and a shared, editable layout.

Existing gameplay maps and vendor assets are read-only. Rebuilds back up these
two generated maps before replacing them. Run import_old_village.py first.
"""
import json,math,random,sys,shutil
from datetime import datetime
from pathlib import Path
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools'))
from import_old_village import DEST,import_file,painted_material,save
from old_village_gameplay import ensure_run_spawner
from setup_old_village_navigation_camera import prepare_current_level
from old_village_music import ensure_level_music
from old_village_districts import dress_districts, ground_actor
from old_village_objectives import ensure_objectives
from polish_old_village import apply_current_level_polish
from add_old_village_backdrop import add_backdrop
E=u.EditorAssetLibrary;M=u.MaterialEditingLibrary;A=u.AssetToolsHelpers.get_asset_tools()
L=u.get_editor_subsystem(u.LevelEditorSubsystem);ACT=u.get_editor_subsystem(u.EditorActorSubsystem)
CFG=json.loads((ROOT/'Tools/old_village_layout.json').read_text())
S=CFG.get('layout_scale',1.0)
for key in ['land','court','training']:
    CFG[key]=[[e*S,n*S] for e,n in CFG[key]]
CFG['roads']=[[[e*S,n*S] for e,n in road] for road in CFG['roads']]
CFG['buildings']=[[name,e*S,n*S,angle,scale,folder] for name,e,n,angle,scale,folder in CFG['buildings']]
MAN=json.loads((ROOT/'Art/Environment/OldVillage/SourceManifest.json').read_text())
IMPORTED={a['name']:DEST+'/Meshes/'+a['category']+'/SM_OV_'+a['name'] for a in MAN['assets']}
J='/Game/Assets/Environment/JapaneseShrine/Meshes/'
V='/Game/Assets/Environment/Stylized_Village/Assets/'
cache={};report={};rng=random.Random(410)
def put(o,**props):
    for k,v in props.items():o.set_editor_property(k,v)
def load(path):
    if path not in cache:
        cache[path]=u.load_asset(path);assert cache[path],path
    return cache[path]
def xyz(e,n,z=0):return u.Vector(n,e,z)
def spawn(cls,label,e=0,n=0,z=0,yaw=0,folder='00_Ground'):
    a=ACT.spawn_actor_from_class(cls,xyz(e,n,z),u.Rotator(pitch=0,yaw=yaw,roll=0));assert a,label
    a.set_actor_label(label);a.set_folder_path(folder);return a
def static(path,label,e,n,z=0,scale=1,yaw=0,folder='60_Boundary',collision=True,mat=None,ground=False):
    mesh=load(path);a=spawn(u.StaticMeshActor,label,e,n,z,yaw,folder)
    c=a.static_mesh_component;c.set_static_mesh(mesh)
    s=(scale,scale,scale) if isinstance(scale,(int,float)) else scale
    a.set_actor_scale3d(u.Vector(*s))
    c.set_collision_profile_name(('OldVillageGround' if ground else 'OldVillageSolid') if collision else 'NoCollision')
    if mat:c.set_material(0,mat)
    return a
def own(name,e,n,z=0,scale=1,angle=0,folder='30_Village',label=None,collision=True):
    return static(IMPORTED[name],label or name,e,n,z,scale,90+angle,folder,collision)
def box(label,e,n,z,width,depth,height,mat,folder='70_Collision',hidden=False,yaw=0):
    a=static('/Engine/BasicShapes/Cube',label,e,n,z+height/2,(depth/100,width/100,height/100),yaw,folder,True,mat)
    if hidden:a.set_actor_hidden_in_game(True);a.static_mesh_component.set_visibility(False)
    return a
def material_custom(name,code,inputs):
    mat=painted_material(name,color=(.3,.3,.3,1));M.delete_all_material_expressions(mat)
    custom=M.create_material_expression(mat,u.MaterialExpressionCustom,0,0)
    put(custom,code=code,output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
    pins=[];nodes=[]
    pos=M.create_material_expression(mat,u.MaterialExpressionWorldPosition,-600,-250)
    for i,(key,tex,mask) in enumerate(inputs):
        node=M.create_material_expression(mat,u.MaterialExpressionTextureObject,-600,i*150)
        put(node,texture=tex,sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS if mask else u.MaterialSamplerType.SAMPLERTYPE_COLOR)
        pin=u.CustomInput();put(pin,input_name=key);pins.append(pin);nodes.append((key,node))
    pin=u.CustomInput();put(pin,input_name='P');pins.append(pin);put(custom,inputs=pins)
    for key,node in nodes:M.connect_material_expressions(node,'',custom,key)
    M.connect_material_expressions(pos,'XYZ',custom,'P')
    M.connect_material_property(custom,'',u.MaterialProperty.MP_BASE_COLOR)
    glow=M.create_material_expression(mat,u.MaterialExpressionMultiply,200,180);put(glow,const_b=.08);M.connect_material_expressions(custom,'',glow,'A');M.connect_material_property(glow,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    for prop,val in [(u.MaterialProperty.MP_ROUGHNESS,.9),(u.MaterialProperty.MP_SPECULAR,.15)]:
        node=M.create_material_expression(mat,u.MaterialExpressionConstant,200,320);put(node,r=val);M.connect_material_property(node,'',prop)
    for p in ['BaseColor','EmissiveColor','Roughness','Specular']:u.SwapVFXSetupLibrary.set_property_text(mat.get_editor_property('editor_only_data'),p,'(UseConstant=False)')
    M.recompile_material(mat);save(mat);return mat

grounddir=ROOT/'Art/Environment/OldVillage/Ground'
for name in ['Island','Water']:
    mesh=import_file(grounddir/('SM_OV_'+name+'.fbx'),DEST+'/Meshes/Ground','SM_OV_'+name,True)
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);save(mesh)
mask=import_file(grounddir/'T_OV_GroundWeights.png',DEST+'/Textures','T_OV_GroundWeights')
put(mask,srgb=False,compression_settings=u.TextureCompressionSettings.TC_MASKS,address_x=u.TextureAddress.TA_CLAMP,address_y=u.TextureAddress.TA_CLAMP);save(mask)
groundmat=material_custom('M_OV_Ground','''
if(P.z < -2) return float3(.13,.145,.16)*(1+.06*sin(P.z*.06));
float2 uv=float2(P.y+HALF_SIZE,HALF_SIZE-P.x)/FULL_SIZE;
float3 w=Texture2DSample(Weights,WeightsSampler,uv).rgb;
float2 p=P.xy;
float2 q=float2(p.x*.819-p.y*.574,p.x*.574+p.y*.819);
float2 r=float2(p.x*.342+p.y*.940,-p.x*.940+p.y*.342);
float3 g=lerp(Texture2DSample(Grass,GrassSampler,p/420).rgb,Texture2DSample(Grass,GrassSampler,q/560+.37).rgb,.5)*float3(.80,.94,.70);
float3 d=(Texture2DSample(Dirt,DirtSampler,p/780).rgb*.35+Texture2DSample(Dirt,DirtSampler,q/1090+.23).rgb*.4+Texture2DSample(Dirt,DirtSampler,r/490+.71).rgb*.25)*float3(1.05,.94,.8);
float3 s=lerp(Texture2DSample(Stone,StoneSampler,p/650).rgb,Texture2DSample(Stone,StoneSampler,q/830+.43).rgb,.35)*float3(.96,.90,.83);
float macro=1+.035*sin(p.x/870)*sin(p.y/710);
return (g*w.r+d*w.g+s*w.b)*macro;
'''.replace('HALF_SIZE',str(6500*S)).replace('FULL_SIZE',str(13000*S)),[('Weights',mask,True)]+[(n,load('/Game/HeavensDivide/Materials/StylizedGround/T_StylizedGround_'+n),False) for n in ['Grass','Dirt','Stone']])
watermat=material_custom('M_OV_Water','''
float2 p=P.xy;
float stripe=pow(saturate(.5+.5*sin(p.x*.018+sin(p.y*.009)*2)),12)*.07;
float depth=.5+.5*sin(p.x*.0006)*sin(p.y*.001);
return lerp(float3(.008,.045,.055),float3(.014,.10,.115),depth)+stripe*float3(.035,.10,.09);
''',[])
stonepaint=painted_material('M_OV_Foundation',color=(.22,.24,.29,1))
woodpaint=painted_material('M_OV_Timber',color=(.24,.105,.05,1))
blockpaint=painted_material('M_OV_Blockout',color=(.39,.49,.55,1))

def lighting():
    a=spawn(u.DirectionalLight,'Warm afternoon sun',0,0,3000,-35,'90_Lighting');a.set_actor_rotation(u.Rotator(pitch=-47,yaw=-35,roll=0),False)
    c=a.get_component_by_class(u.DirectionalLightComponent);put(c,mobility=u.ComponentMobility.MOVABLE,intensity=6.5,use_temperature=True,temperature=5800,light_source_angle=3.)
    a=spawn(u.SkyLight,'Soft painted sky fill',0,0,1000,folder='90_Lighting');c=a.get_component_by_class(u.SkyLightComponent)
    put(c,mobility=u.ComponentMobility.MOVABLE,source_type=u.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP,cubemap=load(V+'_Global/Materials/_UtilityTextures/HDRI_Epic_Courtyard_Daylight'),intensity=1.25)
    a=spawn(u.PostProcessVolume,'Old Village daylight grade',folder='90_Lighting');put(a,unbound=True,priority=10.)
    s=a.get_editor_property('settings')
    for key,val in dict(auto_exposure_min_brightness=1.,auto_exposure_max_brightness=1.,auto_exposure_bias=.5,bloom_intensity=.15,motion_blur_amount=0.,vignette_intensity=.12,ambient_occlusion_intensity=.65,ambient_occlusion_radius=100.).items():
        put(s,**{'override_'+key:True,key:val})
    put(a,settings=s)

def shared(blockout):
    static(DEST+'/Meshes/Ground/SM_OV_Island','Village ground - expanded island',0,0,0,(S,S,1),90,'00_Ground',True,groundmat,True)
    static(DEST+'/Meshes/Ground/SM_OV_Water','Southwest water',0,0,-100,(S,S,1),90,'00_Ground',False,watermat)
    lighting()
    start=spawn(u.PlayerStart,'Player Start • central crossroads',-200,-1550,110,0,'80_Gameplay')
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    put(world.get_world_settings(),default_game_mode=load('/Game/HeavensDivide/Blueprints/BP_SurvivorGameMode').generated_class())
    bounds=spawn(u.MinimapBounds,'Old Village minimap bounds',0,0,0,folder='80_Gameplay')
    bounds.get_component_by_class(u.BoxComponent).set_box_extent(u.Vector(6500*S,6500*S,1800),False)
    for name,e,n,angle,scale,folder in CFG['buildings']:
        if blockout:
            b=load(IMPORTED[name]).get_bounds();width=b.box_extent.x*2*scale;depth=b.box_extent.y*2*scale;height=b.box_extent.z*2*scale
            box('BLOCK '+name,e,n,0,width,depth,height,blockpaint,folder,yaw=angle)
        else:ground_actor(own(name,e,n,scale=scale,angle=angle,folder=folder))
    if blockout:
        static('/Engine/BasicShapes/Cylinder','BLOCK central landmark',-200*S,0,180,(9,9,3.6),0,'10_Shrine',True,blockpaint)
        box('BLOCK pond',-4600*S,3500*S,0,1050,650,35,blockpaint,'20_Gardens')
        paving=box('BLOCK courtyard medallion',1300*S,2800*S,0,850,850,30,blockpaint,'10_Shrine')
        paving.static_mesh_component.set_collision_profile_name('NoCollision')
    # Hidden collision planes close the entire perimeter, including the waterfront.
    p=CFG['land']
    for i,((e1,n1),(e2,n2)) in enumerate(zip(p,p[1:]+p[:1])):
        length=math.hypot(e2-e1,n2-n1);angle=-math.degrees(math.atan2(n2-n1,e2-e1))
        box('Boundary collision %02d'%i,(e1+e2)/2,(n1+n2)/2,-250,length+50,90,2000,stonepaint,hidden=True,yaw=angle)

def tree(e,n,height=850,red=False):
    path=V+'Foliage/Trees/'+('Tree_Small_Red' if red else 'Tree_Medium_Green')
    b=load(path).get_bounds();s=height/(2*b.box_extent.z)
    static(path,('Maple' if red else 'Garden tree'),e,n,-(b.origin.z-b.box_extent.z)*s,s,rng.uniform(0,360),'20_Gardens/Trees',False)
    # Compact solid trunks, with foliage remaining non-blocking.
    a=static('/Engine/BasicShapes/Cylinder','Tree trunk collision',e,n,150,(.95,.95,3),0,'70_Collision',True,woodpaint)
    a.set_actor_hidden_in_game(True);a.static_mesh_component.set_visibility(False)
def pine(e,n,scale=1.4):
    for suffix in ['Trunk','Leaves']:static(J+'SM_TreeBlackPine'+suffix+'01a','Black pine '+suffix,e,n,0,scale,rng.uniform(0,0),'20_Gardens/Trees',False)
def line_own(name,p1,p2,spacing,z=0,folder='60_Boundary',scale=1):
    e1,n1=p1;e2,n2=p2;length=math.hypot(e2-e1,n2-n1);count=max(1,round(length/spacing));angle=-math.degrees(math.atan2(n2-n1,e2-e1))
    for i in range(count):
        t=(i+.5)/count;own(name,e1+(e2-e1)*t,n1+(n2-n1)*t,z,scale,angle,folder)
def fence(p1,p2):
    e1,n1=p1;e2,n2=p2;length=math.hypot(e2-e1,n2-n1);count=max(1,round(length/280));angle=90-math.degrees(math.atan2(n2-n1,e2-e1))
    for i in range(count):
        t=i/count;static(J+'SM_ShrineFence03a','Training fence',e1+(e2-e1)*t,n1+(n2-n1)*t,0,(length/count/150,1.35,1.35),angle,'40_Training')
def modular_house(e,n,thatched=False):
    folder='30_Village/Modular houses'
    box('Stone house footings',e,n,0,510,330,35,stonepaint,folder)
    # User supplied panels form enclosed walls; door and window are fitted in front.
    for side in [-1,1]:
        own('WallPanel',e+side*125,n+150,35,1,0,folder)
        own('WallPanel',e+side*250,n,35,(1.2,1,1),90,folder)
    own('WallPanel',e+125,n-150,35,1,0,folder)
    own('SlidingDoor',e-125,n-155,35,(1,1,1.08),0,folder)
    own('ShutterWindow',e+125,n-178,135,.85,0,folder,collision=False)
    own('ThatchedRoof' if thatched else 'TiledRoof',e,n,245,(.95,.55,.7) if thatched else 1,0,folder)

for blockout,name in [(True,'Lvl_OldVillage_Blockout'),(False,'Lvl_OldVillage')]:
    disk=ROOT/'Content/Maps'/(name+'.umap')
    if disk.exists():
        backup=ROOT/'Saved/OldVillage/Backups'/datetime.now().strftime('%Y%m%d_%H%M%S');backup.mkdir(parents=True,exist_ok=True);shutil.copy2(disk,backup/disk.name)
    if disk.exists():
        assert L.load_level('/Game/Maps/'+name)
        for previous in ACT.get_all_level_actors():
            if not isinstance(previous,u.WorldSettings) and previous.get_class()!=u.Brush.static_class():ACT.destroy_actor(previous)
    else:assert L.new_level('/Game/Maps/'+name)
    shared(blockout)
    if not blockout:dress_districts(globals())
    apply_current_level_polish()
    add_backdrop()
    ensure_run_spawner()
    prepare_current_level()
    ensure_objectives(S)
    ensure_level_music()
    u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(u.Vector(-14000,-2500,17500),u.Rotator(pitch=-50,yaw=10,roll=0))
    assert L.save_current_level()
    allactors=ACT.get_all_level_actors()
    for a in allactors:
        if isinstance(a,u.StaticMeshActor):
            r=a.get_actor_rotation();assert abs(r.pitch)<.01 and abs(r.roll)<.01,(a.get_actor_label(),r)
    floor=next(a for a in allactors if a.get_actor_label().startswith('Village ground'))
    center,extent=floor.get_actor_bounds(False);assert extent.z<130 and extent.x>6400 and extent.y>6400,(center,extent)
    report[name]={'actors':len(allactors),'static_mesh_actors':sum(isinstance(a,u.StaticMeshActor) for a in allactors),'path':'/Game/Maps/'+name}
    u.log('OLD_VILLAGE_MAP_SAVED '+name+' '+str(report[name]))
(ROOT/'Saved/OldVillage/BuildReport.json').write_text(json.dumps(report,indent=2))
u.log('OLD_VILLAGE_BUILD_COMPLETE')
