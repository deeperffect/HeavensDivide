"""Apply Crescent wave-only tuning with backups; -ValidateCrescentWaves is read-only."""
from datetime import datetime
from pathlib import Path
import hashlib
import shutil
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
folder = '/Game/HeavensDivide/Upgrades/Samurai/'
validate = '-ValidateCrescentWaves' in unreal.SystemLibrary.get_command_line()
backup = root / 'Saved/Backups/CrescentWaves' / datetime.now().strftime('%Y%m%d_%H%M%S')
changes = [
    ('DA_Upgrade_SamuraiCrescentStance', {'SlowFraction': .3}, 'Replaces normal melee attacks with traveling Blade Waves, without a melee swing. Wave hits slow enemies by 30% for 5 seconds. Choose one Samurai stance per run. Cannot apply Bleed.'),
    ('DA_Upgrade_SamuraiCrescentSlow', {}, '+5 percentage points slow per rank. Maximum 5 ranks; base 30% reaches 55%. Unavailable with Sudden Eruption.'),
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
        unreal.log('CRESCENT_WAVES: ' + name + ' ' + str({k: before.get(k) for k in tuning}) + ' -> ' + str(tuning))
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
    unreal.log('CRESCENT_WAVES_BACKUP: ' + str(backup))
unreal.log('CRESCENT_WAVES_OK: 30 percent slow for 5s, 55 percent at max ranks; other tuning preserved')
