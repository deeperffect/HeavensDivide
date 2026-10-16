"""Unsaved before/after material and vertex-positioning comparison."""
from pathlib import Path
import unreal as u, time, traceback, json
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/MapSceneryAudit/Leaves';OUT.mkdir(parents=True,exist_ok=True)
A=u.get_editor_subsystem(u.EditorActorSubsystem);L=u.get_editor_subsystem(u.LevelEditorSubsystem);M=u.MaterialEditingLibrary;E=u.EditorAssetLibrary
u.EditorPythonScripting.set_keep_python_script_alive(True);S={'stage':'load','i':0};R={}
def finish():
 (OUT/'Review.json').write_text(json.dumps(R,indent=2));u.unregister_slate_post_tick_callback(S['handle']);u.SystemLibrary.quit_editor()
def tick(dt):
 if S.get('busy'):return
 S['busy']=True
 try:
  now=time.monotonic()
  if S['stage']=='load':
   assert L.load_level('/Game/Maps/Lvl_SunkenSanctuary')
   a=next(a for a in A.get_all_level_actors() if isinstance(a,u.StaticMeshActor) and a.static_mesh_component.static_mesh and 'SM_Tree_2_Leaves' in a.static_mesh_component.static_mesh.get_name())
   c=a.static_mesh_component;p=a.get_actor_location();target=p+u.Vector(0,0,650);pos=target+u.Vector(-1500,-1100,1700)
   cam=A.spawn_actor_from_class(u.CameraActor,pos,u.MathLibrary.find_look_at_rotation(pos,target));cam.get_component_by_class(u.CameraComponent).set_field_of_view(45)
   L.editor_set_viewport_realtime(True);L.editor_set_game_view(True);L.pilot_level_actor(cam)
   S.update(actor=a,camera=cam,pos=pos,target=target,stage='variant')
   R['tree']=a.get_actor_label();R['position']=list(p.to_tuple())
  if S['stage']=='variant':
   c=S['actor'].static_mesh_component
   if S['i']==1:
    c.set_editor_property('evaluate_world_position_offset',True)
    for i in range(c.get_num_materials()):
     old=c.get_material(i);path='/Game/HeavensDivide/Environment/SceneryRepair/Materials/'+old.get_name()+'_Stationary'
     mat=E.load_asset(path) if E.does_asset_exist(path) else E.duplicate_asset(old.get_path_name(),path)
     for n in M.get_scalar_parameter_names(mat):
      if 'wind' in str(n).lower():M.set_material_instance_scalar_parameter_value(mat,n,0.)
     M.update_material_instance(mat);E.save_loaded_asset(mat,False);c.set_material(i,mat)
   S.update(stage='warm',at=now);return
  if S['stage']=='warm' and now-S['at']>8:
   S['camera'].set_actor_location_and_rotation(S['pos'],u.MathLibrary.find_look_at_rotation(S['pos'],S['target']),False,True)
   u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(S['pos'],u.MathLibrary.find_look_at_rotation(S['pos'],S['target']))
   S['shot']=u.AutomationLibrary.take_high_res_screenshot(1400,1000,str(OUT/(['Before','BillboardsEnabled'][S['i']]+'.png')),camera=S['camera'])
   S.update(stage='shot',at=now);return
  if S['stage']=='shot' and now-S['at']>4:
   S['i']+=1
   if S['i']<2:S['stage']='variant'
   else:R['complete']=True;finish()
 except Exception:R['error']=traceback.format_exc();u.log_error(R['error']);finish()
 finally:S['busy']=False
S['handle']=u.register_slate_post_tick_callback(tick)
