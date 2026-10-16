"""Reload repaired maps, audit visible collision, scan walkability and run PIE."""
from pathlib import Path
import sys,json,math,time,traceback
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools'))
from survival_map_layout import MAPS,boundary,playable,inside_polygon,segment_distance
from repair_map_scenery import foliage,DEST
A=u.get_editor_subsystem(u.EditorActorSubsystem);L=u.get_editor_subsystem(u.LevelEditorSubsystem);SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
OUT=ROOT/'Saved/MapSceneryAudit';PRE=ROOT/'Art/Environment/SceneryRepair/Previews';PRE.mkdir(parents=True,exist_ok=True)
KEYS=globals().get('MAP_KEYS',['OldVillage','MoonlitForest','SunkenSanctuary','EmberQuarry'])
S={'stage':'load','i':0,'start':time.monotonic()};D={'passed':False,'maps':{}}
village=json.loads((ROOT/'Tools/old_village_layout.json').read_text());VBOUND=[(n*village['layout_scale'],e*village['layout_scale']) for e,n in village['land']]
def polygon(key):return VBOUND if key=='OldVillage' else boundary(key)
def allowed(key,x,y):return inside_polygon(x,y,VBOUND) if key=='OldVillage' else playable(key,x,y)
def finish():
 (OUT/globals().get('REPORT_NAME','Validation.json')).write_text(json.dumps(D,indent=2));L.editor_request_end_play();u.unregister_slate_post_tick_callback(S['handle']);u.SystemLibrary.quit_editor()
def editor(key):
 actors=A.get_all_level_actors();w=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();rows={'solid_components':0,'foliage_components':0,'visible_generated_geometry':[],'hidden_blockers':[],'routes':[]}
 assert sum(isinstance(a,u.EnemySpawner) for a in actors)==1
 assert sum(isinstance(a,u.RunObjectiveDirector) for a in actors)==1
 assert sum(isinstance(a,u.EncounterDirector) for a in actors)==1
 gate=next(a for a in actors if isinstance(a,u.BossToriiGate));assert gate.get_editor_property('unlock_run_time_seconds')==600
 nav=next(a for a in actors if isinstance(a,u.RecastNavMesh));assert nav.get_editor_property('runtime_generation')==u.RuntimeGenerationType.STATIC
 for a in actors:
  for c in a.get_components_by_class(u.StaticMeshComponent):
   m=c.static_mesh
   if not m:continue
   vis=c.is_visible() and not a.get_editor_property('hidden');name=m.get_name();label=a.get_actor_label()
   if not vis:
    if c.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN)==u.CollisionResponseType.ECR_BLOCK and c.get_collision_enabled()!=u.CollisionEnabled.NO_COLLISION:
     assert any(t in label.lower() for t in ['ground collision','boundary','pond collision','flooded basin','tower solid core','compound bamboo']),('Unexplained invisible blocker',key,label)
     rows['hidden_blockers'].append(label)
    continue
   if name.startswith(('SM_OV_Island','SM_OV_Water.','SM_OV_Countryside','SM_Quarry_EdgeApron')) or name in ['SM_'+key+'_Ground','SM_'+key+'_Surroundings'] or m.get_path_name().startswith('/Engine/BasicShapes/'):
    rows['visible_generated_geometry'].append(label)
   if foliage(c):
    rows['foliage_components']+=1
    if not name.startswith('SM_TreeBamboo'):assert c.get_editor_property('evaluate_world_position_offset'),(key,'disabled foliage positioning',label)
   if c.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN)==u.CollisionResponseType.ECR_BLOCK and c.get_collision_enabled()!=u.CollisionEnabled.NO_COLLISION:
    assert c.get_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY)==u.CollisionResponseType.ECR_BLOCK,(key,'enemy mismatch',label)
    rows['solid_components']+=1
 assert not rows['visible_generated_geometry'],rows['visible_generated_geometry']
 start=next(a for a in actors if isinstance(a,u.PlayerStart)).get_actor_location();start.z=0
 p=u.NavigationSystemV1.project_point_to_navigation(w,start,None,None,query_extent=u.Vector(300,300,150));assert p
 for a in actors:
  if not isinstance(a,u.ObjectiveSpawnPoint):continue
  q=u.NavigationSystemV1.project_point_to_navigation(w,a.get_actor_location(),None,None,query_extent=u.Vector(200,200,150));assert q,(key,'clearing off nav',a.get_actor_label())
  path=u.NavigationSystemV1.find_path_to_location_synchronously(w,p,q,None,None)
  assert math.dist(p.to_tuple(),q.to_tuple())<1 or (path and path.is_valid() and not path.is_partial()),(key,'unreachable objective',a.get_actor_label())
  rows['routes'].append(a.get_actor_label())
 return rows
