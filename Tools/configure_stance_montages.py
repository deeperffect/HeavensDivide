"""Assign Samurai stance montages; -ValidateStanceMontages checks without saving."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
validate = '-ValidateStanceMontages' in u.SystemLibrary.get_command_line()
bp_path = '/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai'
montage_folder = '/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/'
assignments = {
    'crescent_montage': 'AM_SamuraiBladeWave',
    'crescent_alternate_montage': 'AM_SamuraiBladeWave2',
    'iaijutsu_montage': 'AM_SamuraiIaijutsu',
}
bp = u.load_asset(bp_path)
assert bp, 'Missing BP_Samurai'
attack = u.get_default_object(bp.generated_class()).get_component_by_class(u.AutoAttackComponent)
assert attack, 'Missing AutoAttackComponent'
preserved_properties = {name: attack.get_editor_property(name) for name in
                        ['attack_montage', 'alternate_attack_montage', 'double_cut_montage',
                         'iaijutsu_endpoint_montage', 'attack_interval']}
assets = {prop: u.load_asset(montage_folder + name) for prop, name in assignments.items()}
assert all(assets.values()), 'Missing stance montage'

if not validate:
    backup = root / 'Saved/Backups/StanceMontages' / datetime.now().strftime('%Y%m%d_%H%M%S')
    backup.mkdir(parents=True, exist_ok=True)
    source = root / 'Content/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai.uasset'
    shutil.copy2(source, backup / source.name)
    # The assignment only saves this Blueprint; protect existing tuning and animations.
    preserved_files = {p: hashlib.sha256(p.read_bytes()).hexdigest()
                       for folder in ['Content/HeavensDivide/Upgrades',
                                      'Content/HeavensDivide/Blueprints/PlayerCharacters/Montages']
                       for p in (root / folder).rglob('*.uasset')}
    before = {prop: str(attack.get_editor_property(prop)) for prop in assignments}
    for prop, asset in assets.items():
        attack.set_editor_property(prop, asset)
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    attack = u.get_default_object(bp.generated_class()).get_component_by_class(u.AutoAttackComponent)

for prop, asset in assets.items():
    assert attack.get_editor_property(prop) == asset, 'Incorrect montage: ' + prop
for prop, original in preserved_properties.items():
    assert attack.get_editor_property(prop) == original, 'Unrelated property changed: ' + prop
if not validate:
    assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
    for path, digest in preserved_files.items():
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, 'Unrelated asset changed: ' + str(path)
    (backup / 'assignment.json').write_text(json.dumps({
        'before': before, 'after': {prop: asset.get_path_name() for prop, asset in assets.items()}
    }, indent=2), encoding='utf-8')
    u.log('STANCE_MONTAGES_BACKUP: ' + str(backup))
u.log('STANCE_MONTAGES_OK: alternating Crescent and player Iaijutsu montages assigned')
