"""Real PIE detours, crowd query budgets, and camera dissolve/restore checks."""
from pathlib import Path
import json
import time
import traceback
import runpy
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/OldVillage/Review/NavigationCamera.json'
CAMERA_ONLY = globals().get('CAMERA_ONLY', False)
if CAMERA_ONLY:
    OUT = OUT.with_name('CameraOnly.json')
LEVEL = u.get_editor_subsystem(u.LevelEditorSubsystem)
u.EditorPythonScripting.set_keep_python_script_alive(True)
S = {'stage': 'load', 'start': time.monotonic(), 'index': 0}
R = {'passed': False, 'detours': []}
# Unreal XY, centimetres. These cross the pond, training fence, and cottage.
CASES = [('Pond', (3500,-5500), (3500,-3600)),
         ('Training fence', (-4800,-2200), (-4800,0)),
         ('Cottage', (-1700,3700), (-1700,5900))]
SCALE=json.loads((ROOT/'Tools/old_village_layout.json').read_text()).get('layout_scale',1)
CASES=[(name,tuple(x*SCALE for x in a),tuple(x*SCALE for x in z)) for name,a,z in CASES]


def save():
    OUT.write_text(json.dumps(R, indent=2))


def end():
    save()
    LEVEL.editor_request_end_play()
    u.unregister_slate_post_tick_callback(S['handle'])
    if globals().get('RUN_SPAWNING', False):
        def continue_checks(dt):
            if LEVEL.is_in_play_in_editor():
                return
            u.unregister_slate_post_tick_callback(S['next_handle'])
            runpy.run_path(str(ROOT / 'Tools/test_old_village_spawning.py'))
        S['next_handle'] = u.register_slate_post_tick_callback(continue_checks)
    else:
        u.SystemLibrary.quit_editor()


