"""Save run audio, swap, Blood hitbox calibration and partial scenery dissolve.

Run BloodSlashHitbox automation first to measure the authored Niagara footprint.
"""
from pathlib import Path
from datetime import datetime
import json
import shutil
import unreal as u

root=Path(u.Paths.project_dir()).resolve()
out=root/'Saved/RunFixes'
backup=out/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S')
report={'maps':{},'swap':{},'materials':[]}
def preserve(asset):
    path=asset.get_path_name().split('.')[0].removeprefix('/Game/')+'.uasset'
    source=root/'Content'/path
    if source.exists():
        dest=backup/path;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dest)
def save(asset):
    assert u.EditorAssetLibrary.save_loaded_asset(asset,False),asset.get_path_name()

wave=u.load_asset('/Game/HeavensDivide/Audio/Music/W_MitkoIgra');preserve(wave)
wave.set_editor_property('looping',True)
wave.set_editor_property('virtualization_mode',u.VirtualizationMode.PLAY_WHEN_SILENT)
wave.set_editor_property('sound_class_object',u.load_asset('/Game/HeavensDivide/Audio/Mix/SC_Music'))
save(wave)
report['music']={'looping':True,'duration':wave.get_editor_property('duration')}
for name in ['Samurai','Ninja']:
    bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_'+name);preserve(bp)
    swap=u.get_default_object(bp.generated_class()).get_component_by_class(u.SwapPresentationComponent)
    freeze=swap.get_editor_property('swap_freeze_duration')
    swap.set_editor_property('entrance_play_rate',2.5)
    swap.set_editor_property('departure_play_rate',2.5)
    swap.set_editor_property('arrival_drop_duration',.25)
    swap.set_editor_property('arrival_walk_duration',.3)
    u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    assert abs(swap.get_editor_property('swap_freeze_duration')-freeze)<.0001
    report['swap'][name]={'freeze_unchanged':freeze,'entrance_rate':2.5,'departure_rate':2.5}
bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_PlayerCameraRig');preserve(bp)
cdo=u.get_default_object(bp.generated_class())
cdo.set_editor_property('swap_zoom_amount',0.)
u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)

calibration=json.loads((out/'BloodCalibration.json').read_text())
radius=calibration['radius'];assert 1<radius<5000
for name in ['AM_AutoAttackSamurai','AM_DoubleCutBloodStance']:
    montage=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/'+name);preserve(montage)
    count=0
    for event in u.AnimationLibrary.get_animation_notify_events(montage):
        notify=event.get_editor_property('notify')
        if isinstance(notify,u.AnimNotify_SpawnSamuraiSlashNiagara):
            notify.set_editor_property('match_melee_hitbox',True)
            notify.set_editor_property('hitbox_reference_radius',radius)
            notify.set_editor_property('area_bonus_scale_multiplier',1.)
            count+=1
    assert count==1
    save(montage)
report['blood_slash']=calibration

for path in u.EditorAssetLibrary.list_assets('/Game/HeavensDivide/Environment/OldVillage/CameraFade',True,False):
    material=u.load_asset(path)
    if isinstance(material,u.Material):
        preserve(material)
        assert u.CameraOcclusionMaterialLibrary.prepare_material(material),path
        save(material);report['materials'].append(path)

# Read saved maps without rebuilding layouts or changing any scenery.
level=u.get_editor_subsystem(u.LevelEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for name in ['Lvl_OldVillage','Lvl_MoonlitForest','Lvl_SunkenSanctuary','Lvl_EmberQuarry']:
    assert level.load_level('/Game/Maps/'+name)
    music=[a for a in actors.get_all_level_actors() if a.actor_has_tag('OldVillageMusic')]
    assert len(music)==1,name
    audio=music[0].get_component_by_class(u.AudioComponent)
    assert audio.get_editor_property('sound')==wave
    report['maps'][name]={'music':audio.get_editor_property('sound').get_path_name(),
                        'auto_activate':audio.get_editor_property('auto_activate')}
(out/'Applied.json').write_text(json.dumps(report,indent=2))
u.log('RUN_FIXES_ASSETS_SAVED')
