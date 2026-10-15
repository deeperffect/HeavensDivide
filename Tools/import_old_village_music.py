"""Import the supplied MP3 and attach it to both village maps without rebuilding."""
from pathlib import Path
from datetime import datetime
import json
import shutil
import sys
import wave as wav
from array import array
import unreal as u
sys.path.insert(0, str(Path(__file__).resolve().parent))
from old_village_music import ensure_level_music, MUSIC_PATH

root = Path(u.Paths.project_dir()).resolve()
source = root / 'SourceAudio/Music/Mitko_Igra.mp3'
assert source.is_file()
task = u.AssetImportTask()
task.filename = str(source)
task.destination_path = '/Game/HeavensDivide/Audio/Music'
task.destination_name = 'W_MitkoIgra'
task.automated = True
task.replace_existing = True
task.save = True
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
wave = u.load_asset(MUSIC_PATH)
assert isinstance(wave, u.SoundWave), 'Music import failed'
# Decode through Unreal's importer/exporter, retaining the original MP3. A
# 10 ms entrance and 30 ms exit ramp remove endpoint clicks at wraparound.
pcm = root / 'SourceAudio/Music/Mitko_Igra_Loop.wav'
export = u.AssetExportTask()
export.object = wave
export.exporter = u.SoundExporterWAV()
export.filename = str(pcm)
export.automated = True
export.prompt = False
export.replace_identical = True
assert u.Exporter.run_asset_export_task(export), 'PCM export failed'
with wav.open(str(pcm), 'rb') as stream:
    params = stream.getparams()
    assert params.sampwidth == 2, 'Expected 16-bit decoded audio'
    samples = array('h', stream.readframes(params.nframes))
for duration, start in [(0.01, True), (0.03, False)]:
    count = min(round(params.framerate*duration), params.nframes)
    for i in range(count):
        frame = i if start else params.nframes-1-i
        gain = i / max(1, count-1)
        for channel in range(params.nchannels):
            index = frame*params.nchannels+channel
            samples[index] = round(samples[index]*gain)
with wav.open(str(pcm), 'wb') as stream:
    stream.setparams(params)
    stream.writeframes(samples.tobytes())
del samples
task.filename = str(pcm)
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
wave = u.load_asset(MUSIC_PATH)
music_class = u.load_asset('/Game/HeavensDivide/Audio/Mix/SC_Music')
assert music_class
wave.set_editor_property('sound_class_object', music_class)
wave.set_editor_property('looping', True)
wave.set_editor_property('virtualization_mode', u.VirtualizationMode.PLAY_WHEN_SILENT)
wave.set_editor_property('loading_behavior', u.SoundWaveLoadingBehavior.PRIME_ON_LOAD)
assert u.EditorAssetLibrary.save_loaded_asset(wave, False)
report = {'sound': wave.get_path_name(), 'duration_seconds': wave.get_editor_property('duration'),
          'channels': wave.get_editor_property('num_channels'), 'maps': []}
assert report['duration_seconds'] > 0
backup = root / 'Saved/OldVillage/Backups' / (datetime.now().strftime('%Y%m%d_%H%M%S') + '_Music')
backup.mkdir(parents=True, exist_ok=True)
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
for name in ['Lvl_OldVillage', 'Lvl_OldVillage_Blockout']:
    shutil.copy2(root / 'Content/Maps' / (name + '.umap'), backup / (name + '.umap'))
    assert level.load_level('/Game/Maps/' + name)
    assert ensure_level_music()
    assert level.save_current_level()
    report['maps'].append(name)
(root / 'Saved/OldVillage/MusicImport.json').write_text(json.dumps(report, indent=2))
u.log('OLD_VILLAGE_MUSIC_IMPORTED ' + json.dumps(report))
