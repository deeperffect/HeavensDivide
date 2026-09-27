"""Add the original trail/spark layers and lay the Blade Wave crescent flat."""
from pathlib import Path
import shutil
import unreal as u

upgrade = u.load_asset('/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiBladeWave')
system = upgrade.get_editor_property('presentation').get_editor_property('pulse_system')
assert system.get_path_name().split('.')[0] == '/Game/HeavensDivide/VFX/NS_GroundSlash_BladeWave'
source = Path(u.Paths.project_content_dir(), 'HeavensDivide/VFX/NS_GroundSlash_BladeWave.uasset')
backup = Path(u.Paths.project_saved_dir(), 'Backups/GroundSlashFullEffects', source.name)
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(source, backup)
original = u.load_asset('/Game/Assets/VFX/GroundSlash/Particles/NiagaraSystems/NS_GroundSlashV1')
assert u.SwapVFXSetupLibrary.complete_ground_slash_effects(system, original)
assert u.EditorAssetLibrary.save_loaded_asset(system, False)
u.log('GROUND_SLASH_FULL_EFFECTS_PASS')
