"""Bind stationary debris lifetime; run with -AllowCommandletRendering -RenderOffscreen (not -nullrhi)."""
from datetime import datetime
import hashlib
from pathlib import Path
import shutil
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
path = '/Game/HeavensDivide/VFX/NS_GroundSlash_BladeWave'
disk = root / 'Content/HeavensDivide/VFX/NS_GroundSlash_BladeWave.uasset'
system = unreal.load_asset(path)
assert system
preserved = {other: hashlib.sha256(other.read_bytes()).hexdigest()
             for directory in ('HeavensDivide/Upgrades', 'Assets/VFX/GroundSlash')
             for other in (root / 'Content' / directory).rglob('*.uasset')}
backup = root / 'Saved/Backups/CrescentFieldDebris' / datetime.now().strftime('%Y%m%d_%H%M%S')
backup.mkdir(parents=True, exist_ok=True)
shutil.copy2(disk, backup / disk.name)
bindings = unreal.SwapVFXSetupLibrary.bind_ground_slash_debris(system)
assert bindings == 3, f'Expected three debris renderers and two ground lifetime bindings; result={bindings}'
assert unreal.EditorAssetLibrary.save_loaded_asset(system, False)
for other, digest in preserved.items():
    assert hashlib.sha256(other.read_bytes()).hexdigest() == digest, 'Unrelated asset changed: ' + str(other)
unreal.log('CRESCENT_FIELD_DEBRIS_BACKUP: ' + str(backup))
unreal.log('CRESCENT_FIELD_DEBRIS_OK: Ground lifetime bindings saved; card tuning and vendor assets preserved')
