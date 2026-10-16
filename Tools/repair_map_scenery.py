"""Targeted foliage, provenance and collision repair of the four saved run maps.

Preserves the authored layout and user-supplied Tripo kit. Original vendor assets
are read-only. Physics helpers are retained only inside visible obstacles.
"""
from pathlib import Path
from datetime import datetime
import json,math,hashlib,shutil,sys
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools'))
from setup_old_village_navigation_camera import fade_material
from survival_map_layout import MAPS,POOLS,boundary
E=u.EditorAssetLibrary;M=u.MaterialEditingLibrary;A=u.get_editor_subsystem(u.EditorActorSubsystem);L=u.get_editor_subsystem(u.LevelEditorSubsystem)
SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.get_default_object(u.StaticMeshEditorSubsystem)
DEST='/Game/HeavensDivide/Environment/SceneryRepair';OUT=ROOT/'Saved/MapSceneryAudit';OUT.mkdir(parents=True,exist_ok=True)
R='/Game/Assets/Environment/Ancient_Ruins/Meshes/';J='/Game/Assets/Environment/JapaneseShrine/Meshes/'
PLANE='/Game/Assets/VFX/MagicProjectilesVol3/Models/SM_Plane01';SPHERE='/Game/Assets/VFX/StylizedOrbsVol1/Models/SM_Sphere01'
TAG='SceneryRepairV1';cache={};mats={};fades={}
def put(o,**kw):
 for k,v in kw.items():o.set_editor_property(k,v)
def tag(a,t):
 tags=[str(x) for x in a.tags]
 if t not in tags:tags.append(t)
 a.set_editor_property('tags',tags)
def local_mesh(src,simple=False):
 p=src.get_path_name();k=(p,simple)
 if p.startswith(DEST+'/Meshes/'):return src
 if k in cache:return cache[k]
 path=DEST+'/Meshes/'+src.get_name()+'_'+hashlib.sha1(p.encode()).hexdigest()[:6]+('_Trunk' if simple else '')
 m=E.load_asset(path) if E.does_asset_exist(path) else E.duplicate_asset(p,path)
 bs=m.get_editor_property('body_setup')
 if simple:
  bs.set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
 else:
  bs.set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
  # Existing custom sandstone collision remains valid on this isolated copy.
  SM.remove_collisions(m)
 E.set_metadata_tag(m,'ScenerySource',p);E.save_loaded_asset(m,False);cache[k]=m;return m
def fit(a,mesh,center,dimensions,yaw=0,bottom=None,keep_materials=False):
 c=a.static_mesh_component;old=[c.get_material(i) for i in range(c.get_num_materials())]
 c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
 b=mesh.get_bounds();s=[dimensions[i]/max(.001,2*list(b.box_extent.to_tuple())[i]) for i in range(3)]
 if b.box_extent.z<.001:s[2]=1.
 a.set_actor_rotation(u.Rotator(pitch=0,yaw=yaw,roll=0),False);a.set_actor_scale3d(u.Vector(*s))
 nc,ne=a.get_actor_bounds(False);p=a.get_actor_location();p.x+=center[0]-nc.x;p.y+=center[1]-nc.y;p.z+=(bottom+ne.z if bottom is not None else center[2])-nc.z;a.set_actor_location(p,False,True)
 if keep_materials and old:
  for i in range(c.get_num_materials()):c.set_material(i,old[min(i,len(old)-1)])
def solid(c):
 c.set_collision_profile_name('OldVillageOccluder' if 'CameraFade' in [str(t) for t in c.component_tags] else 'OldVillageSolid')
 put(c,can_ever_affect_navigation=True,generate_overlap_events=False)
def visual(c):
 c.set_collision_profile_name('OldVillageCanopy' if 'CameraFade' in [str(t) for t in c.component_tags] else 'NoCollision');put(c,can_ever_affect_navigation=False,generate_overlap_events=False)
def stationary_material(src):
 if not isinstance(src,u.MaterialInstanceConstant):return src
 p=src.get_path_name()
 if p.startswith(DEST+'/Materials/'):return src
 if p in mats:return mats[p]
 names=M.get_scalar_parameter_names(src)
 if not any('wind' in str(n).lower() for n in names):return src
 path=DEST+'/Materials/'+src.get_name()+'_'+hashlib.sha1(p.encode()).hexdigest()[:6]+'_Stationary'
 m=E.load_asset(path) if E.does_asset_exist(path) else E.duplicate_asset(p,path)
 for n in names:
  # Keep wavelengths, mask scales and distances nonzero: those are divisors in
  # the vendor shader, and zeroing them destroys leaf color/opacity.
  if any(t in str(n).lower().replace(' ','') for t in ['windintensity','windweight','windspeed']):M.set_material_instance_scalar_parameter_value(m,n,0.)
 E.set_metadata_tag(m,'SceneryFoliageSource',p)
 M.update_material_instance(m);E.save_loaded_asset(m,False);mats[p]=m;return m
