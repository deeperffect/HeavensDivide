"""Saved-map route/collision checks, rendered previews and real PIE wave checks."""
from pathlib import Path
import sys,json,time,traceback,math,re,shutil
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools'))
from survival_map_layout import MAPS,boundary,playable
OUT=ROOT/'Art/Environment/SurvivalMaps/Previews';OUT.mkdir(parents=True,exist_ok=True)
REPORT=ROOT/'Saved/SurvivalMaps'/globals().get('REPORT_NAME','Validation.json')
L=u.get_editor_subsystem(u.LevelEditorSubsystem);A=u.get_editor_subsystem(u.EditorActorSubsystem)
u.EditorPythonScripting.set_keep_python_script_alive(True)
keys=globals().get('MAP_KEYS',list(MAPS));S={'stage':'load','index':0,'start':time.monotonic()};R={'passed':False,'maps':{}}

def finish():
 REPORT.write_text(json.dumps(R,indent=2));L.editor_request_end_play()
 u.unregister_slate_post_tick_callback(S['handle']);u.SystemLibrary.quit_editor()

def validate_editor(key):
 cfg=MAPS[key];actors=A.get_all_level_actors();world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
 for cls,count in [(u.PlayerStart,1),(u.EnemySpawner,1),(u.RunObjectiveDirector,1),(u.EncounterDirector,1),(u.BossToriiGate,1),(u.ObjectiveSpawnPoint,8),(u.RecastNavMesh,1)]:
  assert sum(isinstance(a,cls) for a in actors)==count,(key,str(cls))
 nav=next(a for a in actors if isinstance(a,u.RecastNavMesh));assert nav.get_editor_property('runtime_generation')==u.RuntimeGenerationType.STATIC
 gate=next(a for a in actors if isinstance(a,u.BossToriiGate));assert gate.get_editor_property('unlock_run_time_seconds')==600.
 for a in actors:
  if a.actor_has_tag('SurvivalMapBackdrop'):
   for c in a.get_components_by_class(u.PrimitiveComponent):
    assert c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION
    assert not c.get_editor_property('can_ever_affect_navigation')
 grounded=[a for a in actors if a.actor_has_tag('SurvivalGrounded')]
 for a in grounded:
  c,e=a.get_actor_bounds(False)
  assert abs(c.z-e.z)<.3,(key,'floating grounded prop',a.get_actor_label(),c.z-e.z)
 instance_count=sum(c.get_instance_count() for a in actors for c in a.get_components_by_class(u.InstancedStaticMeshComponent))
 for a in actors:
  if str(a.get_folder_path())=='95_Backdrop':continue
  for c in a.get_components_by_class(u.InstancedStaticMeshComponent):
   bounds=c.static_mesh.get_bounds();width=max(bounds.box_extent.x*2,bounds.box_extent.y*2,1.)
   for i in range(c.get_instance_count()):
    t=c.get_instance_transform(i,True)
    if a.actor_has_tag('SurvivalInsetPaving'):
     assert bounds.box_extent.z*2*t.scale3d.z<1 and c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION
    else:assert width*t.scale3d.x<=160.1,(key,'oversized groundcover',a.get_actor_label())
 meshes={a.static_mesh_component.static_mesh.get_path_name() for a in actors if isinstance(a,u.StaticMeshActor) and a.static_mesh_component.static_mesh}
 result={'routes':[],'ground':[],'boundary_sweeps':[],'actors':len(actors),'grounded_props_checked':len(grounded),'scenery_instances':instance_count,'unique_static_meshes':len(meshes),'gate_seconds':600.,'navigation':'Static'}
 x,y=cfg['start'];start=u.NavigationSystemV1.project_point_to_navigation(world,u.Vector(x,y,0),None,None,query_extent=u.Vector(300,300,120));assert start
 for x,y in cfg['goals']:
  p=u.NavigationSystemV1.project_point_to_navigation(world,u.Vector(x,y,0),None,None,query_extent=u.Vector(350,350,120));assert p
  path=u.NavigationSystemV1.find_path_to_location_synchronously(world,start,p,None,None)
  at_start=math.hypot(p.x-start.x,p.y-start.y)<1
  good=at_start or bool(path and path.is_valid() and not path.is_partial());assert good,(key,'unreachable clearing',x,y)
  result['routes'].append({'goal':[x,y],'path_points':len(path.path_points) if path else 0});result['ground'].append([x,y])
 # Boundaries block a player capsule going out of the arena.
 points=boundary(key)
 for i in range(0,len(points),max(1,len(points)//6)):
  a=points[i];b=points[(i+1)%len(points)];x=(a[0]+b[0])/2;y=(a[1]+b[1])/2
  d=math.hypot(x,y);nx=x/d;ny=y/d
 boundaries=[a for a in actors if a.get_actor_label().startswith('Sealed play boundary')]
 assert boundaries and all(a.static_mesh_component.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN)==u.CollisionResponseType.ECR_BLOCK for a in boundaries)
 result['occluders']=sum(any(str(t)=='CameraFade' for t in a.static_mesh_component.get_editor_property('component_tags')) for a in actors if isinstance(a,u.StaticMeshActor))
 assert result['occluders']>20
 if globals().get('EXTRA_EDITOR_VALIDATION'):result['extra_editor']=EXTRA_EDITOR_VALIDATION(key,world,actors)
 return result

def start_view(key,index):
 cfg=MAPS[key];world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
 if index==0:
  pos=u.Vector(-6000,-6000,11000);target=u.Vector(0,0,0);fov=95
 elif index<=len(cfg['views']):
  x,y=cfg['views'][index-1];pos=u.Vector(x-2300,y-2100,3400);target=u.Vector(x,y,0);fov=60
 else:
  view=globals().get('EXTRA_VIEWS',{}).get(key,[])[index-len(cfg['views'])-1]
  pos=u.Vector(*view['position']);target=u.Vector(*view['target']);fov=view.get('fov',60)
 camera=A.spawn_actor_from_class(u.CameraActor,pos,u.MathLibrary.find_look_at_rotation(pos,target))
 cc=camera.get_component_by_class(u.CameraComponent);cc.set_field_of_view(fov);cc.set_editor_property('post_process_blend_weight',0.)
 L.editor_set_viewport_realtime(True);L.editor_set_game_view(True);L.pilot_level_actor(camera)
 u.SystemLibrary.execute_console_command(world,'r.ViewDistanceScale '+('4' if index==0 else '1'))
 u.AutomationLibrary.finish_loading_before_screenshot()
 S.update(camera=camera,view_pos=pos,view_rot=u.MathLibrary.find_look_at_rotation(pos,target),stage='warm',at=time.monotonic())

def tick(dt):
 if S.get('busy'):return
 S['busy']=True
 try:
  now=time.monotonic();assert now-S['start']<1000,'Review timeout'
  if S['stage']=='automation':
   if now-S['at']<5:return
   log=(ROOT/'Saved/OldVillage_review_survival_maps.log').read_text(encoding='utf-8',errors='replace')
   results=re.findall(r'Test Completed\. Result=\{([^}]+)\} Name=\{([^}]+)\}',log[S['log_start']:])
   if len(results)<3:return
   R['automation']=[{'name':n,'result':v} for v,n in results]
   assert all(v=='Success' for v,n in results),R['automation']
   assert not re.search(r'Failed to compile Material[^\r\n]*SurvivalMaps|SurvivalMaps[^\r\n]*Failed to compile Material',log),'Reworked map material compilation failed'
   for size in ['1280','1600']:
    src=ROOT/'Saved/RunPreparation/Previews'/('NewRunDefault'+size+'.png')
    if src.exists():shutil.copy2(src,OUT/('DestinationMenu'+size+'.png'))
   R['passed']=True;u.log('SURVIVAL_MAPS_VALIDATED');finish();return
  key=keys[S['index']];cfg=MAPS[key]
  if S['stage']=='load':
   assert L.load_level('/Game/Maps/Lvl_'+key)
   R['maps'][key]=validate_editor(key);S.update(view=0,stage='view',overview_primed=False)
   # Unsaved accelerated scheduling verifies the directors actually operate.
   actors=A.get_all_level_actors()
   director=next(a for a in actors if isinstance(a,u.RunObjectiveDirector))
   director.set_editor_property('blood_shrine_spawn_time',2.);director.set_editor_property('second_blood_shrine_spawn_time',4.)
   encounter=next(a for a in actors if isinstance(a,u.EncounterDirector));encounter.set_editor_property('first_urn_seconds',2.)
   REPORT.write_text(json.dumps(R,indent=2))
  if S['stage']=='view':start_view(key,S['view']);return
  if S['stage']=='prime' and now-S['at']>3:S.update(stage='warm',at=now-7)
  if S['stage']=='warm' and now-S['at']>6:
   S['camera'].set_actor_location_and_rotation(S['view_pos'],S['view_rot'],False,True)
   u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(S['view_pos'],S['view_rot'])
   suffix=(['Overview','Clearing','DistrictA','DistrictB']+[v['name'] for v in globals().get('EXTRA_VIEWS',{}).get(key,[])])[S['view']]
   S['shot']=u.AutomationLibrary.take_high_res_screenshot(1600,1000,str(OUT/(key+'_'+suffix+'.png')),camera=S['camera'])
   if S['view']==0 and not S['overview_primed']:S.update(stage='prime',overview_primed=True,at=now);return
   S.update(stage='shot',at=now);return
  if S['stage']=='shot' and now-S['at']>3:
   L.eject_pilot_level_actor();A.destroy_actor(S['camera'])
   if S['view']<len(cfg['views'])+len(globals().get('EXTRA_VIEWS',{}).get(key,[])) and not globals().get('OVERVIEW_ONLY',False):S.update(view=S['view']+1,stage='view');return
   if globals().get('PREVIEW_ONLY',False):S.update(stage='ended',at=now);return
   u.SystemLibrary.execute_console_command(u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world(),'r.ViewDistanceScale 1')
   L.editor_request_begin_play();S.update(stage='play',at=now,samples=[]);return
  if S['stage'] in ['warm','shot','view','prime']:return
  if S['stage']=='ended':
   if L.is_in_play_in_editor():return
   S['index']+=1
   if S['index']==len(keys):
    if globals().get('SKIP_AUTOMATION',False):R['passed']=True;finish();return
    assert L.load_level('/Engine/Maps/Entry')
    S.update(stage='automation',at=now,log_start=len((ROOT/'Saved/OldVillage_review_survival_maps.log').read_text(encoding='utf-8',errors='replace')))
    u.SystemLibrary.execute_console_command(u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world(),'Automation RunTests HeavensDivide.Progression.RunPreparation+HeavensDivide.UI.RunPreparationAndMinimap+HeavensDivide.Progression.RunReports')
    return
   S['stage']='load';return
  world=u.EditorLevelLibrary.get_game_world()
  if not world:return
  for character in u.GameplayStatics.get_all_actors_of_class(world,u.CharacterBase):
   health=character.get_component_by_class(u.HealthComponent)
   if health:health.set_damage_enabled(False)
  sp=u.GameplayStatics.get_all_actors_of_class(world,u.EnemySpawner)[0];elapsed=sp.get_run_time_seconds()
  if S['stage']=='play' and elapsed>(6 if not S['samples'] else 10):
   pc=u.GameplayStatics.get_player_controller(world,0);enemies=u.GameplayStatics.get_all_actors_of_class(world,u.EnemyBase)
   sample={'seconds':elapsed,'controller_seconds':pc.get_run_time_seconds(),'enemies':len(enemies),'pawn':bool(u.GameplayStatics.get_player_character(world,0))}
   assert sample['pawn'] and sample['enemies']>0 and sample['controller_seconds']==elapsed,(key,sample)
   gate=u.GameplayStatics.get_all_actors_of_class(world,u.BossToriiGate)[0];assert gate.get_gate_state()==u.BossToriiGateState.LOCKED
   S['samples'].append(sample)
   if len(S['samples'])==1:return
   physics={'ground':[],'boundaries':[]}
   characters=u.GameplayStatics.get_all_actors_of_class(world,u.Character)
   for x,y in cfg['goals']:
    hit=u.SystemLibrary.line_trace_single(world,u.Vector(x,y,500),u.Vector(x,y,-1000),u.TraceTypeQuery.ECC_ENEMY_GROUND,False,[],u.DrawDebugTrace.NONE,False)
    physics['ground'].append(bool(hit));assert hit,(key,'missing objective floor',x,y)
   p=boundary(key)
   for i in range(0,len(p),max(1,len(p)//6)):
    a=p[i];b=p[(i+1)%len(p)];x=(a[0]+b[0])/2;y=(a[1]+b[1])/2;d=math.hypot(x,y);nx=x/d;ny=y/d
    hit=u.SystemLibrary.capsule_trace_single_by_profile(world,u.Vector(x-nx*450,y-ny*450,110),u.Vector(x+nx*450,y+ny*450,110),45,95,'Pawn',False,characters,u.DrawDebugTrace.NONE,False)
    physics['boundaries'].append(bool(hit));assert hit,(key,'passable boundary',i)
   R['maps'][key]['physics']=physics
   if globals().get('EXTRA_GAMEPLAY_VALIDATION'):R['maps'][key]['extra_gameplay']=EXTRA_GAMEPLAY_VALIDATION(key,world)
   shrines=u.GameplayStatics.get_all_actors_of_class(world,u.BloodShrine);assert len(shrines)==2,(key,'scheduled shrines',len(shrines))
   R['maps'][key]['gameplay']={'samples':S['samples'],'shrines':len(shrines),'gate_locked_before_ten_minutes':True}
   u.SystemLibrary.execute_console_command(world,'HighResShot 1600x1000 filename="'+str(OUT/(key+'_Gameplay.png')).replace('\\','/')+'"')
   S.update(stage='game_shot',at=now);REPORT.write_text(json.dumps(R,indent=2));return
  if S['stage']=='game_shot' and now-S['at']>3:
   L.editor_request_end_play();S.update(stage='ended',at=now)
 except Exception:R['error']=traceback.format_exc();u.log_error(R['error']);finish()
 finally:S['busy']=False
S['handle']=u.register_slate_post_tick_callback(tick)
