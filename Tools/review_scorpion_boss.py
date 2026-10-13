"""Render imported poses and prove both montages play before optional activation.

Run with -RenderOffscreen -ExecutePythonScript=<this file> -ScorpionActivate
to activate after this review. No preview level or transient actors are saved.
"""
import hashlib
import json
import runpy
import time
import traceback
from pathlib import Path
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/ScorpionBoss';OUT.mkdir(parents=True,exist_ok=True)
if '-ScorpionImportFirst' in u.SystemLibrary.get_command_line():runpy.run_path(str(ROOT/'Tools/import_scorpion_boss.py'))
assets=json.loads((OUT/'ImportedAssets.json').read_text())
ACTORS=u.get_editor_subsystem(u.EditorActorSubsystem);LEVEL=u.get_editor_subsystem(u.LevelEditorSubsystem)
STATE={'stage':'begin','index':0,'at':time.monotonic(),'started':time.monotonic(),'temporary':[],'report':{'poses':{},'montages':{}},'handle':None}
rig=json.loads((OUT/'RiggingReport.json').read_text())
POSES=[]
for label,fraction in [('Idle',0),('Walk',.25),('Walk',.75),('Cast',.25),('Cast',.75),('Death',.25),('Death',.625),('Death',1)]:
    reference=next(s for s in rig['clips'][label]['samples'] if s['fraction']==fraction)
    POSES.append((label,reference['time'],reference))
def vec(v):return [v.x,v.y,v.z]
def finish(error=None):
    if STATE.get('closed'):return
    STATE['closed']=True
    if STATE.get('texture'):STATE['texture'].set_editor_property('never_stream',STATE['old_stream'])
    for actor in STATE['temporary']:
        if actor:ACTORS.destroy_actor(actor)
    if STATE['handle']:u.unregister_slate_post_tick_callback(STATE['handle'])
    if error:(OUT/'VisualError.txt').write_text(error);u.log_error(error)
    u.EditorPythonScripting.set_keep_python_script_alive(False)
    u.SystemLibrary.quit_editor()
def begin():
    assert LEVEL.load_level('/Game/Maps/Lvl_B1_Arena')
    origin=u.Vector(15000,15000,0)
    actor=ACTORS.spawn_actor_from_class(u.SkeletalMeshActor,origin,u.Rotator(yaw=assets['component_yaw']));STATE['temporary'].append(actor)
    comp=actor.get_component_by_class(u.SkeletalMeshComponent);comp.set_skeletal_mesh_asset(u.load_asset(assets['mesh']))
    comp.set_forced_lod(1);comp.set_update_animation_in_editor(True)
    camera=ACTORS.spawn_actor_from_class(u.SceneCapture2D,origin+u.Vector(900,-1100,1000),u.Rotator());STATE['temporary'].append(camera)
    camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(camera.get_actor_location(),origin+u.Vector(0,0,280)),False)
    capture=camera.get_component_by_class(u.SceneCaptureComponent2D)
    capture.set_editor_property('projection_type',u.CameraProjectionMode.ORTHOGRAPHIC);capture.set_editor_property('ortho_width',1000)
    capture.set_editor_property('capture_source',u.SceneCaptureSource.SCS_BASE_COLOR)
    capture.set_editor_property('capture_every_frame',False);capture.set_editor_property('capture_on_movement',False)
    capture.set_editor_property('primitive_render_mode',u.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST);capture.show_only_actor_components(actor)
    target=u.RenderingLibrary.create_render_target2d(actor,900,900,u.TextureRenderTargetFormat.RTF_RGBA8);capture.set_editor_property('texture_target',target)
    texture=u.load_asset(assets['texture']);STATE['old_stream']=texture.get_editor_property('never_stream');texture.set_editor_property('never_stream',True)
    STATE.update(actor=actor,comp=comp,capture=capture,target=target,texture=texture,origin=origin)
    u.AutomationLibrary.finish_loading_before_screenshot();STATE.update(stage='pose',at=time.monotonic())
def setup_pose():
    label,seconds,reference=POSES[STATE['index']];comp=STATE['comp']
    comp.play_animation(u.load_asset(assets['animations'][label]),False);comp.set_position(seconds,False);comp.stop()
    assert u.SwapVFXSetupLibrary.refresh_preview_animation(comp)
    comp.set_cast_shadow(False);comp.set_cast_shadow(True)
    assert abs(comp.get_position()-seconds)<.01
    STATE.update(stage='capture',at=time.monotonic())
