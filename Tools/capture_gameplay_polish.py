"""Unsaved real-map visual review. Run in a rendered offscreen Unreal editor."""
from pathlib import Path
import time,json,traceback,unreal as u
u.EditorPythonScripting.set_keep_python_script_alive(True)
root=Path(u.Paths.project_dir()).resolve();out=root/'Saved/GameplayPolish/Previews';out.mkdir(parents=True,exist_ok=True)
level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
state={'stage':'load','i':0,'at':time.monotonic(),'start':time.monotonic(),'temporary':[],'checks':{}}
views=[('Ground',(3300,0,2950),(1413,0,0)),('WallTrap',(3350,-500,2600),(1413,0,60)),('EmberCircle',(3000,0,2550),(1413,0,30))]
views += [('NinjaDepartureBurst',(1863,-300,550),(1413,0,100)),('SamuraiDepartureBurst',(1863,-300,550),(1413,0,100))]
if '-PolishSmokeOnly' in u.SystemLibrary.get_command_line():
 out=root/'Saved/GameplayPolish/SmokeReview';out.mkdir(parents=True,exist_ok=True)
 views=views[:4];state['i']=3
 assert level.load_level('/Game/Maps/Lvl_B1_Lvl1')
def clear():
 for actor in state['temporary']:
  if u.SystemLibrary.is_valid(actor):actors.destroy_actor(actor)
 state['temporary']=[]
def tick(dt):
 if state.get('busy'):return
 state['busy']=True
 try:
  if time.monotonic()-state['start']>420:raise RuntimeError('Visual review timeout')
  i=state['i']
  if i>=len(views):
   (out/'Checks.json').write_text(json.dumps(state['checks'],indent=2));u.unregister_slate_post_tick_callback(state['handle']);u.SystemLibrary.quit_editor();return
  name,pos,target=views[i]
  if state['stage']=='load':
   if i==0:
    assert level.load_level('/Game/Maps/Lvl_B1_Lvl1')
    floor=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='Plane')
    ground_material=floor.get_component_by_class(u.StaticMeshComponent).get_material(0).get_path_name()
    state['checks']['actual_ground_material']=ground_material
    assert 'MI_Lvl1_Ground' in ground_material,ground_material
   clear()
   state['fx']=None
   if i in (1,2):
    event=actors.spawn_actor_from_class(u.SurvivalFieldObjective,u.Vector(1413,0,5));state['temporary'].append(event)
    if i==1:
     spawner=next(a for a in actors.get_all_level_actors() if isinstance(a,u.EnemySpawner))
     ok=event.initialize_wall_trap(spawner);state['checks']['full_wall_ring_fits_start']=ok;assert ok,'Wall placement failed at actual player start'
     walls=[a for a in actors.get_all_level_actors() if isinstance(a,u.EncounterWall)]
     state['checks']['wall_segments']=len(walls);assert len(walls)==51
     for wall in walls:wall.set_sealed(True);state['temporary'].append(wall)
    else:event.initialize_hold_zone()
    for text in event.get_components_by_class(u.TextRenderComponent):text.set_world_rotation(u.MathLibrary.find_look_at_rotation(text.get_world_location(),u.Vector(*pos)),False,False)
   elif i>=3:
    actor=actors.spawn_actor_from_class(u.NiagaraActor,u.Vector(1413,0,20));state['temporary'].append(actor)
    fx=actor.get_component_by_class(u.NiagaraComponent);fx.set_asset(u.load_asset('/Game/HeavensDivide/VFX/Swap/NS_'+('Ninja' if i==3 else 'Samurai')+'Departure'))
    fx.set_force_solo(True);state['fx']=fx
   camera=actors.spawn_actor_from_class(u.CameraActor,u.Vector(*pos),u.MathLibrary.find_look_at_rotation(u.Vector(*pos),u.Vector(*target)))
   state['temporary'].append(camera);c=camera.get_component_by_class(u.CameraComponent);c.set_field_of_view(60);c.set_editor_property('post_process_blend_weight',0.)
   level.editor_set_viewport_realtime(True);level.editor_set_game_view(True);level.pilot_level_actor(camera)
   state.update(camera=camera,stage='warm',at=time.monotonic())
  elif state['stage']=='warm' and time.monotonic()-state['at']>10:
   if state['fx']:
    state['fx'].set_paused(False);state['fx'].reinitialize_system();state['fx'].activate(True);state['fx'].advance_simulation(6,.025);state['fx'].set_paused(True)
   state.update(stage='ready',at=time.monotonic())
  elif state['stage']=='ready' and time.monotonic()-state['at']>1:
   state['task']=u.AutomationLibrary.take_high_res_screenshot(1600,1000,str(out/(name+'.png')),camera=state['camera']);state.update(stage='capture',at=time.monotonic())
  elif state['stage']=='capture' and time.monotonic()-state['at']>3 and (out/(name+'.png')).exists():
   level.eject_pilot_level_actor();state.update(i=i+1,stage='load')
 except Exception:
  (out/'Error.txt').write_text(traceback.format_exc());u.log_error(traceback.format_exc());u.unregister_slate_post_tick_callback(state['handle']);u.SystemLibrary.quit_editor()
 finally:state['busy']=False
state['handle']=u.register_slate_post_tick_callback(tick)
