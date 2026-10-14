"""Retire solo combo assets, resave current definitions, and configure alternating throws.

Run in Unreal Editor Python after building the native cleanup. Every changed or
deleted asset is backed up; external package references prevent deletion.
"""
import json
import shutil
import hashlib
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
out = root / 'Saved/JointComboCleanup'
backup = out / 'AssetBackups'
backup.mkdir(parents=True, exist_ok=True)


def protect(path):
    relative = Path(path.split('.')[0].removeprefix('/Game/') + '.uasset')
    source = (root / 'Content' / relative).resolve()
    assert source.is_relative_to(root / 'Content'), source
    destination = backup / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists():
        shutil.copy2(source, destination)


report = dict(updated=[], deleted=[], blocked=[], referencers={})
u.AssetRegistryHelpers.get_asset_registry().search_all_assets(True)
folder = '/Game/HeavensDivide/Abilities/Combos/'
nin_bp = u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja')
attack = u.get_default_object(nin_bp.generated_class()).get_component_by_class(u.AutoAttackComponent)
primary = attack.get_editor_property('attack_montage')
alternate = attack.get_editor_property('alternate_attack_montage')
assert primary and alternate and primary != alternate
for identity in ['BloodMoon', 'HeavensDivide', 'Moonwell']:
    path = folder + 'DA_Combo_' + identity
    protect(path)
    asset = u.load_asset(path)
    assert isinstance(asset, u.ComboAbilityDefinition)
    # Preserve every authored system, transform and combat value. New secondary
    # settings reproduce the old literal values on the first migration only.
    stamp = out / (identity + '.migrated')
    if not stamp.exists():
        if identity == 'HeavensDivide':
            asset.set_editor_property('ninja_montage', primary)
            asset.set_editor_property('ninja_alternate_montage', alternate)
            asset.set_editor_property('secondary_vfx_scale', .18)
            asset.set_editor_property('secondary_vfx_seconds', .12)
            asset.set_editor_property('secondary_vfx_color', u.LinearColor(.3, .5, 3, 1))
            asset.set_editor_property('vfx_seconds', .6)
        elif identity == 'Moonwell':
            asset.set_editor_property('vfx_seconds', .45)
    assert u.EditorAssetLibrary.save_loaded_asset(asset, False)
    stamp.write_text('Saved editable presentation defaults; reruns preserve tuning.\n')
    report['updated'].append(path)

# Resaving removes retired native property tags; compile catches any saved graph
# still depending on the removed pulse event or struct.
for name in ['BP_Samurai', 'BP_Ninja', 'BP_PlayerCameraRig']:
    path = '/Game/HeavensDivide/Blueprints/PlayerCharacters/' + name
    protect(path)
    bp = u.load_asset(path)
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert 'ERROR' not in str(bp.get_editor_property('status')).upper(), path
    assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
    report['updated'].append(path)

retired = [folder + 'DA_Combo_' + identity for identity in
           ['IronLotus', 'FallingHeavens', 'SerpentSeal', 'ReturningStorm', 'Starfall', 'VoidStep', 'ThunderNeedles']]
retired += ['/Game/HeavensDivide/VFX/Combos/NS_Combo_' + identity for identity in
            ['BloodMoon', 'HeavensDivide', 'Moonwell', 'IronLotus', 'FallingHeavens', 'SerpentSeal', 'ReturningStorm', 'Starfall', 'VoidStep', 'ThunderNeedles']]
retired += ['/Game/HeavensDivide/VFX/Combos/M_ComboGround',
            '/Game/HeavensDivide/VFX/Combos/Joint/NS_JointGathering']
existing = {p for p in retired if u.EditorAssetLibrary.does_asset_exist(p)}
for path in existing:
    refs = [str(r).split('.')[0] for r in u.EditorAssetLibrary.find_package_referencers_for_asset(path, False)]
    report['referencers'][path] = refs
    external = [r for r in refs if r not in existing and r != path]
    if external:
        report['blocked'].append(dict(asset=path, references=external))
if not (out / 'ReferenceAudit.json').exists():
    (out / 'ReferenceAudit.json').write_text(json.dumps(report, indent=2))
assert not report['blocked'], json.dumps(report['blocked'], indent=2)
for path in retired:
    if path not in existing:
        continue
    protect(path)
    assert u.EditorAssetLibrary.delete_asset(path), path
    # Some source-control providers remove the registry entry but leave the
    # untracked package on disk. Remove only that exact, already-backed-up file.
    relative = Path(path.removeprefix('/Game/') + '.uasset')
    disk = (root / 'Content' / relative).resolve()
    assert disk.is_relative_to(root / 'Content')
    if disk.exists():
        assert hashlib.sha256(disk.read_bytes()).digest() == hashlib.sha256((backup / relative).read_bytes()).digest()
        disk.unlink()
    report['deleted'].append(path)

report['removed_with_backups'] = [p for p in retired if (backup / (p.removeprefix('/Game/') + '.uasset')).exists()
                                  and not (root / 'Content' / (p.removeprefix('/Game/') + '.uasset')).exists()]
report['remaining_definitions'] = list(u.EditorAssetLibrary.list_assets(folder, True, False))
assert len(report['remaining_definitions']) == 3, report['remaining_definitions']
report['alternating_montages'] = [primary.get_path_name(), alternate.get_path_name()]
(out / 'Assets.json').write_text(json.dumps(report, indent=2))
u.log('JOINT_COMBO_CLEANUP_OK ' + str(len(report['deleted'])) + ' retired assets removed')
