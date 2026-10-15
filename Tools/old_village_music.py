"""One world-owned, non-spatial music loop using the existing user-volume mix."""
import unreal as u

MUSIC_PATH = '/Game/HeavensDivide/Audio/Music/W_MitkoIgra'
TAG = 'OldVillageMusic'


def ensure_level_music():
    if not u.EditorAssetLibrary.does_asset_exist(MUSIC_PATH):
        return None
    wave = u.load_asset(MUSIC_PATH)
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    matches = [a for a in actors.get_all_level_actors() if a.actor_has_tag(TAG)]
    assert len(matches) <= 1, 'Duplicate village music actors'
    actor = matches[0] if matches else actors.spawn_actor_from_class(u.AmbientSound, u.Vector(0,0,100))
    actor.set_actor_label('Music - Mitko Igra')
    actor.set_folder_path('80_Gameplay/Audio')
    actor.set_editor_property('tags', [TAG])
    audio = actor.get_component_by_class(u.AudioComponent)
    audio.set_sound(wave)
    audio.set_editor_property('auto_activate', True)
    audio.set_editor_property('allow_spatialization', False)
    audio.set_editor_property('override_attenuation', False)
    audio.set_editor_property('attenuation_settings', None)
    audio.set_editor_property('is_ui_sound', True)
    # AmbientSound's native constructor enables stop-when-owner-destroyed.
    audio.set_volume_multiplier(0.5)
    return actor
