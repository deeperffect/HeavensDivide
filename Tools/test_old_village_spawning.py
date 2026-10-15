"""PIE regression: waves, HUD clock, scheduled shrines/urns and field events.

Schedule overrides affect unsaved editor instances only, never saved maps.
"""
from pathlib import Path
import json
import time
import traceback
import runpy
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
output = root / 'Saved/OldVillage/Review/Spawning.json'
output.parent.mkdir(parents=True, exist_ok=True)
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
u.EditorPythonScripting.set_keep_python_script_alive(True)
maps = ['Lvl_OldVillage', 'Lvl_OldVillage_Blockout']
state = {'index': 0, 'stage': 'load', 'start': time.monotonic()}
check_rendered_hud = '-nullrhi' not in u.SystemLibrary.get_command_line().lower()
report = {'passed': False, 'rendered_hud_checked': check_rendered_hud, 'maps': {}}


def finish():
    output.write_text(json.dumps(report, indent=2))
    level.editor_request_end_play()
    u.unregister_slate_post_tick_callback(state['handle'])
    u.SystemLibrary.quit_editor()


def tick(dt):
    if state.get('busy'):
        return
    state['busy'] = True
    try:
        assert time.monotonic() - state['start'] < 300, 'PIE run verification timed out'
        if state['stage'] == 'ended':
            if level.is_in_play_in_editor():
                return
            state.update(index=state['index'] + 1, stage='load')
        if state['index'] == len(maps):
            report['passed'] = True
            u.log('OLD_VILLAGE_SPAWNING_TEST_PASSED')
            finish()
            return
        name = maps[state['index']]
        if state['stage'] == 'load':
            assert level.load_level('/Game/Maps/' + name)
            actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
            for cls, count in [(u.RunObjectiveDirector,1),(u.EncounterDirector,1),(u.BossToriiGate,1),(u.ObjectiveSpawnPoint,8)]:
                assert sum(isinstance(a,cls) for a in actors)==count, str(cls)
            director=next(a for a in actors if isinstance(a,u.RunObjectiveDirector))
            director.set_editor_property('blood_shrine_spawn_time',2.)
            director.set_editor_property('second_blood_shrine_spawn_time',4.)
            encounter=next(a for a in actors if isinstance(a,u.EncounterDirector))
            encounter.set_editor_property('first_urn_seconds',2.)
            encounter.set_editor_property('first_magnet_seconds',4.)
            report['maps'][name] = []
            level.editor_request_begin_play()
            state.update(stage='play', at=time.monotonic(), observed_urns=set())
            return
        world = u.EditorLevelLibrary.get_game_world()
        if not world:
            return
        # Record scheduled actors as they appear, before combat can destroy an
        # urn. Disable player auto-attacks to keep this placement test focused.
        for character in u.GameplayStatics.get_all_actors_of_class(world,u.CharacterBase):
            attack=character.get_component_by_class(u.AutoAttackComponent)
            if attack:attack.set_auto_attack_enabled(False)
            health=character.get_component_by_class(u.HealthComponent)
            if health:health.set_damage_enabled(False)
        state['observed_urns'].update(a.get_class().get_name() for a in u.GameplayStatics.get_all_actors_of_class(world,u.HealingUrn))
        if time.monotonic() - state['at'] < 3:
            return
        spawners = u.GameplayStatics.get_all_actors_of_class(world, u.EnemySpawner)
        assert len(spawners) == 1, 'Expected exactly one run spawner'
        spawner = spawners[0]
        elapsed = spawner.get_run_time_seconds()
        samples = report['maps'][name]
        threshold = 8 if samples else 1
        if elapsed < threshold:
            return
        controller = u.GameplayStatics.get_player_controller(world, 0)
        hud = controller.get_editor_property('player_hud_widget')
        assert hud, 'Player HUD missing'
        timer = hud.get_editor_property('run_timer_text')
        enemies = u.GameplayStatics.get_all_actors_of_class(world, u.EnemyBase)
        sample = {'spawner_seconds': elapsed, 'controller_seconds': controller.get_run_time_seconds(),
                  'hud_seconds': hud.get_run_time_seconds(), 'hud_text': str(timer.get_text()),
                  'alive_enemies': sum(not a.get_editor_property('is_dead') for a in enemies), 'enemy_actors': len(enemies),
                  'player_present': bool(u.GameplayStatics.get_player_character(world, 0))}
        samples.append(sample)
        if not enemies:
            sample['placement_settings'] = {p: str(spawner.get_editor_property(p)) for p in ['ground_trace_height', 'ground_trace_depth', 'require_nav_mesh_projection']}
            floor = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor) if a.get_actor_label().startswith('Village ground'))
            sample['floor_response'] = str(floor.static_mesh_component.get_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY_GROUND))
            sample['ground_probes'] = []
            for x, y in [(-1550, -200), (1000, -200), (-1550, 2000)]:
                hit = u.SystemLibrary.line_trace_single(world, u.Vector(x,y,1000), u.Vector(x,y,-2000), u.TraceTypeQuery.ECC_ENEMY_GROUND, False, [], u.DrawDebugTrace.NONE, False)
                sample['ground_probes'].append(str(u.GameplayStatics.break_hit_result(hit)) if hit else 'None')
        output.write_text(json.dumps(report, indent=2))
        assert sample['player_present']
        assert sample['controller_seconds'] == elapsed and sample['hud_seconds'] == elapsed
        assert sample['alive_enemies'] > 0 and enemies, 'Natural wave spawning failed'
        assert all(a.get_actor_location().z > -50 for a in enemies), 'Enemy fell below island'
        if len(samples) == 2:
            assert samples[1]['spawner_seconds'] > samples[0]['spawner_seconds']
            # NullRHI does not tick rendered UMG widgets. Their clock data is
            # still checked above; visible text requires the -Render run.
            if check_rendered_hud:
                assert samples[1]['hud_text'] != samples[0]['hud_text'], 'Visible timer did not update'
            shrines=u.GameplayStatics.get_all_actors_of_class(world,u.BloodShrine)
            urns=u.GameplayStatics.get_all_actors_of_class(world,u.HealingUrn)
            sample['scheduled_shrines']=len(shrines)
            sample['scheduled_urn_classes']=sorted(state['observed_urns'])
            sample['surviving_urns']=len(urns)
            assert len(shrines)==2, 'Scheduled shrines missing'
            assert len(set(sample['scheduled_urn_classes']))==2, 'Healing or magnet urn missing'
            if name == 'Lvl_OldVillage':
                probe = runpy.run_path(str(root / 'Tools/validate_old_village.py'), init_globals={'PLAY_WORLD': world})
                report['collision'] = probe['report']
                assert report['collision']['passed'], 'Ground, routes or boundaries failed'
            # A full wall ring deliberately rejects cluttered locations. Test
            # its placement in the open shrine court, after scenery probes.
            scale=json.loads((root/'Tools/old_village_layout.json').read_text()).get('layout_scale',1)
            u.GameplayStatics.get_player_character(world,0).set_actor_location(u.Vector(3000*scale,1000*scale,110),False,True)
            encounter=u.GameplayStatics.get_all_actors_of_class(world,u.EncounterDirector)[0]
            sample['march_spawned']=encounter.spawn_march()
            sample['ember_circle_spawned']=encounter.spawn_ember_circle()
            sample['wall_trap_spawned']=encounter.spawn_wall_trap()
            assert sample['march_spawned'] and sample['ember_circle_spawned'] and sample['wall_trap_spawned'], 'Encounter placement failed'
            level.editor_request_end_play()
            state['stage'] = 'ended'
    except Exception:
        report['error'] = traceback.format_exc()
        u.log_error(report['error'])
        finish()
    finally:
        state['busy'] = False


state['handle'] = u.register_slate_post_tick_callback(tick)
