"""Make the trial's authored fire emitter inherit Niagara component rotation."""
from pathlib import Path
import json
import shutil
import unreal as u

path = '/Game/Assets/VFX/StylizedSmokeV1/Particles/NiagaraSystems/NS_StylizedSmoke_Loop_v08_Fire'
root = Path(u.Paths.project_dir()).resolve()
source = root / 'Content' / (path.removeprefix('/Game/') + '.uasset')
backup = root / 'Saved/Backups/SamuraiFireRotation' / source.name
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(source, backup)
system = u.load_asset(path)
assert system
changed = u.SwapVFXSetupLibrary.prepare_portal_local_space(system)
assert u.SwapVFXSetupLibrary.prepare_portal_local_space(system) == 0
assert u.EditorAssetLibrary.save_loaded_asset(system, False)
out = root / 'Saved/SamuraiFire'
out.mkdir(parents=True, exist_ok=True)
task = u.AssetExportTask()
task.object = system
task.exporter = u.ObjectExporterT3D()
task.filename = str(out / 'fire_local_space.copy')
task.automated = True
task.prompt = False
task.replace_identical = True
assert u.Exporter.run_asset_export_task(task)
data = Path(task.filename).read_bytes()
text = data.decode('utf-16' if data.startswith(b'\xff\xfe') else 'utf-8-sig')
assert 'bLocalSpace=True' in text
report = {'system': path, 'emitters_changed_to_local_space': changed,
          'all_active_emitters_local_space': True, 'backup': str(backup)}
(out / 'rotation_fix.json').write_text(json.dumps(report, indent=2))
u.log('SAMURAI_FIRE_ROTATION_FIXED ' + json.dumps(report))
