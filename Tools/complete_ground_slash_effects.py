"""Keep the original trail/spark layers and stand the Blade Wave crescent upright.

Run with -AllowCommandletRendering -RenderOffscreen (not -nullrhi).
Only the working Niagara system is saved; upgrade tuning and vendor assets are preserved.
"""
from datetime import datetime, timezone
import hashlib
from pathlib import Path
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
preserved = {path: hashlib.sha256(path.read_bytes()).hexdigest()
             for directory in ('HeavensDivide/Upgrades', 'Assets/VFX/GroundSlash')
             for path in (root / 'Content' / directory).rglob('*.uasset')}
upgrade = u.load_asset('/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiCrescentStance')
system = upgrade.get_editor_property('presentation').get_editor_property('pulse_system')
assert system.get_path_name().split('.')[0] == '/Game/HeavensDivide/VFX/NS_GroundSlash_BladeWave'
source = Path(u.Paths.project_content_dir(), 'HeavensDivide/VFX/NS_GroundSlash_BladeWave.uasset')
backup = Path(u.Paths.project_saved_dir(), 'Backups/GroundSlashFullEffects',
              datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S_%f'), source.name)
backup.parent.mkdir(parents=True, exist_ok=True)
shutil.copy2(source, backup)
original = u.load_asset('/Game/Assets/VFX/GroundSlash/Particles/NiagaraSystems/NS_GroundSlashV1')
assert u.SwapVFXSetupLibrary.complete_ground_slash_effects(system, original)
assert u.EditorAssetLibrary.save_loaded_asset(system, False)
assert all(hashlib.sha256(path.read_bytes()).hexdigest() == value for path, value in preserved.items()), 'Unrelated asset changed'
u.log('GROUND_SLASH_UPRIGHT_BACKUP: ' + str(backup))
u.log('GROUND_SLASH_FULL_EFFECTS_PASS: upright crescent; upgrade tuning and vendor assets preserved')
