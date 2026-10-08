"""Halve Fang range/travel speed and cap repeat launches; preserve other saved tuning."""
from pathlib import Path
from datetime import datetime
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
path = '/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaReturningFangStance'
asset = u.load_asset(path)
assert asset
disk = root / 'Content' / (path.removeprefix('/Game/') + '.uasset')
backup = root / 'Saved/Backups/FangPace' / datetime.now().strftime('%Y%m%d_%H%M%S_%f')
backup.mkdir(parents=True)
shutil.copy2(disk, backup / disk.name)
before = {str(k): v for k, v in asset.get_editor_property('balance_parameters').items()}
changes = dict(TargetingRangeMultiplier=.5, FlightSpeedMultiplier=.5, MaxLaunchesPerSecond=4.)
asset.set_editor_property('balance_parameters', dict(before, **changes))
u.SystemLibrary.execute_console_command(None, 'setnopec ' + asset.get_path_name() + ' bHasRuntimeBalance True')
assert u.EditorAssetLibrary.save_loaded_asset(asset, False)
after = {str(k): v for k, v in asset.get_editor_property('balance_parameters').items()}
assert all(after[k] == v for k, v in before.items() if k not in changes)
assert all(after[k] == v for k, v in changes.items())
u.log('FANG_PACE_OK: half range, half flight speed, four launches per second; backup=' + str(backup))
