"""Update only Dash Draw's distance bonus/text. -ValidateIaijutsuDash is read-only."""
import ast
from datetime import datetime
import hashlib
from pathlib import Path
import shutil
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
path = '/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiIaijutsuDash'
card = unreal.load_asset(path)
assert card
tree = ast.parse((root / 'Tools/overhaul_iaijutsu.py').read_text(encoding='utf-8'))
rows = next(ast.literal_eval(node.value) for node in tree.body if isinstance(node, ast.Assign)
            and any(isinstance(target, ast.Name) and target.id == 'rows' for target in node.targets))
row = next(row for row in rows if row[0] == 'IaijutsuDash')
disk = root / 'Content' / (path.removeprefix('/Game/') + '.uasset')
preserved = {other: hashlib.sha256(other.read_bytes()).hexdigest()
             for other in (root / 'Content/HeavensDivide/Upgrades').rglob('*.uasset') if other != disk}
old_balance = {str(key): value for key, value in card.get_editor_property('balance_parameters').items()}
if '-ValidateIaijutsuDash' not in unreal.SystemLibrary.get_command_line():
    backup = root / 'Saved/Backups/IaijutsuDash' / datetime.now().strftime('%Y%m%d_%H%M%S')
    backup.mkdir(parents=True, exist_ok=True)
    shutil.copy2(disk, backup / disk.name)
    balance = dict(old_balance)
    balance['DashDistanceBonus'] = row[5]['DashDistanceBonus']
    card.set_editor_property('balance_parameters', balance)
    card.set_editor_property('description', row[-1])
    unreal.SystemLibrary.execute_console_command(None, 'setnopec ' + card.get_path_name() + ' bHasRuntimeBalance True')
    assert unreal.EditorAssetLibrary.save_loaded_asset(card, False)
    unreal.log('IAIJUTSU_DASH_BACKUP: ' + str(backup))
balance = {str(key): value for key, value in card.get_editor_property('balance_parameters').items()}
assert balance['DashDistanceBonus'] == 1.0
assert str(card.get_editor_property('description')) == row[-1]
assert str(card.get_editor_property('upgrade_id')) == 'IaijutsuDash'
assert [str(value) for value in card.get_editor_property('prerequisite_upgrade_ids')] == ['Iaijutsu']
assert all(balance[key] == value for key, value in old_balance.items() if key != 'DashDistanceBonus')
for other, digest in preserved.items():
    assert hashlib.sha256(other.read_bytes()).hexdigest() == digest, 'Unrelated card changed: ' + str(other)
unreal.log('IAIJUTSU_DASH_OK: Dash Draw doubles Samurai dash distance; other card tuning preserved')
