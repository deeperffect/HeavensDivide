"""Run with Unreal's Python commandlet after building the notify state class."""
import os
import shutil
import unreal


PATH = '/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_AutoAttackSamurai'
lib = unreal.AnimationLibrary
montage = unreal.load_asset(PATH)
assert montage, 'Auto-attack montage missing'
tracks = lib.get_animation_notify_track_names(montage)
events = [(track, event) for track in tracks
          for event in lib.get_animation_notify_events_for_track(montage, track)]
old = [(track, event) for track, event in events
       if isinstance(event.get_editor_property('notify'), unreal.AnimNotify_SpawnSamuraiSlashNiagara)]
trails = [(track, event) for track, event in old
          if event.get_editor_property('notify').get_editor_property('niagara_system').get_name() == 'NS_SlashTrail_Lightning']
if not trails:
    assert any(isinstance(event.get_editor_property('notify_state_class'), unreal.AnimNotifyState_SamuraiSlashNiagara)
               for _, event in events), 'Expected trail notify not found'
    unreal.log('Samurai trail already converted.')
else:
    assert len(trails) == 1, 'Expected exactly one lightning trail'
    assert all(str(event.get_editor_property('notify_name')) == 'SpawnSamuraiSlashNiagara' for _, event in old)
    assert sum(str(event.get_editor_property('notify_name')) == 'SpawnSamuraiSlashNiagara' for _, event in events) == len(old)
    source = os.path.join(unreal.Paths.project_content_dir(), PATH.removeprefix('/Game/') + '.uasset')
    backup = os.path.join(unreal.Paths.project_saved_dir(), 'Backups', 'SamuraiTrailNotify', 'AM_AutoAttackSamurai.uasset')
    os.makedirs(os.path.dirname(backup), exist_ok=True)
    if not os.path.exists(backup):
        shutil.copy2(source, backup)
    # Keep the hit burst as a single-fire event; only the trail needs a duration.
    assert lib.remove_animation_notify_events_by_name(montage, 'SpawnSamuraiSlashNiagara') == len(old)
    for track, event in old:
        notify = event.get_editor_property('notify')
        start = lib.get_anim_notify_event_trigger_time(event)
        if notify.get_editor_property('niagara_system').get_name() == 'NS_SlashTrail_Lightning':
            duration = min(0.2, montage.get_play_length() - start)
            assert duration > 0
            state = lib.add_animation_notify_state_event(montage, track, start, duration, unreal.AnimNotifyState_SamuraiSlashNiagara)
            for field in ['niagara_system', 'weapon_component_name', 'skeleton_socket_name', 'location_offset', 'rotation_offset', 'scale', 'area_bonus_scale_multiplier', 'area_scaled_offset_parameter', 'area_scaled_float_parameters']:
                state.set_editor_property(field, notify.get_editor_property(field))
            unreal.log(f'Samurai trail state: start={start:.4f}s duration={duration:.4f}s')
        else:
            lib.add_animation_notify_event_from_source(montage, start, event, track)
    result = lib.get_animation_notify_events(montage)
    assert len(result) == len(events)
    assert sum(isinstance(e.get_editor_property('notify_state_class'), unreal.AnimNotifyState_SamuraiSlashNiagara) for e in result) == 1
    assert sum(isinstance(e.get_editor_property('notify'), unreal.AnimNotify_SpawnSamuraiSlashNiagara) for e in result) == len(old) - 1
    assert unreal.EditorAssetLibrary.save_loaded_asset(montage, only_if_is_dirty=False)
    unreal.log('Saved Samurai auto-attack trail notify state.')
