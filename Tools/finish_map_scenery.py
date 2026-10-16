"""Final fitted foliage collision, intact tree LODs and backdrop seating."""
import sys,json
from pathlib import Path
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools'))
import repair_map_scenery as r
E=r.E;A=r.A;L=r.L;SM=r.SM;M=r.M;done=set();report={}
def prepare_tree(source):
 m=r.local_mesh(source,True)
 if SM.get_lod_count(m)>2:
  assert SM.remove_lods(m);assert SM.set_lod_from_static_mesh(m,1,source,1,True)==1;E.save_loaded_asset(m,False)
 settings=SM.get_nanite_settings(m)
 if settings.enabled and settings.shape_preservation!=u.NaniteShapePreservation.PRESERVE_AREA:
  settings.set_editor_property('shape_preservation',u.NaniteShapePreservation.PRESERVE_AREA);SM.set_nanite_settings(m,settings,True);E.save_loaded_asset(m,False)
 return m
def fitted_bush(m):
 if m.get_path_name() in done:return
 bs=m.get_editor_property('body_setup');SM.remove_collisions(m)
 b=m.get_bounds();box=u.KBoxElem();r.put(box,center=u.Vector(b.origin.x,b.origin.y,b.origin.z-b.box_extent.z*.5),x=b.box_extent.x*.9,y=b.box_extent.y*.9,z=b.box_extent.z)
 ag=bs.get_editor_property('agg_geom');ag.set_editor_property('box_elems',[box]);bs.set_editor_property('agg_geom',ag)
 bs.set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
 E.save_loaded_asset(m,False);done.add(m.get_path_name())
for key in ['OldVillage','MoonlitForest','SunkenSanctuary','EmberQuarry']:
 assert L.load_level('/Game/Maps/Lvl_'+key);row={'trees':0,'bushes':0,'two_sided':0};terraces=[]
 for a in A.get_all_level_actors():
  for c in a.get_components_by_class(u.StaticMeshComponent):
   m=c.static_mesh
   if not m:continue
   source=r.source_tree(m)
   if source:
    c.set_static_mesh(prepare_tree(source));r.put(c,evaluate_world_position_offset=True);row['trees']+=1
    if isinstance(c,u.InstancedStaticMeshComponent) and str(a.get_folder_path()).startswith('95_Backdrop'):
     b=c.static_mesh.get_bounds()
     for i in range(c.get_instance_count()):
      t=c.get_instance_transform(i,True);t.translation.z=-12-(b.origin.z-b.box_extent.z)*t.scale3d.z;c.update_instance_transform(i,t,True,True,True)
   elif any(t in m.get_name() for t in ['SM_Bush','SM_Rose_Bush']) and m.get_path_name().startswith(r.DEST):
    fitted_bush(m);row['bushes']+=1
   elif m.get_path_name().startswith(r.DEST+'/Meshes/') and m.get_path_name() not in done:
    bs=m.get_editor_property('body_setup');bs.set_editor_property('double_sided_geometry',True);SM.remove_collisions(m);E.save_loaded_asset(m,False);done.add(m.get_path_name());row['two_sided']+=1
  if key=='OldVillage' and isinstance(a,u.StaticMeshActor) and a.get_actor_label()=='Distant stone terrace':
   c,e=a.get_actor_bounds(False);r.fit(a,a.static_mesh_component.static_mesh,list(c.to_tuple()),[e.x*2,e.y*2,35],bottom=-35);terraces.append(a)
  if key=='OldVillage' and isinstance(a,u.StaticMeshActor) and a.get_actor_label()=='Garden pond water':
   center,e=a.get_actor_bounds(False)
   r.fit(a,u.load_asset(r.SPHERE),list(center.to_tuple()),[e.x*2,e.y*2,.1],keep_materials=True);r.visual(a.static_mesh_component)
 if key=='OldVillage':
  for a in A.get_all_level_actors():
   if not isinstance(a,u.StaticMeshActor) or a.get_actor_label() not in ['Hilltop sanctuary','Outlying farmhouse']:continue
   c,e=a.get_actor_bounds(False);p=a.get_actor_location();p.z+=0-(c.z-e.z);a.set_actor_location(p,False,True)
 for a in A.get_all_level_actors():
  if not isinstance(a,u.StaticMeshActor):continue
  c=a.static_mesh_component
  if c.static_mesh and any(t in c.static_mesh.get_name() for t in ['SM_OV_ThatchedCottage','SM_OV_RaisedStorehouse']):c.set_editor_property('fill_collision_underneath_for_navmesh',True)
 world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();assert u.EnemyNavigationLibrary.bake_old_village_navigation(world);assert L.save_current_level();report[key]=row
(r.OUT/'FinalRefinements.json').write_text(json.dumps(report,indent=2))
