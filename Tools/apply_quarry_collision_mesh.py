"""Apply the measured reduced sandstone collider and restore low cliff width."""
from pathlib import Path
from datetime import datetime
import unreal as u,sys,json,math,shutil
root=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(root/'Tools'))
from survival_map_layout import MAPS
from polish_survival_obstacles import prepare_sandstone
out=root/'Saved/SurvivalMaps/ObstaclePolish';E=u.EditorAssetLibrary;A=u.get_editor_subsystem(u.EditorActorSubsystem);L=u.get_editor_subsystem(u.LevelEditorSubsystem)
backup=out/'Backups'/(datetime.now().strftime('%Y%m%d_%H%M%S')+'_CollisionMesh');backup.mkdir(parents=True,exist_ok=True)
disk=root/'Content/Maps/Lvl_EmberQuarry.umap';shutil.copy2(disk,backup/disk.name)
assert L.load_level('/Game/Maps/Lvl_EmberQuarry')
actors=list(A.get_all_level_actors());source=next(a for a in actors if a.get_actor_label()=='Layered sandstone').static_mesh_component.static_mesh
asset_disk=root/'Content'/Path(source.get_path_name().split('.')[0].replace('/Game/','')+'.uasset');shutil.copy2(asset_disk,backup/asset_disk.name)
old={row['name']:row for row in json.loads((out/'Inspection.json').read_text())['maps']['EmberQuarry']};fixed=0
for a in actors:
 if a.get_actor_label()!='Canyon escarpment' or a.actor_has_tag('LowWideEscarpment') or a.actor_has_tag('CameraClearEscarpment'):continue
 row=old[a.get_name()];c,e=a.get_actor_bounds(False);bottom=c.z-e.z;s=a.get_actor_scale3d();s.x=row['scale'][0];s.y=row['scale'][1];a.set_actor_scale3d(s)
 nc,ne=a.get_actor_bounds(False);loc=a.get_actor_location();loc.x+=c.x-nc.x;loc.y+=c.y-nc.y;loc.z+=bottom-(nc.z-ne.z);a.set_actor_location(loc,False,True)
 a.set_editor_property('tags',list(a.get_editor_property('tags'))+['LowWideEscarpment']);fixed+=1
triangles=prepare_sandstone(source);collider=source.get_editor_property('complex_collision_mesh');b=source.get_bounds();c=collider.get_bounds()
assert max(abs(x-y) for x,y in zip(b.origin.to_tuple(),c.origin.to_tuple()))<5,(b,c)
assert max(abs(x-y) for x,y in zip(b.box_extent.to_tuple(),c.box_extent.to_tuple()))<5,(b,c)
w=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();assert u.EnemyNavigationLibrary.bake_old_village_navigation(w)
cfg=MAPS['EmberQuarry'];start=u.NavigationSystemV1.project_point_to_navigation(w,u.Vector(*cfg['start'],0),None,None,query_extent=u.Vector(350,350,120))
for x,y in cfg['goals']:
 p=u.NavigationSystemV1.project_point_to_navigation(w,u.Vector(x,y,0),None,None,query_extent=u.Vector(350,350,120));path=u.NavigationSystemV1.find_path_to_location_synchronously(w,start,p,None,None)
 assert math.hypot(p.x-start.x,p.y-start.y)<1 or bool(path and path.is_valid() and not path.is_partial()),(x,y)
assert L.save_current_level()
for count in [16,24,32]:
 path='/Game/HeavensDivide/Environment/SurvivalMaps/Shared/CollisionTrials/SM_Sandstone_'+str(count)
 if E.does_asset_exist(path):assert E.delete_asset(path)
report={'passed':True,'widened_cliffs':fixed,'source_triangles':2064,'collision_triangles':triangles,'collision_mesh':collider.get_path_name(),'collision_origin':list(c.origin.to_tuple()),'collision_extent':list(c.box_extent.to_tuple()),'routes':8}
(out/'FinalCollision.json').write_text(json.dumps(report,indent=2));u.log('QUARRY_COLLISION_MESH_APPLIED')
