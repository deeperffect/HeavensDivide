"""Retire automatic ability cards and exclusive artwork, preserving Blade Wave.

Run in Unreal Python. -ValidateAttackRoster verifies saved assets read-only.
"""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
rows = json.loads((root / 'Tools/build_family_catalog.json').read_text())
retired = [r for r in rows if not r.get('available', True)]
ids = set()
paths = []
for r in retired:
    family_ids = [r['id']] + [b['id'] for b in r['branches']] + [r['id'] + s for s in r['scales']]
    ids.update(family_ids)
    paths.extend('/Game/HeavensDivide/Upgrades/' + r['owner'] + '/DA_Upgrade_' + r['owner'] + uid for uid in family_ids)
art = ['/Game/HeavensDivide/Blueprints/UI/BuildFamilyArt/T_' + uid for uid in sorted(ids)]
controller = '/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
validate = '-ValidateAttackRoster' in unreal.SystemLibrary.get_command_line()
backup = root / 'Saved/Backups/AutomaticAbilityRemoval' / datetime.now().strftime('%Y%m%d_%H%M%S')

def backup_file(path):
    path = path.resolve()
    destination = backup / path.relative_to(root)
    if path.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)

bp = unreal.load_asset(controller)
component = unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
if not validate:
    allowed = set(paths + art + [controller])
    for path in paths + art:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unexpected = set(str(p) for p in unreal.EditorAssetLibrary.find_package_referencers_for_asset(path, True)) - allowed
            if unexpected:
                raise RuntimeError('Unexpected references to ' + path + ': ' + str(unexpected))
            backup_file(root / 'Content' / (path.removeprefix('/Game/') + '.uasset'))
    backup_file(root / 'Content' / (controller.removeprefix('/Game/') + '.uasset'))
    pool = list(component.get_editor_property('upgrade_pool'))
    keep = [a for a in pool if a and str(a.get_editor_property('upgrade_id')) not in ids]
    assert len(pool) - len(keep) in (0, 70)
    assert len(keep) >= 42
    component.set_editor_property('upgrade_pool', keep)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    for path in paths + art:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            assert unreal.EditorAssetLibrary.delete_asset(path), path
    for uid in ids:
        source = (root / 'Art/UpgradeCards/Generated' / (uid + '.png')).resolve()
        assert source.is_relative_to(root / 'Art/UpgradeCards/Generated')
        if source.exists():
            backup_file(source)
            source.unlink()
    unreal.log('ATTACK_ROSTER_BACKUP: ' + str(backup))

for path in paths + art:
    assert not unreal.EditorAssetLibrary.does_asset_exist(path), path
pool = list(component.get_editor_property('upgrade_pool'))
actual = [str(a.get_editor_property('upgrade_id')) for a in pool if a]
assert len(actual) == len(pool) == len(set(actual)) >= 42
assert not ids.intersection(actual)
assert {'BladeWave', 'BladeWavePower', 'WideArc', 'BladeWaveHaste', 'ReturningBlade', 'CrossingBlades', 'SplinterWave', 'TagTeam', 'GrandEntrance'}.issubset(actual)
for a in pool:
    assert not ids.intersection(str(v) for v in a.get_editor_property('prerequisite_upgrade_ids'))
    assert not ids.intersection(str(v.get_editor_property('upgrade_id')) for v in a.get_editor_property('prerequisite_requirements'))
    assert a.get_editor_property('card_artwork'), a.get_path_name()
unreal.log('ATTACK_ROSTER_OK: 70 retired cards and textures absent; retained upgrades with valid prerequisites and artwork')
