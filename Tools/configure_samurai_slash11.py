"""Replace only the Slash_11 notify, preserving the event's timing and metadata."""
from pathlib import Path
import shutil
import unreal as u

path = '/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_AutoAttackSamurai'
montage = u.load_asset(path)
lib = u.AnimationLibrary
events = list(lib.get_animation_notify_events(montage))
matches = [e for e in events if isinstance(e.get_editor_property('notify'), u.AnimNotify_PlayNiagaraEffect)
           and e.get_editor_property('notify').get_editor_property('template').get_name() == 'NS_Slash_11']
if not matches:
    assert any(isinstance(e.get_editor_property('notify'), u.AnimNotify_SpawnSamuraiSlashNiagara)
               and e.get_editor_property('notify').get_editor_property('attach_to_character_mesh') for e in events)
    u.log('SLASH11_ALREADY_CONFIGURED')
else:
    assert len(matches) == 1
    source = Path(u.Paths.project_content_dir(), path.removeprefix('/Game/') + '.uasset')
    backup = Path(u.Paths.project_saved_dir(), 'Backups/SamuraiSlash11/AM_AutoAttackSamurai.uasset')
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(source, backup)
    event = matches[0]
    old_name = event.get_editor_property('notify_name')
    assert sum(e.get_editor_property('notify_name') == old_name for e in events) == 1
    track = next(track for track in lib.get_animation_notify_track_names(montage)
                 if any(e.get_editor_property('notify') == event.get_editor_property('notify')
                        for e in lib.get_animation_notify_events_for_track(montage, track)))
    start = lib.get_anim_notify_event_trigger_time(event)
    old = event.get_editor_property('notify')
    assert old.get_editor_property('attached') and not old.get_editor_property('absolute_scale')
    new = u.new_object(u.AnimNotify_SpawnSamuraiSlashNiagara, outer=montage)
    new.set_editor_property('niagara_system', old.get_editor_property('template'))
    new.set_editor_property('attach_to_character_mesh', True)
    new.set_editor_property('skeleton_socket_name', old.get_editor_property('socket_name'))
    for field in ['location_offset', 'rotation_offset', 'scale']:
        new.set_editor_property(field, old.get_editor_property(field))
    new.set_editor_property('area_bonus_scale_multiplier', 1.0)
    new.set_editor_property('area_scaled_offset_parameter', 'None')
    new.set_editor_property('area_scaled_float_parameters', [])
    event.set_editor_property('notify', new)
    assert lib.remove_animation_notify_events_by_name(montage, old_name) == 1
    lib.add_animation_notify_event_from_source(montage, start, event, track)
    assert len(lib.get_animation_notify_events(montage)) == len(events)
    assert u.EditorAssetLibrary.save_loaded_asset(montage, False)
    u.log('SLASH11_CONFIGURED: preserved timing, offsets, rotation and base scale; area strength=1')
