"""Apply Iaijutsu charge/width tuning with backups; -ValidateIaijutsuCharge is read-only."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
folder = '/Game/HeavensDivide/Upgrades/Samurai/'
validate = '-ValidateIaijutsuCharge' in unreal.SystemLibrary.get_command_line()
backup = root / 'Saved/Backups/IaijutsuCharge' / datetime.now().strftime('%Y%m%d_%H%M%S')
row = next(r for r in json.loads((root / 'Tools/samurai_build_upgrades.json').read_text()) if r['id'] == 'Iaijutsu')
changes = [
    ('DA_Upgrade_SamuraiIaijutsuStance', {'ChargeDuration': 1.0, 'SlashRadius': 100.0}, row['description']),
    ('DA_Upgrade_SamuraiIaijutsuWidth', {'PerRank': .25}, '+25% Iaijutsu lane width per rank. Maximum 5 ranks.'),
]
changed_paths = {root / 'Content/HeavensDivide/Upgrades/Samurai' / (name + '.uasset') for name, _, _ in changes}
if not validate:
    preserved = {path: hashlib.sha256(path.read_bytes()).hexdigest()
                 for path in (root / 'Content/HeavensDivide/Upgrades').rglob('*.uasset') if path not in changed_paths}
for name, tuning, description in changes:
    card = unreal.load_asset(folder + name)
    assert card, 'Missing card: ' + name
    before = {str(k): float(v) for k, v in card.get_editor_property('balance_parameters').items()}
    if not validate:
        source = root / 'Content/HeavensDivide/Upgrades/Samurai' / (name + '.uasset')
        destination = backup / source.relative_to(root)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        card.set_editor_property('balance_parameters', {**before, **tuning})
        card.set_editor_property('description', description)
        assert unreal.EditorAssetLibrary.save_loaded_asset(card, False)
        unreal.log('IAIJUTSU_TUNING: ' + name + ' ' + str({k: before.get(k) for k in tuning}) + ' -> ' + str(tuning))
    after = {str(k): float(v) for k, v in card.get_editor_property('balance_parameters').items()}
    for key, value in tuning.items():
        assert abs(after[key] - value) < .0001, (name, key, after[key], value)
    assert str(card.get_editor_property('description')) == description
    for key, value in before.items():
        if key not in tuning:
            assert after[key] == value, 'Unrelated tuning changed: ' + key
if not validate:
    for path, digest in preserved.items():
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, 'Unrelated card changed: ' + str(path)
    unreal.log('IAIJUTSU_TUNING_BACKUP: ' + str(backup))
unreal.log('IAIJUTSU_CHARGE_OK: 1s charge, 100cm half-width, +25% width per rank; other tuning preserved')
