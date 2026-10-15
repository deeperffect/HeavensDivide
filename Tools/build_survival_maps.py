"""Build three isolated survival maps; vendor assets and OldVillage stay intact."""
from pathlib import Path
from datetime import datetime
import json,math,random,sys,shutil,hashlib
from types import SimpleNamespace
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools'))
from survival_map_layout import MAPS,POOLS,boundary,playable,reserved,backdrop_height,segment_distance,inside_polygon
import survival_map_dressing as dressing
from import_old_village import import_file
from setup_old_village_navigation_camera import fade_material
from old_village_music import ensure_level_music
E=u.EditorAssetLibrary;M=u.MaterialEditingLibrary;AT=u.AssetToolsHelpers.get_asset_tools()
ACT=u.get_editor_subsystem(u.EditorActorSubsystem);L=u.get_editor_subsystem(u.LevelEditorSubsystem)
SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.get_default_object(u.StaticMeshEditorSubsystem)
DEST='/Game/HeavensDivide/Environment/SurvivalMaps'
J='/Game/Assets/Environment/JapaneseShrine/Meshes/'
R='/Game/Assets/Environment/Ancient_Ruins/Meshes/'
G='/Game/Assets/Environment/GreenCave/Meshes/'
V='/Game/Assets/Environment/Stylized_Village/Assets/'
OUT=ROOT/'Saved/SurvivalMaps';OUT.mkdir(parents=True,exist_ok=True)
cache={};copies={};fade_cache={};REPORT={};name=None;cfg=None;rng=None

def put(o,**props):
 for k,v in props.items():o.set_editor_property(k,v)

def load(path):
 if path not in cache:cache[path]=u.load_asset(path);assert cache[path],path
 return cache[path]

def actor(cls,label,x=0,y=0,z=0,yaw=0,folder='30_Scenery'):
 a=ACT.spawn_actor_from_class(cls,u.Vector(x,y,z),u.Rotator(pitch=0,yaw=yaw,roll=0));assert a,label
 a.set_actor_label(label);a.set_folder_path(folder);return a

def copied_mesh(path,tree=False):
 if path in copies:return copies[path]
 src=load(path)
 target=DEST+'/Shared/Meshes/'+src.get_name()+'_'+hashlib.sha1(path.encode()).hexdigest()[:6]
 mesh=E.load_asset(target) if E.does_asset_exist(target) else E.duplicate_asset(path,target)
 if tree and SM.get_lod_count(mesh)>2:
  SM.remove_lods(mesh)
  SM.set_lod_from_static_mesh(mesh,1,src,1,True)
 # Local copies only. Architecture keeps openings; broad rocks get cheap proxies.
 mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 E.save_loaded_asset(mesh,False);copies[path]=mesh;return mesh

def mesh_actor(mesh,label,x,y,z=0,scale=1,yaw=0,folder='30_Scenery',solid=False,mat=None,seat=True,backdrop=False):
 a=actor(u.StaticMeshActor,label,x,y,z,yaw,folder);c=a.static_mesh_component
 c.set_static_mesh(mesh if isinstance(mesh,u.StaticMesh) else load(mesh))
 s=(scale,scale,scale) if isinstance(scale,(int,float)) else scale
 a.set_actor_scale3d(u.Vector(*s))
 if seat:
  center,extent=a.get_actor_bounds(False);p=a.get_actor_location();p.z+=z-(center.z-extent.z);a.set_actor_location(p,False,True)
 c.set_collision_profile_name('OldVillageOccluder' if solid else 'NoCollision')
 put(c,generate_overlap_events=False,can_ever_affect_navigation=solid,cast_shadow=not backdrop)
 if mat:
  for slot in range(max(1,c.get_num_materials())):c.set_material(slot,mat)
 if backdrop:
  a.set_editor_property('tags',['SurvivalMapBackdrop']);put(c,affect_distance_field_lighting=False,visible_in_ray_tracing=False)
 elif not label.startswith('Ground'):
  center,extent=a.get_actor_bounds(False)
  if extent.z>65 or center.z+extent.z>180:
   for slot in range(c.get_num_materials()):
    src=c.get_material(slot)
    if src:
     try:c.set_material(slot,fade_material(src,fade_cache))
     except Exception as ex:u.log_warning('Scenery material retained: '+str(ex))
   c.set_collision_profile_name('OldVillageOccluder' if solid else 'OldVillageCanopy')
   c.set_editor_property('component_tags',['CameraFade'])
  if extent.z<180:c.set_cull_distance(8500);c.set_cast_shadow(False)
 c.set_editor_property('evaluate_world_position_offset',False)
 return a