def foliage(c):
 path=c.static_mesh.get_path_name()
 return any(t in path for t in ['/Foliage/','Tree_','TreeBamboo','TreeBlackPine','TreeCedar','SM_Bush','SM_Plant','SM_Leafy','SM_Ivy','SM_Vine','SM_Grass','SM_Fern','SM_Clover','SM_Lily','SM_Blue_Flower','SM_Mushroom'])
def source_tree(mesh):
 name=mesh.get_name()
 if name.startswith('Tree_'):
  name='_'.join(name.split('_')[:3]);p='/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Trees/'+name
  return u.load_asset(p) if E.does_asset_exist(p) else None
 return None
def masked_ground(src,key,surround=False):
 path=DEST+'/Materials/M_'+key+('_Surround' if surround else '_Ground')
 m=E.load_asset(path) if E.does_asset_exist(path) else E.duplicate_asset(src.get_path_name(),path)
 result=m;instances=[]
 while isinstance(m,u.MaterialInstanceConstant):
  instances.append(m);parent=m.get_editor_property('parent');parentpath=path+'_Parent'+str(len(instances))
  base=E.load_asset(parentpath) if E.does_asset_exist(parentpath) else E.duplicate_asset(parent.get_path_name(),parentpath)
  M.set_material_instance_parent(m,base);over=m.get_editor_property('base_property_overrides');over.set_editor_property('override_blend_mode',False);m.set_editor_property('base_property_overrides',over);m=base
 # World-space masks preserve the authored coast and flooded reservoirs while
 # replacing the procedural floor geometry with a supplied flat mesh.
 code='return 1.;'
 if key=='OldVillage':code='return step(-12727.922,P.x+P.y);'
 elif key=='SunkenSanctuary':
  code='float a=1.;'
  for x,y,hx,hy in POOLS:code+=f'a*=1.-(step(abs(P.x-({x})),{hx-25})*step(abs(P.y-({y})),{hy-25}));'
  code+='return a;'
 if key in ['OldVillage','SunkenSanctuary']:
  node=M.create_material_expression(m,u.MaterialExpressionCustom);put(node,code=code,output_type=u.CustomMaterialOutputType.CMOT_FLOAT1)
  pin=u.CustomInput();put(pin,input_name='P');put(node,inputs=[pin]);p=M.create_material_expression(m,u.MaterialExpressionWorldPosition)
  M.connect_material_expressions(p,'XYZ',node,'P');M.connect_material_property(node,'',u.MaterialProperty.MP_OPACITY_MASK)
  put(m,blend_mode=u.BlendMode.BLEND_MASKED)
  u.SwapVFXSetupLibrary.set_property_text(m.get_editor_property('editor_only_data'),'OpacityMask','(UseConstant=False)')
 M.recompile_material(m);E.save_loaded_asset(m,False)
 for inst in instances:M.update_material_instance(inst);E.save_loaded_asset(inst,False)
 return result
