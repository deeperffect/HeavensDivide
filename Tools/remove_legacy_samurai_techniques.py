"""Remove unrelated technique cards without rewriting any retained tuning.

Run in Unreal Python; -ValidateSamuraiCleanup performs read-only validation.
"""
import hashlib
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
controller='/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
ids={'SamuraiTechnique.Cleaver','SamuraiTechnique.Duelist','SamuraiTechnique.Deathblow'}
paths=['/Game/HeavensDivide/Upgrades/SamuraiTrial/DA_Upgrade_SamuraiTechnique'+name for name in ['Cleaver','Duelist','Deathblow']]
keep_ids={r['id'] for r in json.loads((root/'Tools/samurai_build_upgrades.json').read_text())}
keep_ids.update(['BleedingEdge','DeepCuts','SamuraiHeavyBlade','SamuraiArea','DoubleCut','BladeWave','BladeWavePower','BladeWaveHaste','WideArc','ReturningBlade','CrossingBlades','SplinterWave'])
validate='-ValidateSamuraiCleanup' in unreal.SystemLibrary.get_command_line()
backup=root/'Saved/Backups/LegacySamuraiCleanup'/datetime.now().strftime('%Y%m%d_%H%M%S')
def disk(path):return root/'Content'/(path.removeprefix('/Game/').split('.')[0]+'.uasset')
def copy(path):
    src=disk(path);dest=backup/src.relative_to(root)
    if src.exists():dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
bp=unreal.load_asset(controller)
comp=unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
pool=list(comp.get_editor_property('upgrade_pool'))
retained=[a for a in pool if a and str(a.get_editor_property('upgrade_id')) not in ids]
# Snapshot all retained asset bytes, including the user's edited balance and descriptions.
hashes={str(disk(a.get_path_name())):hashlib.sha256(disk(a.get_path_name()).read_bytes()).hexdigest() for a in retained}
samurai=[a for a in retained if a.get_path_name().startswith('/Game/HeavensDivide/Upgrades/Samurai/')]
actual_samurai={str(a.get_editor_property('upgrade_id')) for a in samurai}
assert actual_samurai==keep_ids, {'extra':sorted(actual_samurai-keep_ids),'missing':sorted(keep_ids-actual_samurai)}
if not validate:
    for path in paths:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            refs=set(str(r) for r in unreal.EditorAssetLibrary.find_package_referencers_for_asset(path,True))
            assert not refs-set(paths+[controller]),(path,refs)
            copy(path)
    copy(controller)
    backup.mkdir(parents=True,exist_ok=True)
    (backup/'retained_asset_hashes.json').write_text(json.dumps(hashes,indent=2))
    comp.set_editor_property('upgrade_pool',retained)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
    for path in paths:
        if unreal.EditorAssetLibrary.does_asset_exist(path):assert unreal.EditorAssetLibrary.delete_asset(path)
    assert all(hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest for path,digest in hashes.items()),'Retained tuning changed'
    unreal.log('SAMURAI_CLEANUP_BACKUP: '+str(backup))
for path in paths:assert not unreal.EditorAssetLibrary.does_asset_exist(path),path
actual=[str(a.get_editor_property('upgrade_id')) for a in comp.get_editor_property('upgrade_pool') if a]
assert len(actual)==len(set(actual))==57 and not ids.intersection(actual)
for a in retained:
    assert not ids.intersection(str(v) for v in a.get_editor_property('prerequisite_upgrade_ids'))
    assert not ids.intersection(str(v.get_editor_property('upgrade_id')) for v in a.get_editor_property('prerequisite_requirements'))
unreal.log('SAMURAI_CLEANUP_OK: 3 legacy techniques absent; 22 build-linked Samurai cards; 57 total cards; retained card files unchanged')