def proxy(label,x,y,w,d,h=500,yaw=0,z=0,round=False):
 mesh='/Engine/BasicShapes/Cylinder' if round else '/Engine/BasicShapes/Cube'
 a=mesh_actor(mesh,label,x,y,z,scale=(w/100,d/100,h/100),yaw=yaw,folder='70_Collision',seat=True)
 a.set_actor_hidden_in_game(True);c=a.static_mesh_component;c.set_visibility(False)
 c.set_collision_profile_name('OldVillageSolid');put(c,can_ever_affect_navigation=True,cast_shadow=False)
 return a

def prop(path,label,x,y,height=None,scale=1,yaw=0,solid=True,z=0,folder='30_Scenery',collision_box=False):
 mesh=copied_mesh(path)
 b=mesh.get_bounds()
 if height:scale=height/(2*b.box_extent.z)
 a=mesh_actor(mesh,label,x,y,z,scale,yaw,folder,solid and not collision_box)
 center,_=a.get_actor_bounds(False);loc=a.get_actor_location();loc.x+=x-center.x;loc.y+=y-center.y;a.set_actor_location(loc,False,True)
 if collision_box:
  center,extent=a.get_actor_bounds(False)
  proxy(label+' collision',center.x,center.y,extent.x*1.7,extent.y*1.7,min(650,extent.z*2),z=z)
 return a

