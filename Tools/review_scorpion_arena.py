"""Capture the saved scorpion boss with actual arena lighting; never save previews.

Launch UnrealEditor-Cmd with -RenderOffscreen -ExecutePythonScript=<this file>.
Idle, casting and death frames use the saved mesh, scale, floor and lighting.
"""
import hashlib
import json
import runpy
import struct
import time
import traceback
import uuid
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/ScorpionBoss/ArenaPreview'
if '-ScorpionSizeCorrection' in u.SystemLibrary.get_command_line():
    OUT = ROOT / 'Saved/ScorpionBoss/SizeCorrection/ArenaPreview'
OUT.mkdir(parents=True, exist_ok=True)
MAP_FILE = ROOT / 'Content/Maps/Lvl_B1_Arena.umap'
ASSETS = json.loads((ROOT / 'Saved/ScorpionBoss/ImportedAssets.json').read_text())
if '-ScorpionSizeFollowup' in u.SystemLibrary.get_command_line():
    runpy.run_path(str(ROOT / 'Tools/activate_scorpion_boss.py'))
    runpy.run_path(str(ROOT / 'Tools/verify_level1_lighting.py'))
LEVEL = u.get_editor_subsystem(u.LevelEditorSubsystem)
ACTORS = u.get_editor_subsystem(u.EditorActorSubsystem)
STATE = dict(stage='begin', index=0, busy=False, closed=False, handle=None,
             started=time.monotonic(), temporary=[], views=[])
POSES = [('Idle', 0), ('Cast', .6), ('Death', 2.8)]


def finish(error=None):
    if STATE['closed']:
        return
    STATE['closed'] = True
    LEVEL.eject_pilot_level_actor()
    for actor in reversed(STATE['temporary']):
        ACTORS.destroy_actor(actor)
    if STATE.get('handle'):
        u.unregister_slate_post_tick_callback(STATE['handle'])
    if error:
        (OUT / 'Error.txt').write_text(error)
        u.log_error(error)
    else:
        assert hashlib.sha256(MAP_FILE.read_bytes()).hexdigest() == STATE['map_hash']
        (OUT / 'Validation.json').write_text(json.dumps(dict(
            passed=True, map_unchanged=True, arena_sha256=STATE['map_hash'],
            saved_boss_name=str(STATE['boss'].get_editor_property('boss_display_name')),
            views=STATE['views']), indent=2))
        u.log('SCORPION_ARENA_PREVIEW_PASSED')
    u.EditorPythonScripting.set_keep_python_script_alive(False)
    u.SystemLibrary.quit_editor()


def begin():
    STATE['map_hash'] = hashlib.sha256(MAP_FILE.read_bytes()).hexdigest()
    assert LEVEL.load_level('/Game/Maps/Lvl_B1_Arena')
    boss = next(a for a in ACTORS.get_all_level_actors() if isinstance(a, u.CinderOracleBoss))
    component = boss.get_component_by_class(u.SkeletalMeshComponent)
    assert component.get_editor_property('skeletal_mesh_asset') == u.load_asset(ASSETS['mesh'])
    if '-ScorpionSizeCorrection' in u.SystemLibrary.get_command_line():
        capsule = boss.get_component_by_class(u.CapsuleComponent)
        collision = dict(boss_radius=capsule.get_scaled_capsule_radius(), players={})
        for character in ['Samurai', 'Ninja']:
            blueprint = u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_' + character)
            player = u.get_default_object(blueprint.generated_class())
            player_capsule = player.get_component_by_class(u.CapsuleComponent)
            collision['players'][character] = dict(
                radius=player_capsule.get_scaled_capsule_radius(),
                boss_response=str(capsule.get_collision_response_to_channel(player_capsule.get_collision_object_type())),
                player_response=str(player_capsule.get_collision_response_to_channel(capsule.get_collision_object_type())))
        (OUT / 'CollisionAudit.json').write_text(json.dumps(collision, indent=2))
    component.set_update_animation_in_editor(True)
    ground = boss.get_actor_location() - u.Vector(0, 0, boss.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_half_height())
    size = component.get_editor_property('relative_scale3d').x / .65
    location = ground + u.Vector(800, -1050, 1100) * size
    rotation = u.MathLibrary.find_look_at_rotation(location, ground + u.Vector(0, 0, 190) * size)
    camera = ACTORS.spawn_actor_from_class(u.CameraActor, location, rotation, transient=True)
    STATE['temporary'].append(camera)
    cc = camera.get_component_by_class(u.CameraComponent)
    cc.set_editor_property('projection_mode', u.CameraProjectionMode.ORTHOGRAPHIC)
    cc.set_editor_property('ortho_width', 1350 * size)
    cc.set_editor_property('constrain_aspect_ratio', False)
    cc.set_editor_property('post_process_blend_weight', 0)
    STATE.update(boss=boss, component=component, camera=camera)
    LEVEL.editor_set_game_view(True)
    LEVEL.pilot_level_actor(camera)
    u.AutomationLibrary.finish_loading_before_screenshot()
    STATE.update(stage='pose')


def prepare_pose():
    label, seconds = POSES[STATE['index']]
    component = STATE['component']
    component.play_animation(u.load_asset(ASSETS['animations'][label]), False)
    component.set_position(seconds, False)
    component.stop()
    assert u.SwapVFXSetupLibrary.refresh_preview_animation(component)
    component.set_cast_shadow(False)
    component.set_cast_shadow(True)
    assert abs(component.get_position() - seconds) < .01
    STATE.update(stage='warm', at=time.monotonic())


def capture():
    label, _ = POSES[STATE['index']]
    pending = OUT / ('_' + label + '_' + uuid.uuid4().hex + '.png')
    task = u.AutomationLibrary.take_high_res_screenshot(1600, 900, str(pending), camera=STATE['camera'])
    assert task and task.is_valid_task()
    STATE.update(stage='capture', at=time.monotonic(), pending=pending, task=task)


def tick(delta):
    if STATE['busy'] or STATE['closed']:
        return
    STATE['busy'] = True
    try:
        if time.monotonic() - STATE['started'] > 480:
            raise TimeoutError('Arena screenshot deadline exceeded')
        if STATE['stage'] == 'begin':
            begin()
        elif STATE['stage'] == 'pose':
            prepare_pose()
        elif STATE['stage'] == 'warm':
            LEVEL.editor_invalidate_viewports()
            if time.monotonic() - STATE['at'] > 8:
                capture()
        elif STATE['stage'] == 'capture':
            if time.monotonic() - STATE['at'] > 90:
                raise TimeoutError('Screenshot task did not complete')
            if STATE['task'].is_task_done() and STATE['pending'].exists():
                raw = STATE['pending'].read_bytes()
                assert raw[:8] == b'\x89PNG\r\n\x1a\n' and len(raw) > 1024
                assert struct.unpack('>II', raw[16:24]) == (1600, 900)
                label, seconds = POSES[STATE['index']]
                final = OUT / (label + '.png')
                STATE['pending'].replace(final)
                STATE['views'].append(dict(animation=label, time=seconds, file=str(final),
                                           sha256=hashlib.sha256(raw).hexdigest()))
                STATE['index'] += 1
                if STATE['index'] == len(POSES):
                    finish()
                else:
                    STATE['stage'] = 'pose'
    except Exception:
        finish(traceback.format_exc())
    finally:
        STATE['busy'] = False


u.EditorPythonScripting.set_keep_python_script_alive(True)
STATE['handle'] = u.register_slate_post_tick_callback(tick)
