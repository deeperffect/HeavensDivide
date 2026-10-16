"""Exhaustive navigation-grid connectivity; identifies isolated spawn pockets."""
from pathlib import Path
import json,math,sys,unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools'))
from survival_map_layout import boundary,playable,inside_polygon,segment_distance
L=u.get_editor_subsystem(u.LevelEditorSubsystem);A=u.get_editor_subsystem(u.EditorActorSubsystem);D={}
cfg=json.loads((ROOT/'Tools/old_village_layout.json').read_text());vb=[(n*cfg['layout_scale'],e*cfg['layout_scale']) for e,n in cfg['land']]
for key in ['OldVillage','MoonlitForest','SunkenSanctuary','EmberQuarry']:
 assert L.load_level('/Game/Maps/Lvl_'+key);world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();actors=A.get_all_level_actors();bounds=vb if key=='OldVillage' else boundary(key)
 start=next(a for a in actors if isinstance(a,u.PlayerStart)).get_actor_location();start.z=0
 start=u.NavigationSystemV1.project_point_to_navigation(world,start,None,None,query_extent=u.Vector(300,300,150));assert start
 rows=[];count=0
 for x in range(math.ceil(min(p[0] for p in bounds)/225)*225,int(max(p[0] for p in bounds)),225):
  for y in range(math.ceil(min(p[1] for p in bounds)/225)*225,int(max(p[1] for p in bounds)),225):
   if not (inside_polygon(x,y,vb) if key=='OldVillage' else playable(key,x,y)) or min(segment_distance((x,y),a,b) for a,b in zip(bounds,bounds[1:]+bounds[:1]))<220:continue
   p=u.NavigationSystemV1.project_point_to_navigation(world,u.Vector(x,y,0),None,None,query_extent=u.Vector(80,80,120))
   if not p:continue
   count+=1
   if math.dist(start.to_tuple(),p.to_tuple())<1:continue
   path=u.NavigationSystemV1.find_path_to_location_synchronously(world,start,p,None,None)
   if path and path.is_valid() and not path.is_partial():continue
   nearby=[]
   for a in actors:
    if not isinstance(a,u.StaticMeshActor) or not a.static_mesh_component.is_visible() or a.get_editor_property('hidden') or str(a.get_folder_path()).startswith(('00_Ground','95_Backdrop')):continue
    c,e=a.get_actor_bounds(False);gap=math.hypot(max(0,abs(c.x-x)-e.x),max(0,abs(c.y-y)-e.y))
    if gap<200:nearby.append({'actor':a.get_name(),'label':a.get_actor_label(),'gap':gap,'center':list(c.to_tuple()),'extent':list(e.to_tuple())})
   rows.append({'point':[x,y],'projected':list(p.to_tuple()),'nearby':nearby})
 D[key]={'projected_samples':count,'unreachable':rows}
 (ROOT/'Saved/MapSceneryAudit/Connectivity.json').write_text(json.dumps(D,indent=2))
