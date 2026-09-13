"""Remove the eleven retired family Prepare cards/art; universal Prepare is baseline.

Run with Unreal's Python commandlet; append -ValidateFamilyRemoval for read-only checks.
Backups are kept under Saved/Backups/UniversalPrepare before any asset is changed.
"""
import shutil
from datetime import datetime
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
ids = {'StormConductor', 'FallenConstellation', 'ThreadSever', 'PlagueHarvest',
       'VenomEdge', 'OrbitRelay', 'RallyingShadows', 'EclipseHarvest',
       'PhantomHandoff', 'CrimsonVerdict', 'CarrionFeast'}
cards = ['/Game/HeavensDivide/Upgrades/Synergy/DA_BuildSynergy_' + uid for uid in sorted(ids)]
art = ['/Game/HeavensDivide/Blueprints/UI/BuildFamilyArt/T_' + uid for uid in sorted(ids)]
controller = '/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
validate = '-ValidateFamilyRemoval' in unreal.SystemLibrary.get_command_line()
backup = root / 'Saved/Backups/UniversalPrepare' / datetime.now().strftime('%Y%m%d_%H%M%S')

def backup_file(path):
    path = path.resolve()
    target = backup / path.relative_to(root)
    if path.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)

if not validate:
    for path in cards + art:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            refs = unreal.EditorAssetLibrary.find_package_referencers_for_asset(path, True)
            unexpected = set(str(r) for r in refs) - set(cards + art + [controller, path])
            if unexpected:
                raise RuntimeError('Unexpected references to ' + path + ': ' + str(unexpected))
            backup_file(root / 'Content' / (path.removeprefix('/Game/') + '.uasset'))
    backup_file(root / 'Content' / (controller.removeprefix('/Game/') + '.uasset'))
    bp = unreal.load_asset(controller)
    component = unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
    pool = component.get_editor_property('upgrade_pool')
    retained = [a for a in pool if a and str(a.get_editor_property('upgrade_id')) not in ids]
    assert len(pool) - len(retained) in (0, 11), 'Unexpected retired card count'
    component.set_editor_property('upgrade_pool', retained)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    for path in cards + art:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            assert unreal.EditorAssetLibrary.delete_asset(path), path
    for uid in ids:
        path = (root / 'Art/UpgradeCards/Generated' / (uid + '.png')).resolve()
        assert path.is_relative_to(root / 'Art/UpgradeCards/Generated')
        if path.exists():
            backup_file(path)
            path.unlink()
    unreal.log('UNIVERSAL_PREPARE_BACKUP: ' + str(backup))

for path in cards + art:
    assert not unreal.EditorAssetLibrary.does_asset_exist(path), path
bp = unreal.load_asset(controller)
cdo = unreal.get_default_object(bp.generated_class())
pool = cdo.get_editor_property('player_upgrade_component').get_editor_property('upgrade_pool')
assert len(pool) >= 111, len(pool)
assert len({str(a.get_editor_property('upgrade_id')) for a in pool}) == len(pool)
for a in pool:
    assert a and str(a.get_editor_property('upgrade_id')) not in ids
    assert not ids.intersection(str(v) for v in a.get_editor_property('prerequisite_upgrade_ids'))
    for req in a.get_editor_property('prerequisite_requirements'):
        assert str(req.get_editor_property('upgrade_id')) not in ids
ability = cdo.get_component_by_class(unreal.SurvivorAbilityComponent)
assert ability
assert abs(ability.get_editor_property('preparation_duration') - 6) < .001
assert abs(ability.get_editor_property('preparation_damage_multiplier') - .6) < .001
assert abs(ability.get_editor_property('preparation_spread_radius') - 300) < .001
unreal.log('UNIVERSAL_PREPARE_ASSETS_OK: 11 retired cards and textures absent; '+str(len(pool))+' unique pool entries; shared Prepare settings verified')
