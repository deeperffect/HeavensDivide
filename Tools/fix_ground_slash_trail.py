"""Bind the active Blade Wave decal trail to the wave's heading; preserve tuning."""
from pathlib import Path
import shutil
import unreal as u

upgrade = u.load_asset('/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiCrescentStance')
assert upgrade
system = upgrade.get_editor_property('presentation').get_editor_property('pulse_system')
assert system
package = system.get_path_name().split('.')[0]
assert package == '/Game/HeavensDivide/VFX/NS_GroundSlash_BladeWave', package
source = Path(u.Paths.project_content_dir(), package.removeprefix('/Game/') + '.uasset')
backup = Path(u.Paths.project_saved_dir(), 'Backups/GroundSlashTrailOrientation', source.name)
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(source, backup)
changed = u.SwapVFXSetupLibrary.bind_ground_slash_trail_orientation(system)
assert changed == 1, f'Expected one world-space decal renderer; got {changed}'
assert u.EditorAssetLibrary.save_loaded_asset(system, False)
u.log('GROUND_SLASH_TRAIL_ORIENTATION_PASS')
