"""Local, editor-only render review queue; avoids restarting Unreal per variant."""
from pathlib import Path
import json,time,traceback,unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/MapSceneryAudit/RenderSession';OUT.mkdir(parents=True,exist_ok=True)
A=u.get_editor_subsystem(u.EditorActorSubsystem);L=u.get_editor_subsystem(u.LevelEditorSubsystem)
S={'last':None,'stage':'idle'}
def reply(data):
 (OUT/'result.json').write_text(json.dumps(data,indent=2))
def tick(dt):
 if S.get('busy'):return
 S['busy']=True
 try:
  now=time.monotonic()
  if S['stage']=='idle':
   request=OUT/'request.json'
   if not request.exists():return
   job=json.loads(request.read_text())
   if job['id']==S['last']:return
   S['last']=job['id'];S['job']=job
   if job.get('quit'):
    reply({'id':job['id'],'done':True});u.unregister_slate_post_tick_callback(S['handle']);u.SystemLibrary.quit_editor();return
   if job.get('script'):exec(compile((ROOT/job['script']).read_text(),job['script'],'exec'),{'__name__':'render_job','ROOT':ROOT,'u':u,'A':A,'L':L})
   if not job.get('camera'):reply({'id':job['id'],'done':True});return
   pos=u.Vector(*job['camera']);rot=u.MathLibrary.find_look_at_rotation(pos,u.Vector(*job.get('target',[0,0,0])))
   cam=A.spawn_actor_from_class(u.CameraActor,pos,rot);cam.get_component_by_class(u.CameraComponent).set_field_of_view(job.get('fov',95))
   L.editor_set_viewport_realtime(True);L.editor_set_game_view(True);L.pilot_level_actor(cam)
   S.update(stage='warm',camera=cam,at=now,primed=False)
  elif S['stage']=='warm' and now-S['at']>7:
   job=S['job'];S['shot']=u.AutomationLibrary.take_high_res_screenshot(1600,1000,str(ROOT/job['image']),camera=S['camera'])
   if not S['primed']:S.update(primed=True,at=now-3);return
   S.update(stage='capture',at=now)
  elif S['stage']=='capture' and now-S['at']>3:
   L.eject_pilot_level_actor();A.destroy_actor(S['camera']);reply({'id':S['last'],'done':True,'image':S['job']['image']});S['stage']='idle'
 except Exception:
  reply({'id':S.get('last'),'error':traceback.format_exc()});S['stage']='idle'
 finally:S['busy']=False
reply({'ready':True})
u.EditorPythonScripting.set_keep_python_script_alive(True);S['handle']=u.register_slate_post_tick_callback(tick)
