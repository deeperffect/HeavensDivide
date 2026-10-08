"""Double Magnetism's per-rank bonus without changing the base pickup radius."""
from pathlib import Path
import json
import shutil
import unreal as u

path = '/Game/HeavensDivide/Upgrades/Global/DA_Upgrade_GlobalPickupRadius'
asset = u.load_asset(path)
assert isinstance(asset, u.UpgradeDefinition)
root = Path(u.Paths.project_dir()).resolve()
backup = root / 'Saved/PickupAndSwapFix/Backup'
backup.mkdir(parents=True, exist_ok=True)
source = root / 'Content' / (path.removeprefix('/Game/') + '.uasset')
if not (backup / source.name).exists():
    shutil.copy2(source, backup / source.name)
entries = list(asset.get_editor_property('rarity_magnitudes'))
before = [round(e.get_editor_property('magnitude'), 4) for e in entries]
assert before in ([.15, .25, .4], [.3, .5, .8]), before
for entry, value in zip(entries, [.3, .5, .8]):
    entry.set_editor_property('magnitude', value)
asset.set_editor_property('rarity_magnitudes', entries)
modifiers = list(asset.get_editor_property('stat_modifiers'))
for modifier in modifiers:
    assert modifier.get_editor_property('shared_player_stat') == u.SharedPlayerStatType.PICKUP_RADIUS_MULTIPLIER
    assert round(modifier.get_editor_property('value_per_level'), 4) in (.15, .3)
    modifier.set_editor_property('value_per_level', .3)
assert modifiers
asset.set_editor_property('stat_modifiers', modifiers)
asset.set_editor_property('description', 'Increase pickup range by 30%.')
asset.set_editor_property('rolled_description_format', 'Increase pickup range by {Percent}%.')
assert u.EditorAssetLibrary.save_loaded_asset(asset, False)
report = {'previous': before, 'current': [.3, .5, .8]}
(backup.parent / 'PickupRange.json').write_text(json.dumps(report, indent=2))
u.log('PICKUP_RANGE_TUNING_PASS ' + json.dumps(report))
