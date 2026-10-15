"""Final targeted presentation corrections; preserves all placed map actors."""
from pathlib import Path
from datetime import datetime
import json,sys,shutil,math
import unreal as u
root=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(root/'Tools'))
from survival_map_layout import MAPS
from setup_old_village_navigation_camera import fade_material
builders={'LIBRARY_ONLY':True}
exec(compile((root/'Tools/build_survival_maps.py').read_text(encoding='utf-8'),'build_survival_maps.py','exec'),builders)
E=u.EditorAssetLibrary;A=u.get_editor_subsystem(u.EditorActorSubsystem);L=u.get_editor_subsystem(u.LevelEditorSubsystem)
report={};cache={}
backup=root/'Saved/SurvivalMaps/Backups'/(datetime.now().strftime('%Y%m%d_%H%M%S')+'_Finish')
backup.mkdir(parents=True,exist_ok=True)
for key in MAPS:
 shutil.copy2(root/'Content/Maps'/('Lvl_'+key+'.umap'),backup/('Lvl_'+key+'.umap'))
 assert L.load_level('/Game/Maps/Lvl_'+key)
 builders['name']=key;builders['cfg']=MAPS[key];ground,sky,water,land=builders['materials']()
 for a in A.get_all_level_actors():
  if a.get_actor_label()=='Continuous world beyond arena':a.static_mesh_component.set_material(0,land)
 changed=[];scaled_plants=0
 for a in A.get_all_level_actors():
  if str(a.get_folder_path())=='95_Backdrop' or a.actor_has_tag('SurvivalInsetPaving'):continue
  for c in a.get_components_by_class(u.InstancedStaticMeshComponent):
   bounds=c.static_mesh.get_bounds();width=max(bounds.box_extent.x*2,bounds.box_extent.y*2,1.)
   limit=60 if c.static_mesh.get_name().startswith('SM_Blue_Flower') else 160
   if key=='EmberQuarry' and c.static_mesh.get_name().startswith('SM_FlatStone'):
    for slot in range(c.get_num_materials()):c.set_material(slot,E.load_asset('/Game/HeavensDivide/Environment/SurvivalMaps/EmberQuarry/Materials/M_PaintedSandstone'))
   for i in range(c.get_instance_count()):
    t=c.get_instance_transform(i,True);scale=t.scale3d.x
    if width*scale<=limit+.01:continue
    new_scale=limit/width;bottom=t.translation.z+(bounds.origin.z-bounds.box_extent.z)*scale
    t.scale3d=u.Vector(new_scale,new_scale,new_scale);loc=t.translation;loc.z=bottom-(bounds.origin.z-bounds.box_extent.z)*new_scale;t.translation=loc
    assert c.update_instance_transform(i,t,True,True,True);scaled_plants+=1
 if key=='EmberQuarry':
  actors=A.get_all_level_actors()
  for a in actors:
   if a.get_actor_label()=='Inset mine rails':
    c,e=a.get_actor_bounds(False)
    if c.z-e.z<0:loc=a.get_actor_location();loc.z+=.3-(c.z-e.z);a.set_actor_location(loc,False,True)
  for a in actors:
   if not isinstance(a,u.StaticMeshActor) or not str(a.get_folder_path()).endswith('/Mine face'):continue
   center,ext=a.get_actor_bounds(False)
   if ext.z*2<1200:continue
   x=center.x;y=center.y-650;distance=math.hypot(x,y);nx=x+x/distance*1000;ny=y+y/distance*1000
   for proxy in actors:
    if not isinstance(proxy,u.StaticMeshActor) or str(proxy.get_folder_path())!='70_Collision':continue
    pc,pe=proxy.get_actor_bounds(False)
    if math.hypot(pc.x-center.x,pc.y-center.y)>1:continue
    s=proxy.get_actor_scale3d();s.x*=1100/1500;s.y*=1100/1500;proxy.set_actor_scale3d(s)
    loc=proxy.get_actor_location();loc.x+=nx-pc.x;loc.y+=ny-pc.y;proxy.set_actor_location(loc,False,True)
   a.set_actor_scale3d(a.get_actor_scale3d()*(1100/1500));nc,ne=a.get_actor_bounds(False);loc=a.get_actor_location()
   loc.x+=nx-nc.x;loc.y+=ny-nc.y;loc.z-=nc.z-ne.z;a.set_actor_location(loc,False,True)
  world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
  assert u.EnemyNavigationLibrary.bake_old_village_navigation(world)
  cfg=MAPS[key];start=u.NavigationSystemV1.project_point_to_navigation(world,u.Vector(*cfg['start'],0),None,None,query_extent=u.Vector(350,350,120))
  for x,y in cfg['goals']:
   goal=u.NavigationSystemV1.project_point_to_navigation(world,u.Vector(x,y,0),None,None,query_extent=u.Vector(350,350,120))
   path=u.NavigationSystemV1.find_path_to_location_synchronously(world,start,goal,None,None)
   assert math.hypot(goal.x-start.x,goal.y-start.y)<1 or (path and path.is_valid() and not path.is_partial()),('quarry path still blocked',x,y)
 for a in A.get_all_level_actors():
  if not isinstance(a,u.StaticMeshActor) or a.actor_has_tag('SurvivalMapBackdrop'):continue
  c=a.static_mesh_component
  if not c.is_visible():continue
  center,extent=a.get_actor_bounds(False)
  if center.z+extent.z<=180 or any(str(t)=='CameraFade' for t in c.get_editor_property('component_tags')):continue
  for slot in range(c.get_num_materials()):
   src=c.get_material(slot)
   if src:c.set_material(slot,fade_material(src,cache))
  c.set_editor_property('component_tags',['CameraFade'])
  if c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION:c.set_collision_profile_name('OldVillageCanopy')
  changed.append(a.get_actor_label())
 assert L.save_current_level()
 report[key]={'raised_occluders_prepared':changed,'rescaled_groundcover':scaled_plants}
(root/'Saved/SurvivalMaps/FinishReport.json').write_text(json.dumps(report,indent=2))
u.log('SURVIVAL_REWORK_FINISH_COMPLETE')
