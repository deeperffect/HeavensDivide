"""Audio-enabled PIE smoke check, including seeking across the loop boundary."""
from pathlib import Path
import json
import time
import traceback
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
output = root / 'Saved/OldVillage/MusicValidation.json'
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
u.EditorPythonScripting.set_keep_python_script_alive(True)
maps = ['Lvl_OldVillage', 'Lvl_OldVillage_Blockout']
state = {'index': 0, 'stage': 'load', 'start': time.monotonic()}
report = {'passed': False, 'maps': {}}


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
        assert time.monotonic() - state['start'] < 300, 'Music verification timeout'
        if state['stage'] == 'ended':
            if level.is_in_play_in_editor():
                return
            state.update(index=state['index']+1, stage='load')
        if state['index'] == len(maps):
            report['passed'] = True
            u.log('OLD_VILLAGE_MUSIC_TEST_PASSED')
            finish()
            return
        name = maps[state['index']]
        if state['stage'] == 'load':
            assert level.load_level('/Game/Maps/' + name)
            level.editor_request_begin_play()
            state.update(stage='playing', at=time.monotonic())
            return
        world = u.EditorLevelLibrary.get_game_world()
        if not world or time.monotonic()-state['at'] < 4:
            return
        actors = [a for a in u.GameplayStatics.get_all_actors_of_class(world,u.AmbientSound) if a.actor_has_tag('OldVillageMusic')]
        assert len(actors) == 1, 'Expected exactly one music loop'
        audio = actors[0].get_component_by_class(u.AudioComponent)
        assert audio.is_playing(), 'Music is not playing (run with -Audio)'
        if state['stage'] == 'playing':
            wave = audio.get_editor_property('sound')
            assert wave.get_editor_property('looping')
            assert wave.get_editor_property('sound_class_object').get_name() == 'SC_Music'
            assert wave.get_editor_property('virtualization_mode') == u.VirtualizationMode.PLAY_WHEN_SILENT
            assert not audio.get_editor_property('allow_spatialization')
            duration = wave.get_editor_property('duration')
            report['maps'][name] = {'auto_started': True, 'duration_seconds': duration,
                'music_bus': True, 'non_spatial': True, 'volume_multiplier': audio.get_editor_property('volume_multiplier')}
            # Starting just before the end exercises looping without waiting the
            # full track. Playback remains on the map's saved component.
            audio.stop()
            audio.play(max(0, duration-0.5))
            state.update(stage='loop', at=time.monotonic())
            return
        if state['stage'] == 'loop':
            report['maps'][name]['playing_after_loop_boundary'] = audio.is_playing()
            audio.set_volume_multiplier(0)
            state.update(stage='mute', at=time.monotonic())
            return
        if state['stage'] == 'mute':
            report['maps'][name]['playing_while_silent'] = audio.is_playing()
            audio.set_volume_multiplier(0.5)
            actors[0].destroy_actor()
            assert not audio.is_playing(), 'Music continued after its owner was destroyed'
            report['maps'][name]['stops_with_owner'] = True
            level.editor_request_end_play()
            state['stage'] = 'ended'
    except Exception:
        report['error'] = traceback.format_exc()
        u.log_error(report['error'])
        finish()
    finally:
        state['busy'] = False


state['handle'] = u.register_slate_post_tick_callback(tick)
