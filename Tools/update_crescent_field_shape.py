"""Update field path/inheritance descriptions and remove the obsolete radius. -ValidateCrescentFieldShape is read-only."""
import ast
from datetime import datetime
import hashlib
from pathlib import Path
import shutil
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
tree = ast.parse((root / 'Tools/overhaul_crescent.py').read_text(encoding='utf-8'))
rows = next(ast.literal_eval(node.value) for node in tree.body if isinstance(node, ast.Assign)
            and any(isinstance(target, ast.Name) and target.id == 'rows' for target in node.targets))
targets = {}
for upgrade_id in ('CrescentField', 'CrescentFieldChance'):
    path = '/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_Samurai' + upgrade_id
    card = unreal.load_asset(path)
    assert card, path
    disk = root / 'Content' / (path.removeprefix('/Game/') + '.uasset')
    description = next(row[-1] for row in rows if row[0] == upgrade_id)
    balance = {str(key): value for key, value in card.get_editor_property('balance_parameters').items()}
    if upgrade_id == 'CrescentField':
        balance.pop('Radius', None)
    targets[disk] = (card, description, balance)
preserved = {other: hashlib.sha256(other.read_bytes()).hexdigest()
             for other in (root / 'Content/HeavensDivide/Upgrades').rglob('*.uasset') if other not in targets}
if '-ValidateCrescentFieldShape' not in unreal.SystemLibrary.get_command_line():
    backup = root / 'Saved/Backups/CrescentFieldShape' / datetime.now().strftime('%Y%m%d_%H%M%S')
    backup.mkdir(parents=True, exist_ok=True)
    for disk, (card, description, balance) in targets.items():
        shutil.copy2(disk, backup / disk.name)
        card.set_editor_property('balance_parameters', balance)
        card.set_editor_property('description', description)
        assert unreal.EditorAssetLibrary.save_loaded_asset(card, False)
    unreal.log('CRESCENT_FIELD_SHAPE_BACKUP: ' + str(backup))
for card, description, balance in targets.values():
    assert str(card.get_editor_property('description')) == description
    assert {str(key): value for key, value in card.get_editor_property('balance_parameters').items()} == balance
for other, digest in preserved.items():
    assert hashlib.sha256(other.read_bytes()).hexdigest() == digest, 'Unrelated card changed: ' + str(other)
unreal.log('CRESCENT_FIELD_SHAPE_OK: Completed-path/inherited-split descriptions and tuning validated; other cards preserved')