def custom_material(key,code,textures=False,unlit=False,sky=False):
 path=DEST+'/'+name+'/Materials/M_'+key
 mat=E.load_asset(path) if E.does_asset_exist(path) else AT.create_asset('M_'+key,DEST+'/'+name+'/Materials',u.Material,u.MaterialFactoryNew())
 M.delete_all_material_expressions(mat);put(mat,two_sided=sky,shading_model=u.MaterialShadingModel.MSM_UNLIT if unlit else u.MaterialShadingModel.MSM_DEFAULT_LIT)
 if sky:put(mat,is_sky=True)
 node=M.create_material_expression(mat,u.MaterialExpressionCustom);put(node,code=code,output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
 pins=[];sources=[]
 for key,cls,out in [('P',u.MaterialExpressionWorldPosition,'XYZ'),('C',u.MaterialExpressionCameraPositionWS,'')]:
  p=u.CustomInput();put(p,input_name=key);pins.append(p);sources.append((key,M.create_material_expression(mat,cls),out))
 if textures:
  for key,tex in [('Grass','ForestMoss'),('Dirt','QuarryEarth'),('Stone','SanctuaryStone')]:
   p=u.CustomInput();put(p,input_name=key);pins.append(p)
   texnode=M.create_material_expression(mat,u.MaterialExpressionTextureObject)
   put(texnode,texture=load(DEST+'/Shared/Textures/T_Painted_'+tex))
   sources.append((key,texnode,''))
 put(node,inputs=pins)
 for key,src,out in sources:M.connect_material_expressions(src,out,node,key)
 if unlit:M.connect_material_property(node,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
 else:
  M.connect_material_property(node,'',u.MaterialProperty.MP_BASE_COLOR)
  glow=M.create_material_expression(mat,u.MaterialExpressionMultiply);put(glow,const_b=.26 if name=='MoonlitForest' else .08)
  M.connect_material_expressions(node,'',glow,'A');M.connect_material_property(glow,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
  rough=M.create_material_expression(mat,u.MaterialExpressionConstant);put(rough,r=.9);M.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS)
 for key in ['BaseColor','EmissiveColor','Roughness']:u.SwapVFXSetupLibrary.set_property_text(mat.get_editor_property('editor_only_data'),key,'(UseConstant=False)')
 M.recompile_material(mat);assert E.save_loaded_asset(mat,False);return mat

def color(v):return 'float3('+','.join(str(x) for x in v)+')'

def materials():
 code='float d=100000; float2 p=P.xy; float2 a,b,v; float t;\n'
 for path in cfg['paths']:
  for a,b in zip(path,path[1:]):
   code+=f'a=float2({a[0]},{a[1]});b=float2({b[0]},{b[1]});v=b-a;t=saturate(dot(p-a,v)/max(dot(v,v),1));d=min(d,length(p-a-v*t));\n'
 for x,y in cfg['goals']:code+=f'd=min(d,length(p-float2({x},{y}))-380);\n'
 code+='float w=1-smoothstep(330,780,d+sin(p.x*.005)*70+cos(p.y*.004)*65);\n'
 code+='float3 g=Texture2DSample(Grass,GrassSampler,p/1150).rgb;float3 e=Texture2DSample(Dirt,DirtSampler,p/1300).rgb;float3 s=Texture2DSample(Stone,StoneSampler,p/850).rgb;float luminance=dot(e,float3(.299,.587,.114));float broad=.92+.08*sin(p.x*.0013)*cos(p.y*.0017);\n'
 if name=='MoonlitForest':code+='return lerp(g*.82,lerp(luminance*float3(.78,.82,.86),s*float3(.62,.74,.83),.18),w)*broad;'
 elif name=='SunkenSanctuary':code+='float moss=(1-w)*(.22+.28*sin(p.x*.002+sin(p.y*.002)));return lerp(s,g,saturate(moss))*broad;'
 else:code+='return lerp(e*.85,lerp(e,luminance*float3(1.35,1.06,.73),.55),w)*broad;'
 ground=custom_material('Ground',code,True)
 skycode='float3 d=normalize(P-C);float t=saturate(d.z);float3 col=lerp('+color(cfg['horizon'])+','+color(cfg['zenith'])+',pow(t,.55));'
 if name=='MoonlitForest':skycode+='float moon=smoothstep(.9994,.99965,dot(d,normalize(float3(.3,.5,.7))));col+=moon*float3(1.1,1.3,1.6);float stars=pow(saturate(sin(d.x*431)*sin(d.y*517)*sin(d.z*397)),160);col+=stars*.3;'
 sky=custom_material('Horizon',skycode+'return col;',unlit=True,sky=True)
 water=custom_material('Water','float k=pow(saturate(.5+.5*sin(P.x*.003+sin(P.y*.004)*.45)),24)*.012;return '+('float3(.012,.07,.09)' if name=='MoonlitForest' else 'float3(.025,.18,.19)' if name=='SunkenSanctuary' else 'float3(.07,.045,.028)')+'+k;',unlit=True)
 land=custom_material('DistantLand','float h=saturate(P.z/2400);return '+color(cfg['ground'])+'*lerp(.52,.72,h);',unlit=True)
 return ground,sky,water,land

def lighting():
 a=actor(u.DirectionalLight,'Moonlight' if name=='MoonlitForest' else 'Low angled sun',z=4000,folder='90_Lighting')
 a.set_actor_rotation(u.Rotator(pitch=-43,yaw=cfg['yaw'],roll=0),False)
 put(a.get_component_by_class(u.DirectionalLightComponent),mobility=u.ComponentMobility.MOVABLE,intensity=cfg['sun'],use_temperature=True,temperature=cfg['temperature'],light_source_angle=3.)
 if name=='MoonlitForest':put(a.get_component_by_class(u.DirectionalLightComponent),use_temperature=False,light_color=u.Color(r=125,g=165,b=255,a=255))
 a=actor(u.SkyLight,'Soft ambient fill',z=1000,folder='90_Lighting')
 put(a.get_component_by_class(u.SkyLightComponent),mobility=u.ComponentMobility.MOVABLE,source_type=u.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP,cubemap=load(V+'_Global/Materials/_UtilityTextures/HDRI_Epic_Courtyard_Daylight'),intensity=cfg['sky'],light_color=u.Color(r=130,g=165,b=230,a=255) if name=='MoonlitForest' else u.Color(r=255,g=244,b=228,a=255))
 a=actor(u.PostProcessVolume,cfg['title']+' color grade',folder='90_Lighting');put(a,unbound=True,priority=10.)
 settings=a.get_editor_property('settings')
 for k,v in dict(auto_exposure_min_brightness=1.,auto_exposure_max_brightness=1.,auto_exposure_bias=cfg['exposure'],bloom_intensity=.22,motion_blur_amount=0.,vignette_intensity=.12,ambient_occlusion_intensity=.5).items():put(settings,**{'override_'+k:True,k:v})
 a.set_editor_property('settings',settings)
 a=actor(u.ExponentialHeightFog,'Non-volumetric distant haze',z=-250,folder='95_Backdrop')
 put(a.get_component_by_class(u.ExponentialHeightFogComponent),fog_density=.018,fog_height_falloff=.2,start_distance=10500,fog_max_opacity=.7,fog_inscattering_luminance=u.LinearColor(*cfg['fog'],1),enable_volumetric_fog=False)

def instances(mesh,label,positions,heights,folder='95_Backdrop',mat=None):
 # Instanced distant silhouettes and low plants do not enter gameplay collision/nav.
 SDS=u.get_engine_subsystem(u.SubobjectDataSubsystem);SD=u.SubobjectDataBlueprintFunctionLibrary
 a=actor(u.Actor,label,folder=folder);a.set_editor_property('tags',['SurvivalMapBackdrop']);a.set_actor_tick_enabled(False)
 hs=SDS.k2_gather_subobject_data_for_instance(a);ah=next(h for h in hs if SD.is_actor(SD.get_data(h)))
 rh,err=SDS.add_new_subobject(u.AddNewSubobjectParams(parent_handle=ah,new_class=u.SceneComponent,blueprint_context=None));assert not str(err),str(err)
 put(SD.get_associated_object(SD.get_data(rh)),mobility=u.ComponentMobility.STATIC)
 ch,err=SDS.add_new_subobject(u.AddNewSubobjectParams(parent_handle=rh,new_class=u.HierarchicalInstancedStaticMeshComponent,blueprint_context=None));assert not str(err),str(err)
 c=SD.get_associated_object(SD.get_data(ch));c.set_static_mesh(mesh)
 if mat:
  for slot in range(c.get_num_materials()):c.set_material(slot,mat)
 put(c,mobility=u.ComponentMobility.STATIC,can_ever_affect_navigation=False,cast_shadow=False,affect_distance_field_lighting=False,visible_in_ray_tracing=False,evaluate_world_position_offset=False)
 c.set_collision_profile_name('NoCollision');c.set_cull_distance(22000 if folder=='95_Backdrop' else 7500)
 b=mesh.get_bounds();transforms=[]
 for (x,y,z),h in zip(positions,heights):
  s=h/max(1.,b.box_extent.z*2)
  if folder!='95_Backdrop':s=min(s,(60 if mesh.get_name().startswith('SM_Blue_Flower') else 160)/max(1.,b.box_extent.x*2,b.box_extent.y*2))
  transforms.append(u.Transform(location=u.Vector(x,y,z-(b.origin.z-b.box_extent.z)*s),rotation=u.Rotator(pitch=0,yaw=rng.uniform(0,360),roll=0),scale=u.Vector(s,s,s)))
 c.add_instances(transforms,True,True,False);return a

def gameplay():
 world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
 world.get_world_settings().set_editor_property('default_game_mode',load('/Game/HeavensDivide/Blueprints/BP_SurvivorGameMode').generated_class())
 actor(u.PlayerStart,'Player start',*cfg['start'],110,folder='80_Gameplay')
 bounds=actor(u.MinimapBounds,cfg['title']+' minimap bounds',folder='80_Gameplay')
 bounds.get_component_by_class(u.BoxComponent).set_box_extent(u.Vector(cfg['size'][0]/2,cfg['size'][1]/2,1800),False)
 sp=actor(load('/Game/HeavensDivide/Blueprints/EnemyCharacters/BP_EnemySpawner').generated_class(),'Enemy waves and run clock',z=100,folder='80_Gameplay')
 put(sp,require_nav_mesh_projection=True,nav_mesh_projection_extent=u.Vector(250,250,100))
 for bp,label in [('/Game/HeavensDivide/Blueprints/Objectives/BP_RunObjectiveDirector','Shrines and run objectives'),('/Game/HeavensDivide/Blueprints/Encounters/BP_EncounterDirector','Urns and field encounters')]:
  actor(load(bp).generated_class(),label,z=100,folder='80_Gameplay')
 gate=actor(load('/Game/HeavensDivide/Blueprints/BP_BossToriiGate').generated_class(),'Boss gate - ten minutes',*cfg['gate'],0,folder='80_Gameplay')
 put(gate,unlock_run_time_seconds=600.,boss_arena_level=load('/Game/Maps/Lvl_B1_Arena'),enemy_spawner=sp)
 visual=gate.get_editor_property('gate_visual');center,extent,_=u.SystemLibrary.get_component_bounds(visual)
 rel=visual.get_editor_property('relative_location');rel.z-=center.z-extent.z;visual.set_relative_location(rel,False,False);gate.align_portal_to_gate()
 assert u.EnemyNavigationLibrary.bake_old_village_navigation(world),'Static navigation bake failed'
 positions=[]
 for i,(x,y) in enumerate(cfg['goals']):
  p=u.NavigationSystemV1.project_point_to_navigation(world,u.Vector(x,y,0),None,None,query_extent=u.Vector(350,350,120))
  if p is None:
   nearby=[]
   for a in ACT.get_all_level_actors():
    if not isinstance(a,u.StaticMeshActor):continue
    c,e=a.get_actor_bounds(False);comp=a.static_mesh_component
    if abs(c.x-x)<e.x+500 and abs(c.y-y)<e.y+500:
     nearby.append({'actor':a.get_actor_label(),'center':list(c.to_tuple()),'extent':list(e.to_tuple()),'profile':str(comp.get_collision_profile_name()),'nav':comp.get_editor_property('can_ever_affect_navigation')})
   (OUT/'FailedClearing.json').write_text(json.dumps({'map':name,'point':[x,y],'nearby':nearby},indent=2))
  assert p is not None,(name,'objective off nav',x,y)
  actor(u.ObjectiveSpawnPoint,'Objective clearing %02d'%(i+1),p.x,p.y,0,folder='80_Gameplay/Objective clearings');positions.append(list(p.to_tuple()))
 ensure_level_music()
 return positions

def build(key):
 global name,cfg,rng
 name=key;cfg=MAPS[key];rng=random.Random(cfg['seed'])
 package='/Game/Maps/Lvl_'+key;disk=ROOT/'Content/Maps'/('Lvl_'+key+'.umap')
 if disk.exists():
  dest=OUT/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S');dest.mkdir(parents=True,exist_ok=True);shutil.copy2(disk,dest/disk.name)
  assert L.load_level(package)
  for a in list(ACT.get_all_level_actors()):
   if not isinstance(a,u.WorldSettings) and a.get_class()!=u.Brush.static_class():ACT.destroy_actor(a)
 else:assert L.new_level(package)
 floor,sky,water,land=materials()
 meshes=[]
 for suffix in ['Ground','Surroundings']:
  mn='SM_'+key+'_'+suffix
  mesh=import_file(ROOT/'Art/Environment/SurvivalMaps/Terrain'/(mn+'.fbx'),DEST+'/'+key+'/Meshes',mn,True)
  mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);E.save_loaded_asset(mesh,False);meshes.append(mesh)
 a=mesh_actor(meshes[0],'Ground - '+cfg['title'],0,0,seat=False,mat=floor,folder='00_Ground')
 a.set_editor_property('tags',['SurvivalMapGround']);a.static_mesh_component.set_collision_profile_name('OldVillageGround');a.static_mesh_component.set_editor_property('can_ever_affect_navigation',True)
 # Verify FBX units before placing any scenery.
 center,extent=a.get_actor_bounds(False);assert extent.x>cfg['size'][0]*.35,(name,extent)
 mesh_actor(meshes[1],'Continuous world beyond arena',0,0,seat=False,mat=land,folder='95_Backdrop',backdrop=True)
 mesh_actor('/Engine/BasicShapes/Plane','Distant water and flooded basins',0,0,-45 if name=='SunkenSanctuary' else -470,(3000,3000,1),mat=water,seat=False,folder='95_Backdrop',backdrop=True)
 mesh_actor('/Engine/BasicShapes/Sphere','Painted sky',0,0,0,(10000,10000,10000),mat=sky,seat=False,folder='95_Backdrop',backdrop=True)
 lighting()
 dressing.configure(SimpleNamespace(**globals()));count=dressing.build(water)
 if key=='EmberQuarry':
  from quarry_edge_apron import install
  install()
 objectives=gameplay()
 assert L.save_current_level()
 REPORT[key]={'revision':2,'map':package,'actors':len(ACT.get_all_level_actors()),'instanced_scenery':count,'unique_mesh_sources':len(dressing.used),'objective_clearings':objectives,'gate_seconds':600,'navigation':'Static','ground_bounds':[list(center.to_tuple()),list(extent.to_tuple())]}
 (OUT/'BuildReport.json').write_text(json.dumps(REPORT,indent=2));u.log('SURVIVAL_MAP_SAVED '+key)

if not globals().get('LIBRARY_ONLY',False):
 for texname in ['ForestMoss','SanctuaryStone','QuarryEarth']:
  tex=import_file(ROOT/'Art/Environment/SurvivalMaps/Textures'/('T_Painted_'+texname+'.png'),DEST+'/Shared/Textures','T_Painted_'+texname)
  put(tex,srgb=True,max_texture_size=2048,power_of_two_mode=u.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO,address_x=u.TextureAddress.TA_MIRROR,address_y=u.TextureAddress.TA_MIRROR)
  E.save_loaded_asset(tex,False)
 for key in globals().get('MAP_KEYS',list(MAPS)):build(key)
 u.log('SURVIVAL_MAPS_BUILD_COMPLETE')
