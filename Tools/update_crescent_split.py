"""Migrate Splitting Waves to sideways branches and publish its player-facing copy.

Run with UnrealEditor-Cmd -run=pythonscript. Backs up the saved card, changes
only its split angle, then uses the copy publisher to update the description.
"""
import hashlib
import runpy
import shutil
from datetime import datetime
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
asset_path = '/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiCrescentSplit'
disk = root / 'Content' / (asset_path.removeprefix('/Game/') + '.uasset')
backup = root / 'Saved/Backups/CrescentSideBranches' / datetime.now().strftime('%Y%m%d_%H%M%S_%f')
backup.mkdir(parents=True)
shutil.copy2(disk, backup / disk.name)
protected = {p: hashlib.sha256(p.read_bytes()).hexdigest()
             for p in (root / 'Content/HeavensDivide/Upgrades').rglob('*.uasset') if p != disk}
card = unreal.load_asset(asset_path)
assert card and str(card.get_editor_property('upgrade_id')) == 'CrescentSplit'
before = {str(k): v for k, v in card.get_editor_property('balance_parameters').items()}
after = dict(before)
after['Angle'] = 90.0
if before != after:
    card.set_editor_property('balance_parameters', after)
    assert unreal.EditorAssetLibrary.save_loaded_asset(card, False)
assert {str(k): v for k, v in card.get_editor_property('balance_parameters').items()} == after

# This publisher saves only cards whose copy changed, preserves other properties,
# and refreshes the icon gallery's copy manifest and validation baselines.
runpy.run_path(str(root / 'Tools/publish_upgrade_copy.py'), run_name='__main__')
assert all(hashlib.sha256(p.read_bytes()).hexdigest() == h for p, h in protected.items()), 'Unrelated card changed'
unreal.log('CRESCENT_SIDE_BRANCHES_OK: angle=90; other cards unchanged; backup=' + str(backup))
