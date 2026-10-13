from pathlib import Path
import json
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
tools=u.AssetToolsHelpers.get_asset_tools()
base='/Game/HeavensDivide/Audio'
def create(name,cls,factory):
    asset=u.load_asset(base+'/Mix/'+name)
    return asset or tools.create_asset(name,base+'/Mix',cls,factory())
sfx=create('SC_SoundEffects',u.SoundClass,u.SoundClassFactory)
music=create('SC_Music',u.SoundClass,u.SoundClassFactory)
mix=create('SM_UserVolumes',u.SoundMix,u.SoundMixFactory)
assert sfx and music and mix
mix.set_editor_property('duration',-1.0)
for asset in (sfx,music,mix):u.EditorAssetLibrary.save_loaded_asset(asset,False)
task=u.AssetImportTask()
task.filename=str(root/'SourceAudio/Ambience/Ambience_Lvl1.wav')
task.destination_path=base+'/Ambience'
task.destination_name='W_Ambience_Lvl1'
task.automated=True;task.replace_existing=True;task.save=True
tools.import_asset_tasks([task])
wave=u.load_asset(base+'/Ambience/W_Ambience_Lvl1')
assert wave
wave.set_editor_property('looping',True)
wave.set_editor_property('virtualization_mode',u.VirtualizationMode.PLAY_WHEN_SILENT)
wave.set_editor_property('sound_class_object',sfx)
u.EditorAssetLibrary.save_loaded_asset(wave,False)
assigned=[]
for data in u.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game',True):
    if str(data.asset_class_path.asset_name) not in ('SoundWave','SoundCue','MetaSoundSource'):continue
    asset=data.get_asset()
    if not asset:continue
    old=asset.get_editor_property('sound_class_object')
    if old==music:continue
    if old!=sfx:
        asset.set_editor_property('sound_class_object',sfx)
        u.EditorAssetLibrary.save_loaded_asset(asset,False)
        assigned.append(asset.get_path_name())
(root/'Saved/MenuCombatAudio/AudioImport.json').write_text(json.dumps(dict(ambience=wave.get_path_name(),duration=wave.get_editor_property('duration'),looping=wave.get_editor_property('looping'),effects=assigned),indent=2),encoding='utf-8')
u.log('LEVEL_AUDIO_READY')
