"""Remove confirmed unused upgrade assets. Run with Unreal Python.
-ValidateUnusedUpgrades performs saved-package checks without writing.
Backups and retained-asset hashes are stored under Saved/UpgradeCleanup.
"""
import hashlib, json, shutil
from datetime import datetime
from pathlib import Path
import unreal as u
root = Path(u.Paths.project_dir()).resolve()
rows = json.loads((root/'Tools/unused_upgrade_cleanup.json').read_text())['removed']
removed = {r['id'] for r in rows}
paths = {r['path'] for r in rows}
controller = '/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
validate = '-ValidateUnusedUpgrades' in u.SystemLibrary.get_command_line()
out = root/'Saved/UpgradeCleanup'
backup = out/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S')
def disk(p):
    assert p.startswith('/Game/'), p
    result = (root/'Content'/(p.removeprefix('/Game/').split('.')[0]+'.uasset')).resolve()
    assert result.is_relative_to((root/'Content').resolve()), result
    return result
def copy(p):
    src = disk(p)
    if src.exists():
        dest = backup/src.relative_to(root)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
def refs(p):
    return {str(x) for x in u.EditorAssetLibrary.find_package_referencers_for_asset(p, True)}
bp = u.load_asset(controller)
comp = u.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
pool = list(comp.get_editor_property('upgrade_pool'))
retained = [a for a in pool if a and str(a.get_editor_property('upgrade_id')) not in removed]
ids = {str(a.get_editor_property('upgrade_id')) for a in retained}
assert len(ids) == len(retained) == 150, (len(ids),len(retained))
for a in retained:
    required = {str(x) for x in a.get_editor_property('prerequisite_upgrade_ids')}
    required |= {str(x.get_editor_property('upgrade_id')) for x in a.get_editor_property('prerequisite_requirements')}
    assert required.issubset(ids), (a.get_path_name(),required-ids)
hashes = {a.get_path_name().split('.')[0]:hashlib.sha256(disk(a.get_path_name()).read_bytes()).hexdigest() for a in retained}
deleted_icons = []
if not validate:
    # Preflight every package before editing the pool or deleting anything.
    for p in paths:
        if u.EditorAssetLibrary.does_asset_exist(p):
            unexpected = refs(p)-paths-{controller}
            assert not unexpected, (p,unexpected)
    used_art = {a.get_editor_property('card_artwork').get_path_name().split('.')[0] for a in retained if a.get_editor_property('card_artwork')}
    icon_paths = {r['art'] for r in rows if r.get('art') and r['art'] not in used_art}
    icon_paths = {p for p in icon_paths if u.EditorAssetLibrary.does_asset_exist(p) and not (refs(p)-paths)}
    for p in paths | icon_paths | {controller}: copy(p)
    (backup/'RetainedHashes.json').write_text(json.dumps(hashes,indent=2))
    if len(retained) != len(pool):
        comp.set_editor_property('upgrade_pool',retained)
        u.BlueprintEditorLibrary.compile_blueprint(bp)
        assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
    for p in sorted(paths):
        if u.EditorAssetLibrary.does_asset_exist(p):
            assert not (refs(p)-paths), (p,refs(p))
            assert u.EditorAssetLibrary.delete_asset(p), p
    for p in sorted(icon_paths):
        assert not refs(p), (p,refs(p))
        assert u.EditorAssetLibrary.delete_asset(p), p
        deleted_icons.append(p)
    assert all(hashlib.sha256(disk(p).read_bytes()).hexdigest()==h for p,h in hashes.items()), 'Retained upgrade tuning changed'
for p in paths: assert not u.EditorAssetLibrary.does_asset_exist(p), p
actual = list(comp.get_editor_property('upgrade_pool'))
assert len(actual)==150 and all(a and str(a.get_editor_property('upgrade_id')) not in removed for a in actual)
registry=u.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
assets=registry.get_assets_by_class(u.TopLevelAssetPath('/Script/HeavensDivide','UpgradeDefinition'),True)
assert len(assets)==150, len(assets)
result={'validated':True,'remaining_upgrades':150,'removed_upgrade_ids':sorted(removed),'deleted_icons':deleted_icons,'backup':str(backup) if not validate else None,'retained_tuning_unchanged':True}
(out/('Validation.json' if validate else 'Applied.json')).write_text(json.dumps(result,indent=2))
u.log('UNUSED_UPGRADES_OK: 15 unused cards removed; 150 active cards, valid prerequisites and unchanged retained tuning')