def replace_geometry(key,actors,report):
 plane=local_mesh(u.load_asset(PLANE));sphere=u.load_asset(SPHERE);floorasset=local_mesh(u.load_asset(R+'SM_Floor'))
 assert plane.get_bounds().box_extent.z<.01, 'Replacement floor must be a horizontal plane'
 for a in actors:
  if not isinstance(a,u.StaticMeshActor):continue
  c=a.static_mesh_component;m=c.static_mesh
  if not m or not c.is_visible() or a.get_editor_property('hidden'):continue
  name=m.get_name();label=a.get_actor_label();center,ext=a.get_actor_bounds(False);dims=[ext.x*2,ext.y*2,ext.z*2]
  ground=name=='SM_OV_Island' or name=='SM_'+key+'_Ground'
  surround=name in ['SM_OV_Countryside','SM_'+key+'_Surroundings']
  if ground or surround:
   oldmat=c.get_material(0)
   if ground:
    helper=A.spawn_actor_from_class(u.StaticMeshActor,a.get_actor_location(),a.get_actor_rotation());helper.set_actor_scale3d(a.get_actor_scale3d())
    helper.set_actor_label('Ground collision - '+key);helper.set_folder_path('70_Collision/Ground');helper.static_mesh_component.set_static_mesh(m)
    helper.static_mesh_component.set_collision_profile_name('OldVillageGround');put(helper.static_mesh_component,can_ever_affect_navigation=True,cast_shadow=False)
    helper.set_actor_hidden_in_game(True);helper.static_mesh_component.set_visibility(False);tag(helper,'SurvivalMapGround');tag(helper,'MapGroundCollision')
    w,h=(19900,19900) if key=='OldVillage' else (MAPS[key]['size'][0]+1800,MAPS[key]['size'][1]+1800)
    fit(a,plane,[0,0,0],[w,h,0]);c.set_material(0,masked_ground(oldmat,key));visual(c)
    a.set_actor_label('Painted ground - '+key);a.set_editor_property('tags',[str(t) for t in a.tags if str(t)!='SurvivalMapGround'])
   else:
    fit(a,plane,[0,0,-12],[60000,60000,0]);c.set_material(0,masked_ground(oldmat,key,True));visual(c)
   tag(a,TAG);report['replaced_geometry'].append({'actor':label,'old':m.get_path_name(),'new':plane.get_path_name()});continue
  if name=='SM_Quarry_EdgeApron':
   report['replaced_geometry'].append({'actor':label,'old':m.get_path_name(),'new':'covered by supplied ground mesh'});A.destroy_actor(a);continue
  if name=='SM_OV_Water' or m.get_path_name().startswith('/Engine/BasicShapes/Plane') or (name=='Cylinder' and label=='Garden pond water'):
   fit(a,plane,list(center.to_tuple()),[dims[0],dims[1],0],keep_materials=True);visual(c);tag(a,TAG)
   report['replaced_geometry'].append({'actor':label,'old':m.get_path_name(),'new':plane.get_path_name()});continue
  if name=='Cube' and label in ['Stone house footings','Distant stone terrace']:
   fit(a,floorasset,list(center.to_tuple()),dims,keep_materials=False)
   if 'Distant' in label:visual(c)
   else:solid(c)
   tag(a,TAG);report['replaced_geometry'].append({'actor':label,'old':m.get_path_name(),'new':floorasset.get_path_name()});continue
  if name=='Sphere' and 'sky' in label.lower():
   fit(a,sphere,list(center.to_tuple()),dims,keep_materials=True);visual(c);tag(a,TAG)
   report['replaced_geometry'].append({'actor':label,'old':m.get_path_name(),'new':sphere.get_path_name()})
def boundary_art(key,report):
 if key=='OldVillage':return
 # Replace each invisible wall with the supplied solid stone trim, matching its
 # line. The 1.8m low escarpment foot is visible, camera-friendly and gap-free.
 mesh=local_mesh(u.load_asset(R+'SM_Trim_Full'))
 for a in list(A.get_all_level_actors()):
  if not isinstance(a,u.StaticMeshActor) or not a.get_actor_label().startswith('Sealed play boundary'):continue
  p=a.get_actor_location();yaw=a.get_actor_rotation().yaw;scale=a.get_actor_scale3d();length=scale.x*100
  fit(a,mesh,[p.x,p.y,0],[length,170,180],yaw,bottom=-20)
  a.set_actor_hidden_in_game(False);a.static_mesh_component.set_visibility(True);a.set_folder_path('60_Boundary/Stone footing')
  a.set_actor_label('Solid perimeter stone footing');solid(a.static_mesh_component);tag(a,TAG);tag(a,'VisiblePlayBoundary')
  report['visible_boundary_sections']+=1
