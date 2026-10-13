"""Keep the ambience loop alive while muted; audit saved game audio routing."""
import json
import shutil
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
out = root / 'Saved/MasterVolumeFix'
out.mkdir(parents=True, exist_ok=True)
path = '/Game/HeavensDivide/Audio/Ambience/W_Ambience_Lvl1'
wave = u.load_asset(path)
assert isinstance(wave, u.SoundWave)
before = str(wave.get_editor_property('virtualization_mode'))
desired = u.VirtualizationMode.PLAY_WHEN_SILENT
if wave.get_editor_property('virtualization_mode') != desired:
    backup = out / 'Backups/W_Ambience_Lvl1.uasset'
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(root / 'Content/HeavensDivide/Audio/Ambience/W_Ambience_Lvl1.uasset', backup)
    wave.set_editor_property('virtualization_mode', desired)
    assert u.EditorAssetLibrary.save_loaded_asset(wave, False)

sfx = u.load_asset('/Game/HeavensDivide/Audio/Mix/SC_SoundEffects')
music = u.load_asset('/Game/HeavensDivide/Audio/Mix/SC_Music')
assert wave.get_editor_property('sound_class_object') == sfx
assert wave.get_editor_property('looping')
routed = []
unrouted = []
for data in u.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game', True):
    if str(data.asset_class_path.asset_name) not in ('SoundWave', 'SoundCue', 'MetaSoundSource'):
        continue
    sound = data.get_asset()
    cls = sound.get_editor_property('sound_class_object')
    if cls not in (sfx, music):
        unrouted.append(dict(sound=sound.get_path_name(), sound_class=str(cls)))
    else:
        routed.append(sound.get_path_name())
report = dict(ambience=wave.get_path_name(), previous_virtualization=before,
              virtualization=str(wave.get_editor_property('virtualization_mode')),
              looping=wave.get_editor_property('looping'), routed_count=len(routed), unrouted=unrouted)
(out / 'AudioAssets.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
assert not unrouted, unrouted
u.log('MASTER_VOLUME_AUDIO_ASSETS_OK')
