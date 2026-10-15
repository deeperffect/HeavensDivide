"""Targeted perimeter, sandstone collision and Ancient_Ruins corrections."""
from pathlib import Path
from datetime import datetime
from types import SimpleNamespace
import json,math,random,shutil,sys,hashlib
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools'))
OUT=ROOT/'Saved/SurvivalMaps/ObstaclePolish';OUT.mkdir(parents=True,exist_ok=True)
E=u.EditorAssetLibrary;A=u.get_editor_subsystem(u.EditorActorSubsystem);L=u.get_editor_subsystem(u.LevelEditorSubsystem)
SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.get_default_object(u.StaticMeshEditorSubsystem)

def prepare_sandstone(mesh):
 # A shared 412-triangle collision mesh retains the crevices that convex hulls
 # bridged. Static triangle BVH, 80% fewer faces than the 2,064-triangle visual.
 # Prepared offline; no runtime generation or extra ticking.
 path='/Game/HeavensDivide/Environment/SurvivalMaps/Shared/Meshes/SM_Sandstone_Collision'
 collider=E.load_asset(path) if E.does_asset_exist(path) else None
 if not collider:
  from import_old_village import import_file
  collider=import_file(ROOT/'Art/Environment/SurvivalMaps/Collision/SM_Sandstone_Collision.fbx',path.rsplit('/',1)[0],path.rsplit('/',1)[1],True)
 mesh.set_editor_property('complex_collision_mesh',collider)
 mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 assert SM.remove_collisions(mesh)
 E.set_metadata_tag(mesh,'SurvivalFittedCollision','LowPolyTrimeshV1')
 assert E.save_loaded_asset(mesh,False)
 return 412

def resize_about_bottom(a,factor,new_center=None):
 c,e=a.get_actor_bounds(False);bottom=c.z-e.z
 a.set_actor_scale3d(a.get_actor_scale3d()*factor)
 nc,ne=a.get_actor_bounds(False);loc=a.get_actor_location()
 loc.x+=(new_center[0] if new_center else c.x)-nc.x;loc.y+=(new_center[1] if new_center else c.y)-nc.y;loc.z+=bottom-(nc.z-ne.z)
 a.set_actor_location(loc,False,True)

def set_cliff_height(a,height):
 c,e=a.get_actor_bounds(False);bottom=c.z-e.z;s=a.get_actor_scale3d();s.z*=height/(e.z*2);a.set_actor_scale3d(s)
 nc,ne=a.get_actor_bounds(False);loc=a.get_actor_location();loc.x+=c.x-nc.x;loc.y+=c.y-nc.y;loc.z+=bottom-(nc.z-ne.z);a.set_actor_location(loc,False,True)

def fit_boundary_cliff(a,height):
 # Local Y is the broad wall face. Align it with the actual boundary segment,
 # retaining an overlapping width and a shallow depth outside the combat floor.
 from survival_map_layout import boundary
 center,_=a.get_actor_bounds(False);points=boundary('EmberQuarry');nearest=None
 for p,q in zip(points,points[1:]+points[:1]):
  dx=q[0]-p[0];dy=q[1]-p[1];length=math.hypot(dx,dy)
  t=max(0,min(1,((center.x-p[0])*dx+(center.y-p[1])*dy)/(length*length)))
  x=p[0]+dx*t;y=p[1]+dy*t;distance=math.hypot(center.x-x,center.y-y)
  if nearest is None or distance<nearest[0]:nearest=(distance,x,y,dx,dy,length)
 _,x,y,dx,dy,length=nearest;nx=dy/length;ny=-dx/length
 if nx*x+ny*y<0:nx=-nx;ny=-ny
 b=a.static_mesh_component.static_mesh.get_bounds()
 a.set_actor_rotation(u.Rotator(pitch=0,yaw=math.degrees(math.atan2(dy,dx))-90,roll=0),False)
 a.set_actor_scale3d(u.Vector(350/(2*b.box_extent.x),1050/(2*b.box_extent.y),height/(2*b.box_extent.z)))
 c,e=a.get_actor_bounds(False);loc=a.get_actor_location();loc.x+=x+nx*600-c.x;loc.y+=y+ny*600-c.y;loc.z+=-180-(c.z-e.z);a.set_actor_location(loc,False,True)
 tags=list(a.get_editor_property('tags'))
 if 'CameraClearEscarpment' not in [str(t) for t in tags]:tags.append('CameraClearEscarpment')
 a.set_editor_property('tags',tags)

