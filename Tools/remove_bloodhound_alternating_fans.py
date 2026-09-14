"""Remove two Ninja cards, preserving every retained card and pool ordering."""
import hashlib,json,shutil
from pathlib import Path
from datetime import datetime
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
controller='/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
retired={'Bloodhound','AlternatingFans'}
backup=root/'Saved/Backups/NinjaBranchRemoval'/datetime.now().strftime('%Y%m%d_%H%M%S')
def disk(path):return root/'Content'/(path.removeprefix('/Game/').split('.')[0]+'.uasset')
bp=unreal.load_asset(controller)
component=unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
pool=list(component.get_editor_property('upgrade_pool'))
kept=[a for a in pool if str(a.get_editor_property('upgrade_id')) not in retired]
paths=['/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_Ninja'+id for id in sorted(retired)]
for a in kept:
    assert not retired.intersection(str(id) for id in a.get_editor_property('prerequisite_upgrade_ids')),a.get_path_name()
for path in paths:
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        refs={str(r) for r in unreal.EditorAssetLibrary.find_package_referencers_for_asset(path,True)}
        assert not refs-{controller},(path,refs)
hashes={str(disk(a.get_path_name())):hashlib.sha256(disk(a.get_path_name()).read_bytes()).hexdigest() for a in kept}
for path in [controller]+paths:
    src=disk(path)
    if src.exists():
        dst=backup/src.relative_to(root);dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
component.set_editor_property('upgrade_pool',kept)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
for path in paths:
    if unreal.EditorAssetLibrary.does_asset_exist(path):assert unreal.EditorAssetLibrary.delete_asset(path)
actual=list(unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component').get_editor_property('upgrade_pool'))
ids=[str(a.get_editor_property('upgrade_id')) for a in actual]
assert len(ids)==len(set(ids))==55 and not retired.intersection(ids)
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items())
(backup/'preserved_hashes.json').write_text(json.dumps(hashes,indent=2))
unreal.log('NINJA_BRANCH_REMOVAL_OK: Bloodhound and Alternating Fans removed; 55 retained cards unchanged.')