def spawn_enemy(world, position):
    cls = u.load_class(None, '/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyGrunt.BP_EnemyGrunt_C')
    transform = u.Transform(location=u.Vector(position.x, position.y, 90))
    # Blueprint's spawn internals intentionally have no direct Python glue.
    library = u.get_default_object(u.GameplayStatics)
    actor = library.call_method('BeginDeferredActorSpawnFromClass', args=(world, cls, transform, u.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN, None, u.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    library.call_method('FinishSpawningActor', args=(actor, transform, u.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    actor.configure_for_stress_test(True, True)
    return actor


def project(world, point):
    location = u.NavigationSystemV1.project_point_to_navigation(world, u.Vector(*point, 0), None, None, query_extent=u.Vector(250,250,250))
    assert location is not None, 'No baked navigation at ' + str(point)
    return location


def begin_camera(world, pawn, now):
    S['rig'] = u.GameplayStatics.get_all_actors_of_class(world,u.PlayerCameraRig)[0]
    S['fade'] = S['rig'].get_component_by_class(u.CameraOcclusionComponent)
    pawn.set_actor_location(u.Vector(5500*SCALE-700,600*SCALE,110),False,True)
    S.update(stage='fade',sim=now)


def tick(dt):
    if S.get('busy'):
        return
    S['busy'] = True
    try:
        assert time.monotonic() - S['start'] < 1200, 'Runtime validation timeout'
        if S['stage'] == 'load':
            assert LEVEL.load_level('/Game/Maps/Lvl_OldVillage')
            LEVEL.editor_request_begin_play()
            S.update(stage='init', at=time.monotonic())
            return
        world = u.EditorLevelLibrary.get_game_world()
        if not world or time.monotonic() - S['at'] < 3:
            return
        now = u.GameplayStatics.get_time_seconds(world)
        pawn = u.GameplayStatics.get_player_character(world, 0)
        if S['stage'] == 'init':
            for a in u.GameplayStatics.get_all_actors_of_class(world, u.EnemySpawner):
                a.set_spawning_enabled(False)
            for a in u.GameplayStatics.get_all_actors_of_class(world, u.EnemyBase):
                a.destroy_actor()
            for a in u.GameplayStatics.get_all_actors_of_class(world, u.CharacterBase):
                attack = a.get_component_by_class(u.AutoAttackComponent)
                if attack:
                    attack.set_auto_attack_enabled(False)
            S['budget'] = next(s for s in u.ObjectIterator(u.EnemyNavigationSubsystem) if s.get_outer() == world)
            if CAMERA_ONLY:
                begin_camera(world, pawn, now)
                return
            S['stage'] = 'next'
        if S['stage'] == 'next':
            if S['index'] == len(CASES):
                S.update(stage='crowd', sim=now, crowd=[])
                start = project(world, (-4800*SCALE,-2200*SCALE))
                target = project(world, (-4800*SCALE,0))
                pawn.set_actor_location(u.Vector(target.x,target.y,100),False,True)
                for i in range(128):
                    a = spawn_enemy(world, u.Vector(start.x + (i % 8)*8, start.y + (i//8)*8, 90))
                    a.set_actor_hidden_in_game(True)
                    S['crowd'].append(a)
                return
            name, start_xy, target_xy = CASES[S['index']]
            start, target = project(world,start_xy), project(world,target_xy)
            path = u.NavigationSystemV1.find_path_to_location_synchronously(world,start,target)
            assert path and path.is_valid() and not path.is_partial(), name + ' has no complete route'
            points = list(path.get_editor_property('path_points'))
            assert len(points) > 2, name + ' did not exercise an obstacle detour'
            pawn.set_actor_location(u.Vector(target.x,target.y,100),False,True)
            enemy = spawn_enemy(world,start)
            S.update(stage='detour', enemy=enemy, target=target, sim=now, travelled=0., last=enemy.get_actor_location())
            R['detours'].append({'name':name,'path_points':len(points),'start':[start.x,start.y],'target':[target.x,target.y]})
            return
        if S['stage'] == 'detour':
            enemy = S['enemy']
            p = enemy.get_actor_location()
            S['travelled'] += p.distance(S['last'])
            S['last'] = p
            distance = u.Vector(p.x,p.y,0).distance(u.Vector(S['target'].x,S['target'].y,0))
            if distance < 220:
                R['detours'][-1].update(passed=True, seconds=now-S['sim'], travelled_cm=S['travelled'])
                enemy.destroy_actor()
                S.update(stage='next', index=S['index']+1)
                save()
            else:
                velocity = enemy.get_velocity()
                R['detours'][-1].update(distance_remaining=distance, seconds=now-S['sim'],
                    enemy_position=[p.x,p.y,p.z], velocity=[velocity.x,velocity.y],
                    path_requests=S['budget'].get_editor_property('path_requests'))
                assert now-S['sim'] < 60, 'Enemy stuck: ' + R['detours'][-1]['name']
            return
        if S['stage'] == 'crowd' and now-S['sim'] >= 12:
            budget = S['budget']
            R['navigation'] = {key: budget.get_editor_property(key) for key in ['path_requests','deferred_requests','peak_requests_per_frame','total_path_milliseconds','peak_path_milliseconds']}
            R['navigation']['stress_enemies'] = len(S['crowd'])
            assert 0 < R['navigation']['peak_requests_per_frame'] <= 2
            assert R['navigation']['path_requests'] > 0
            for a in S['crowd']:
                a.destroy_actor()
            begin_camera(world, pawn, now)
            return
        if S['stage'] == 'fade' and now-S['sim'] >= 2:
            fade = S['fade']
            count = fade.get_editor_property('fading_object_count')
            R['camera'] = {'occluding_objects':count, 'checks':fade.get_editor_property('occlusion_checks')}
            assert count > 0, 'Shrine did not trigger camera occlusion'
            # Runtime custom primitive data is not reflected to Python in 5.8.
            # Compare rendered on/off captures to verify the material result.
            filename = str(OUT.parent / 'Camera_Dissolve_On.png').replace('\\','/')
            u.SystemLibrary.execute_console_command(world, 'HighResShot 1280x720 filename="' + filename + '"')
            S.update(stage='capture_on', sim=now)
            return
        if S['stage'] == 'capture_on' and now-S['sim'] >= 1:
            S['fade'].set_editor_property('enabled',False)
            S.update(stage='capture_off', sim=now)
            return
        if S['stage'] == 'capture_off' and now-S['sim'] >= 1:
            assert S['fade'].get_editor_property('fading_object_count') == 0, 'Disabling fade did not restore scenery'
            filename = str(OUT.parent / 'Camera_Dissolve_Off.png').replace('\\','/')
            u.SystemLibrary.execute_console_command(world, 'HighResShot 1280x720 filename="' + filename + '"')
            S.update(stage='move_clear',sim=now)
            return
        if S['stage'] == 'move_clear' and now-S['sim'] >= 1:
            S['fade'].set_editor_property('enabled',True)
            # Move to the open central spawn and verify restoration.
            pawn.set_actor_location(u.Vector(-1550,-200,110),False,True)
            S.update(stage='restore',sim=now)
            return
        if S['stage'] == 'restore' and now-S['sim'] >= 2:
            R['camera']['after_clear'] = S['fade'].get_editor_property('fading_object_count')
            assert R['camera']['after_clear'] == 0, 'Scenery did not restore'
            R['passed'] = True
            u.log('OLD_VILLAGE_NAVIGATION_CAMERA_TEST_PASSED')
            end()
    except Exception:
        R['error'] = traceback.format_exc()
        u.log_error(R['error'])
        end()
    finally:
        S['busy'] = False


S['handle'] = u.register_slate_post_tick_callback(tick)
