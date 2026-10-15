from pathlib import Path
import json,sys,unreal as u
root=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(root/'Tools'))
from survival_map_layout import MAPS
L=u.get_editor_subsystem(u.LevelEditorSubsystem);A=u.get_editor_subsystem(u.EditorActorSubsystem)
assert L.load_level('/Game/Maps/Lvl_EmberQuarry')
w=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
cfg=MAPS['EmberQuarry'];report={'routes':[],'obstacles':[]}
start=u.NavigationSystemV1.project_point_to_navigation(w,u.Vector(*cfg['start'],0),None,None,query_extent=u.Vector(350,350,120))
for x,y in cfg['goals']:
 p=u.NavigationSystemV1.project_point_to_navigation(w,u.Vector(x,y,0),None,None,query_extent=u.Vector(350,350,120))
 path=u.NavigationSystemV1.find_path_to_location_synchronously(w,start,p,None,None)
 report['routes'].append({'goal':[x,y],'projection':list(p.to_tuple()),'partial':path.is_partial(),'points':[list(v.to_tuple()) for v in path.path_points]})
for a in A.get_all_level_actors():
 if not isinstance(a,u.StaticMeshActor):continue
 c=a.static_mesh_component
 if c.get_editor_property('can_ever_affect_navigation') and not a.actor_has_tag('SurvivalMapGround'):
  center,ext=a.get_actor_bounds(False)
  report['obstacles'].append({'label':a.get_actor_label(),'mesh':c.static_mesh.get_name(),'center':list(center.to_tuple()),'extent':list(ext.to_tuple())})
(root/'Saved/SurvivalMaps/QuarryNavigation.json').write_text(json.dumps(report,indent=2))
u.log('QUARRY_NAVIGATION_INSPECTED')
