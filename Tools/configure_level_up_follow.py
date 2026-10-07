"""Keep already-emitted level-up particles with their moving component.

Run in Unreal Python; -ValidateLevelUpFollow only checks the saved asset.
"""
from pathlib import Path
import re
import shutil
import unreal as u

asset_path = '/Game/Assets/VFX/MagicProjectilesVol4/AoE_MagicAbilities/Particles/P_AoE_GrowingBeam_LevelUp'
root = Path(u.Paths.project_dir()).resolve()
validate_only = '-ValidateLevelUpFollow' in u.SystemLibrary.get_command_line()
effect = u.load_asset(asset_path)
assert isinstance(effect, u.ParticleSystem)
output = root / 'Saved/LevelUpFollow'
output.mkdir(parents=True, exist_ok=True)
task = u.AssetExportTask()
task.object = effect
task.filename = str(output / 'effect.t3d')
task.exporter = u.ObjectExporterT3D()
task.automated = True
task.prompt = False
task.replace_identical = True
assert u.Exporter.run_asset_export_task(task)
data = Path(task.filename).read_bytes()
exported = data.decode('utf-16' if data.startswith(b'\xff\xfe') else 'utf-8-sig')
paths = sorted(set(re.findall(r"ExportPath=\"/Script/Engine\.ParticleModuleRequired'([^']+)'\"", exported)))
assert paths, 'No Cascade required modules found'
modules = [u.find_object(None, path) for path in paths]
assert all(modules)
pending = [module for module in modules if not module.get_editor_property('bUseLocalSpace')]
if not validate_only and pending:
    source = root / 'Content' / (asset_path.removeprefix('/Game/') + '.uasset')
    backup = root / 'Saved/Backups/LevelUpFollow' / source.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(source, backup)
    for module in pending:
        module.set_editor_property('bUseLocalSpace', True)
    assert u.EditorAssetLibrary.save_loaded_asset(effect, False)
assert all(module.get_editor_property('bUseLocalSpace') for module in modules)
u.log('LEVEL_UP_FOLLOW_OK modules=%d changed=%d validate_only=%s' %
      (len(modules), 0 if validate_only else len(pending), validate_only))