def capture_pose():
    label,seconds,reference=POSES[STATE['index']];comp=STATE['comp'];camera=STATE['capture'];target=STATE['target']
    camera.capture_scene();pixels=u.RenderingLibrary.read_render_target(STATE['actor'],target,False)
    coverage=sum(bool(p.r or p.g or p.b) for p in pixels);assert coverage>18000,(label,'Scorpion did not render',coverage)
    filename=f'UE_{label}_{round(seconds*100):03}.png'
    u.RenderingLibrary.export_render_target(STATE['actor'],target,str(OUT),filename)
    image=OUT/filename;assert image.exists() and image.stat().st_size>5000
    head=comp.get_socket_location('Head');body=comp.get_socket_location('Thorax')
    if label=='Idle':assert head.x>body.x,('Model faces backward',head,body)
    bones={n:vec(comp.get_socket_location(n)-STATE['origin']) for n in ['root','Thorax','Head','Foot_L1','Foot_R2','Tail_Tip','Claw_L_Outer']}
    height_errors={n:abs(v[2]-reference['bones_cm'][n][2]) for n,v in bones.items()}
    assert max(height_errors.values())<1,(label,'Imported bone heights differ from authored rig',height_errors)
    STATE['report']['poses'][filename]=dict(animation=label,time=seconds,coverage=coverage,bones=bones,height_errors_cm=height_errors,sha256=hashlib.sha256(image.read_bytes()).hexdigest())
    STATE['index']+=1;STATE.update(stage='pose' if STATE['index']<len(POSES) else 'montages',at=time.monotonic())
def montages():
    comp=STATE['comp'];comp.set_anim_instance_class(u.load_asset(assets['anim_blueprint']).generated_class())
    assert u.SwapVFXSetupLibrary.refresh_preview_animation(comp)
    instance=comp.get_anim_instance();assert instance
    idle_body=comp.get_socket_location('Thorax');idle_claw=comp.get_socket_location('Claw_L_Outer')
    for label,seconds in [('Cast',.55),('Death',1.4)]:
        instance.montage_stop(0);clip=u.load_asset(assets['montages'][label]);assert instance.montage_play(clip)>0
        assert u.SwapVFXSetupLibrary.refresh_preview_animation(comp,seconds)
        assert instance.montage_is_playing(clip)
        body=comp.get_socket_location('Thorax');claw=comp.get_socket_location('Claw_L_Outer')
        if label=='Cast':assert claw.distance(idle_claw)>5,('Montage did not animate the claws',claw,idle_claw)
        else:assert body.z<idle_body.z-35,('Montage did not collapse the body',body,idle_body)
        STATE['report']['montages'][label]=dict(plays=True,time=seconds,body=vec(body-STATE['origin']),claw=vec(claw-STATE['origin']))
    STATE['report']['assets']=assets
    (OUT/'UnrealVisualValidation.json').write_text(json.dumps(STATE['report'],indent=2))
    for a in STATE['temporary']:ACTORS.destroy_actor(a)
    STATE['temporary']=[]
    if '-ScorpionActivate' in u.SystemLibrary.get_command_line():runpy.run_path(str(ROOT/'Tools/activate_scorpion_boss.py'))
    u.log('SCORPION_UNREAL_VISUAL_REVIEW_PASSED');finish()
def tick(delta):
    # Level loading and render flushes pump Slate; never re-enter a review step.
    if STATE.get('busy') or STATE.get('closed'):return
    STATE['busy']=True
    try:
        if time.monotonic()-STATE['started']>480:raise TimeoutError('Scorpion review deadline exceeded')
        if STATE['stage']=='begin':begin()
        elif STATE['stage']=='pose':setup_pose()
        elif STATE['stage']=='capture' and time.monotonic()-STATE['at']>1.5:capture_pose()
        elif STATE['stage']=='montages':montages()
    except Exception:finish(traceback.format_exc())
    finally:STATE['busy']=False
u.EditorPythonScripting.set_keep_python_script_alive(True)
STATE['handle']=u.register_slate_post_tick_callback(tick)