def physics(key,w):
 actors=u.GameplayStatics.get_all_actors_of_class(w,u.Actor);ignored=[a for a in actors if isinstance(a,u.Character)]
 result={'grid_samples':0,'open_samples':0,'blocked_samples':0,'nav_missing':[],'hidden_contacts':{},'boundary_probes':[],'boundary_missing':[],'reachable_grid_samples':0}
 bounds=polygon(key);xs=[p[0] for p in bounds];ys=[p[1] for p in bounds];free=[]
 for x in range(math.ceil(min(xs)/225)*225,int(max(xs)),225):
  for y in range(math.ceil(min(ys)/225)*225,int(max(ys)),225):
   if not allowed(key,x,y) or min(segment_distance((x,y),a,b) for a,b in zip(bounds,bounds[1:]+bounds[:1]))<220:continue
   result['grid_samples']+=1
   hit=u.SystemLibrary.capsule_trace_single_by_profile(w,u.Vector(x,y,120),u.Vector(x+1,y,120),90,95,'Pawn',False,ignored,u.DrawDebugTrace.NONE,False)
   if hit:
    result['blocked_samples']+=1
    continue
   result['open_samples']+=1
   q=u.NavigationSystemV1.project_point_to_navigation(w,u.Vector(x,y,0),None,None,query_extent=u.Vector(80,80,120))
   if q:free.append(q)
   else:result['nav_missing'].append([x,y])
 # Query every boundary segment at three locations, including near its joins.
 for i,(a,b) in enumerate(zip(bounds,bounds[1:]+bounds[:1])):
  dx=b[0]-a[0];dy=b[1]-a[1];length=math.hypot(dx,dy);nx=dy/length;ny=-dx/length
  if nx*(a[0]+b[0])+ny*(a[1]+b[1])<0:nx=-nx;ny=-ny
  for t in [.1,.5,.9]:
   x=a[0]+dx*t;y=a[1]+dy*t
   hit=u.SystemLibrary.capsule_trace_single_by_profile(w,u.Vector(x-nx*500,y-ny*500,110),u.Vector(x+nx*600,y+ny*600,110),45,95,'Pawn',False,ignored,u.DrawDebugTrace.NONE,False)
   if hit:result['boundary_probes'].append([i,t])
   else:result['boundary_missing'].append([i,t])
 pawn=u.GameplayStatics.get_player_character(w,0);start=pawn.get_actor_location()
 goals=free if globals().get('EXHAUSTIVE_ROUTES',False) else free[::max(1,len(free)//32)]
 result['unreachable_open_points']=[]
 for q in goals:
  path=u.NavigationSystemV1.find_path_to_location_synchronously(w,start,q,None,None)
  if path and path.is_valid() and not path.is_partial():result['reachable_grid_samples']+=1
  else:result['unreachable_open_points'].append(list(q.to_tuple()))
 result['sampled_navigation_goals']=len(goals)
 return result
def views(key):
 rows=[('Overview',(-6500,-6500,11500),(0,0,0),95)]
 if globals().get('OVERVIEWS_ONLY',False):return rows
 if key=='SunkenSanctuary':rows += [('Leaves',(-9200,2300,2350),(-7700,3400,650),45),('Boundary',(8700,0,2000),(7900,0,100),70)]
 elif key=='OldVillage':rows += [('Village',(-1400,-3000,2600),(200,-2000,100),65),('Waterfront',(-7600,-7600,3000),(-6400,-6400,0),70)]
 elif key=='MoonlitForest':rows += [('Forest',(-4100,-4400,2400),(-2900,-3000,150),70),('Boundary',(7600,0,2000),(6900,0,100),70)]
 else:rows += [('Quarry',(3750,-2300,1850),(2000,-2300,100),70),('Boundary',(8400,-1800,1650),(7700,-1800,100),72)]
 return rows
def tick(dt):
 if S.get('busy'):return
 S['busy']=True
 try:
  now=time.monotonic();assert now-S['start']<1400,'Timed out'
  key=KEYS[S['i']]
  if S['stage']=='load':
   assert L.load_level('/Game/Maps/Lvl_'+key);D['maps'][key]=editor(key);S.update(stage='view',view=0)
   (OUT/globals().get('REPORT_NAME','Validation.json')).write_text(json.dumps(D,indent=2))
   if globals().get('SKIP_VIEWS',False):L.editor_request_begin_play();S.update(stage='play',at=now);return
  if S['stage']=='view':
   suffix,pos,target,fov=views(key)[S['view']];pos=u.Vector(*pos);rot=u.MathLibrary.find_look_at_rotation(pos,u.Vector(*target))
   cam=A.spawn_actor_from_class(u.CameraActor,pos,rot);cam.get_component_by_class(u.CameraComponent).set_field_of_view(fov);L.editor_set_viewport_realtime(True);L.editor_set_game_view(True);L.pilot_level_actor(cam)
   S.update(camera=cam,pos=pos,rot=rot,suffix=suffix,stage='warm',at=now,primed=False);return
  if S['stage']=='warm' and now-S['at']>7:
   S['camera'].set_actor_location_and_rotation(S['pos'],S['rot'],False,True);u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(S['pos'],S['rot'])
   S['shot']=u.AutomationLibrary.take_high_res_screenshot(1600,1000,str(PRE/(key+'_'+S['suffix']+'.png')),camera=S['camera'])
   if not S['primed']:S.update(primed=True,at=now-3);return
   S.update(stage='shot',at=now);return
  if S['stage']=='shot' and now-S['at']>3:
   L.eject_pilot_level_actor();A.destroy_actor(S['camera']);S['view']+=1
   if S['view']<len(views(key)):S['stage']='view';return
   if globals().get('PREVIEW_ONLY',False):S.update(stage='ended',at=now);return
   L.editor_request_begin_play();S.update(stage='play',at=now);return
  if S['stage']=='play':
   w=u.EditorLevelLibrary.get_game_world()
   if not w:return
   for a in u.GameplayStatics.get_all_actors_of_class(w,u.CharacterBase):
    health=a.get_component_by_class(u.HealthComponent)
    if health:health.set_damage_enabled(False)
   sp=u.GameplayStatics.get_all_actors_of_class(w,u.EnemySpawner)[0]
   if sp.get_run_time_seconds()<8:return
   result=physics(key,w);D['maps'][key]['physics']=result
   D['maps'][key]['run_seconds']=sp.get_run_time_seconds();D['maps'][key]['enemies']=len(u.GameplayStatics.get_all_actors_of_class(w,u.EnemyBase))
   assert D['maps'][key]['enemies']>0
   (OUT/globals().get('REPORT_NAME','Validation.json')).write_text(json.dumps(D,indent=2));L.editor_request_end_play();S.update(stage='ended',at=now);return
  if S['stage']=='ended' and not L.is_in_play_in_editor():
   S['i']+=1
   if S['i']==len(KEYS):
    D['passed']=all((globals().get('PREVIEW_ONLY',False) or (not m['physics']['boundary_missing'] and not m['physics']['unreachable_open_points'])) and not m['visible_generated_geometry'] for m in D['maps'].values());finish();return
   S['stage']='load'
 except Exception:D['error']=traceback.format_exc();u.log_error(D['error']);finish()
 finally:S['busy']=False
u.EditorPythonScripting.set_keep_python_script_alive(True);S['handle']=u.register_slate_post_tick_callback(tick)