def repair(key):
 assert L.load_level('/Game/Maps/Lvl_'+key)
 actors=list(A.get_all_level_actors());report={'replaced_geometry':[],'solid_props':[],'foliage_components':0,'removed_proxies':[],'visible_boundary_sections':0,'grounded_backdrop':0}
 replace_geometry(key,actors,report);boundary_art(key,report)
 actors=list(A.get_all_level_actors())
 # Broad boxes bridge rock recesses and the space underneath roofs/branches.
 # Their visible meshes now carry static fitted collision.
 for a in actors:
  if not isinstance(a,u.StaticMeshActor):continue
  c=a.static_mesh_component;label=a.get_actor_label()
  if not c.is_visible() or a.get_editor_property('hidden'):
   keep=any(t in label.lower() for t in ['ground collision','boundary','pond collision','flooded basin','tower solid core','trunk'])
   if not keep and 'collision' in label.lower():report['removed_proxies'].append(label);A.destroy_actor(a)
 for a in list(A.get_all_level_actors()):
  for c in a.get_components_by_class(u.StaticMeshComponent):
   m=c.static_mesh
   if not m or not c.is_visible() or a.get_editor_property('hidden'):continue
   name=m.get_name();label=a.get_actor_label();folder=str(a.get_folder_path());center,ext,_=u.SystemLibrary.get_component_bounds(c)
   background=folder.startswith('95_Backdrop');edge=folder.startswith('60_Boundary')
   if foliage(c):
    for i in range(c.get_num_materials()):
     if c.get_material(i):c.set_material(i,stationary_material(c.get_material(i)))
    # Billboard leaf clusters and distant impostors need vertex positioning.
    put(c,evaluate_world_position_offset=not name.startswith('SM_TreeBamboo'))
    report['foliage_components']+=1
    if background or isinstance(c,u.InstancedStaticMeshComponent):visual(c);continue
    tree=source_tree(m)
    if tree and SM.get_simple_collision_count(tree)>0:
     c.set_static_mesh(local_mesh(tree,True));solid(c);tag(a,'FittedTreeTrunk')
    elif any(t in name for t in ['Trunk','DeadTree']):c.set_static_mesh(local_mesh(m));solid(c);tag(a,'FittedTreeTrunk')
    elif any(t in name for t in ['SM_Bush','SM_Rose_Bush']):
     # Contact follows visible foliage, rather than a broad hidden box.
     c.set_static_mesh(local_mesh(m));solid(c)
    else:visual(c)
    continue
   if isinstance(c,u.InstancedStaticMeshComponent):continue
   if background:
    visual(c);continue
   if folder.startswith('00_Ground') or a.actor_has_tag('SurvivalMapGround') or ('water' in label.lower() and 'waterfront' not in label.lower()) or label.startswith('Plane') or 'Inset ' in label or 'CourtyardMedallion' in name:
    if folder.startswith('00_Ground') and 'ground' in label.lower():continue
    visual(c);continue
   if any(t in name for t in ['Cup','Vase','PotUnSoiled']) or (ext.z*2<45 and max(ext.x,ext.y)*2<80):visual(c);continue
   # Low paving and foliage cards don't become a snagging collision surface.
   if ext.z*2<8 or any(t in label for t in ['Tools on','Pickaxe on','Reservoir lilies','Ivy ']):visual(c);continue
   c.set_static_mesh(local_mesh(m));solid(c)
   if edge:
    tags=[str(t) for t in a.tags if str(t)!='SurvivalMapBackdrop'];a.set_editor_property('tags',tags);tag(a,'SolidBoundaryScenery')
   report['solid_props'].append(label)
 # Remove redundant / misplaced foliage proxies after fitting the actual trunks.
 for a in list(A.get_all_level_actors()):
  if not isinstance(a,u.StaticMeshActor):continue
  label=a.get_actor_label()
  if label in ['Solid tree trunk','Tree trunk collision','Ancient garden trunk collision'] or label.startswith('Plant trunk collision - Planted'):
   report['removed_proxies'].append(label);A.destroy_actor(a)
 # Seat distant scenic objects on the replacement ground sheet. This is only
 # backdrop scenery; authored playable object transforms stay unchanged.
 for a in list(A.get_all_level_actors()):
  if not str(a.get_folder_path()).startswith('95_Backdrop'):continue
  for c in a.get_components_by_class(u.InstancedStaticMeshComponent):
   if not c.static_mesh:continue
   b=c.static_mesh.get_bounds()
   for i in range(c.get_instance_count()):
    t=c.get_instance_transform(i,True);t.translation.z=-12-(b.origin.z-b.box_extent.z)*t.scale3d.z;c.update_instance_transform(i,t,True,True,True)
   report['grounded_backdrop']+=c.get_instance_count()
  if not isinstance(a,u.StaticMeshActor) or a.actor_has_tag(TAG):continue
  c=a.static_mesh_component;m=c.static_mesh
  if not m or 'water' in a.get_actor_label().lower() or 'sky' in a.get_actor_label().lower():continue
  cent,ext=a.get_actor_bounds(False)
  p=a.get_actor_location();p.z+=-12-(cent.z-ext.z);a.set_actor_location(p,False,True);report['grounded_backdrop']+=1
 world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();assert u.EnemyNavigationLibrary.bake_old_village_navigation(world)
 assert L.save_current_level();return report
def run():
 backup=OUT/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S');backup.mkdir(parents=True,exist_ok=True)
 result={'passed':False,'maps':{},'scope':'Four playable run maps; supplied Tripo meshes retained; invisible physics helpers are not scenery.'}
 for key in ['OldVillage','MoonlitForest','SunkenSanctuary','EmberQuarry']:
  disk=ROOT/'Content/Maps'/('Lvl_'+key+'.umap');shutil.copy2(disk,backup/disk.name)
  result['maps'][key]=repair(key);(OUT/'Changes.json').write_text(json.dumps(result,indent=2))
 result['passed']=True;(OUT/'Changes.json').write_text(json.dumps(result,indent=2));u.log('MAP_SCENERY_REPAIR_SAVED')
if __name__=='__main__':run()