def clear_distant_cliff(a):
 from survival_map_layout import boundary,backdrop_height
 c,e=a.get_actor_bounds(False);points=boundary('EmberQuarry');nearest=None
 for p,q in zip(points,points[1:]+points[:1]):
  dx=q[0]-p[0];dy=q[1]-p[1];t=max(0,min(1,((c.x-p[0])*dx+(c.y-p[1])*dy)/(dx*dx+dy*dy)))
  x=p[0]+dx*t;y=p[1]+dy*t;d=math.hypot(c.x-x,c.y-y)
  if nearest is None or d<nearest[0]:nearest=(d,x,y)
 d,x,y=nearest;nx=(c.x-x)/max(1,d);ny=(c.y-y)/max(1,d)
 shift=max(0,1000-(d-abs(nx)*e.x-abs(ny)*e.y))
 if shift<1:return False
 loc=a.get_actor_location();loc.x+=nx*shift;loc.y+=ny*shift
 loc.z+=backdrop_height('EmberQuarry',c.x+nx*shift,c.y+ny*shift)-280-(c.z-e.z)
 a.set_actor_location(loc,False,True);return True

def run():
 from survival_map_layout import MAPS,edge_distance
 from survival_ruin_details import add_details,TAG
 builders={'LIBRARY_ONLY':True}
 exec(compile((ROOT/'Tools/build_survival_maps.py').read_text(encoding='utf-8'),'build_survival_maps.py','exec'),builders)
 backup=OUT/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S');backup.mkdir(parents=True,exist_ok=True)
 old_village=ROOT/'Content/Maps/Lvl_OldVillage.umap';old_hash=hashlib.sha256(old_village.read_bytes()).hexdigest()
 report={'passed':False,'maps':{},'old_village_sha256':old_hash}
 for key in ['MoonlitForest','EmberQuarry','SunkenSanctuary']:
  disk=ROOT/'Content/Maps'/('Lvl_'+key+'.umap');shutil.copy2(disk,backup/disk.name)
  assert L.load_level('/Game/Maps/Lvl_'+key)
  builders.update(name=key,cfg=MAPS[key],rng=random.Random(2311));b=SimpleNamespace(**builders)
  result={};actors=list(A.get_all_level_actors())
  if key=='MoonlitForest':
   changed=0;heights=[]
   for a in actors:
    if a.get_actor_label() not in ['Woodland perimeter','Distant woodland']:continue
    near=a.get_actor_label()=='Woodland perimeter'
    for c in a.get_components_by_class(u.InstancedStaticMeshComponent):
     bounds=c.static_mesh.get_bounds()
     for i in range(c.get_instance_count()):
      t=c.get_instance_transform(i,True);h=bounds.box_extent.z*2*t.scale3d.z
      if near and h>1130:factor=.4
      elif not near and edge_distance(key,t.translation.x,t.translation.y)<1800 and h>1400:factor=1400/h
      else:
       if near:heights.append(h)
       continue
      bottom=t.translation.z+(bounds.origin.z-bounds.box_extent.z)*t.scale3d.z
      t.scale3d=t.scale3d*factor;loc=t.translation
      if near:loc.x*=1.035;loc.y*=1.035
      loc.z=bottom-(bounds.origin.z-bounds.box_extent.z)*t.scale3d.z;t.translation=loc
      assert c.update_instance_transform(i,t,True,True,True);changed+=1
      if near:heights.append(h*factor)
   result={'resized_tree_instances':changed,'woodland_perimeter_height_cm':[min(heights),max(heights)]}
  elif key=='EmberQuarry':
   heights=[];changed=0
   for a in actors:
    if a.get_actor_label()=='Canyon escarpment':
     c,e=a.get_actor_bounds(False)
     if not a.actor_has_tag('CameraClearEscarpment'):
      fit_boundary_cliff(a,min(800,e.z*2*(.26 if e.z*2>1000 else .85)));changed+=1
     heights.append(a.get_actor_bounds(False)[1].z*2)
    elif a.get_actor_label()=='Distant canyon formation':
     c,e=a.get_actor_bounds(False)
     if edge_distance(key,c.x,c.y)<2200 and e.z*2>1500:resize_about_bottom(a,1500/(e.z*2));changed+=1
   rocks=[a for a in actors if a.get_actor_label()=='Layered sandstone']
   assert rocks
   mesh=rocks[0].static_mesh_component.static_mesh
   asset_disk=ROOT/'Content'/Path(mesh.get_path_name().split('.')[0].replace('/Game/','')+'.uasset')
   shutil.copy2(asset_disk,backup/asset_disk.name)
   collision_triangles=prepare_sandstone(mesh)
   proxies=[a for a in actors if a.get_actor_label()=='Layered sandstone collision']
   before_boxes=[{'center':list(a.get_actor_bounds(False)[0].to_tuple()),'extent':list(a.get_actor_bounds(False)[1].to_tuple())} for a in proxies]
   for a in proxies:A.destroy_actor(a)
   for a in rocks:
    c=a.static_mesh_component;c.set_collision_profile_name('OldVillageOccluder');c.set_editor_property('can_ever_affect_navigation',True)
    c.set_editor_property('generate_overlap_events',False)
   result={'resized_cliffs':changed,'canyon_perimeter_height_cm':[min(heights),max(heights)],'sandstone_rocks':len(rocks),'removed_box_proxies':len(proxies),'shared_collision_triangles':collision_triangles,'old_collision_boxes':before_boxes}
  else:
   # SM_Column is an open-backed wall pilaster. SM_Corner_Pillar is closed at every LOD.
   mesh=b.copied_mesh('/Game/Assets/Environment/Ancient_Ruins/Meshes/SM_Corner_Pillar')
   columns=[a for a in actors if a.get_actor_label()=='Cloister column'];proxies=[a for a in actors if a.get_actor_label()=='Cloister column collision']
   for a in columns:
    c,e=a.get_actor_bounds(False);bottom=c.z-e.z
    comp=a.static_mesh_component;comp.set_static_mesh(mesh);a.set_actor_scale3d(u.Vector(3.18,3.18,2.25))
    nc,ne=a.get_actor_bounds(False);loc=a.get_actor_location();loc.x+=c.x-nc.x;loc.y+=c.y-nc.y;loc.z+=bottom-(nc.z-ne.z);a.set_actor_location(loc,False,True)
    for slot in range(comp.get_num_materials()):comp.set_material(slot,b.fade_material(mesh.get_material(slot),b.fade_cache))
    for proxy in proxies:
     pc,pe=proxy.get_actor_bounds(False)
     if math.hypot(pc.x-c.x,pc.y-c.y)>1:continue
     proxy.set_actor_scale3d(u.Vector(2.05,2.05,6.5))
     n,ex=proxy.get_actor_bounds(False);p=proxy.get_actor_location();p.z+=bottom-(n.z-ex.z);proxy.set_actor_location(p,False,True)
   # Only regenerate this named addition on re-runs; preserve all other placed actors.
   for a in list(A.get_all_level_actors()):
    if a.actor_has_tag(TAG):A.destroy_actor(a)
   result={'replaced_open_columns':len(columns),'column_source':mesh.get_path_name(),'ruin_details':add_details(b)}
  if key!='MoonlitForest':
   world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
   assert u.EnemyNavigationLibrary.bake_old_village_navigation(world),'Navigation bake failed '+key
   cfg=MAPS[key];start=u.NavigationSystemV1.project_point_to_navigation(world,u.Vector(*cfg['start'],0),None,None,query_extent=u.Vector(350,350,120));routes=[]
   for x,y in cfg['goals']:
    p=u.NavigationSystemV1.project_point_to_navigation(world,u.Vector(x,y,0),None,None,query_extent=u.Vector(350,350,120))
    path=u.NavigationSystemV1.find_path_to_location_synchronously(world,start,p,None,None)
    ok=math.hypot(p.x-start.x,p.y-start.y)<1 or bool(path and path.is_valid() and not path.is_partial())
    routes.append({'goal':[x,y],'reachable':ok})
   result['routes']=routes
  report['maps'][key]=result
  assert L.save_current_level()
  (OUT/'Changes.json').write_text(json.dumps(report,indent=2))
 assert hashlib.sha256(old_village.read_bytes()).hexdigest()==old_hash,'OldVillage was modified'
 assert all(r['reachable'] for m in report['maps'].values() for r in m.get('routes',[])),report
 report['passed']=True;(OUT/'Changes.json').write_text(json.dumps(report,indent=2));u.log('SURVIVAL_OBSTACLES_POLISHED')

if __name__=='__main__':run()
