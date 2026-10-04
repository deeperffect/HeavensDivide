from pathlib import Path
import unreal as u

asset = u.load_asset('/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiCrescentStance')
assert asset
out = Path(u.Paths.project_saved_dir(), 'GroundSlashInspection').resolve()
out.mkdir(parents=True, exist_ok=True)
task = u.AssetExportTask()
task.object = asset
task.exporter = u.ObjectExporterT3D()
task.filename = str(out / 'blade_wave_upgrade.copy')
task.automated = True
task.prompt = False
task.replace_identical = True
assert u.Exporter.run_asset_export_task(task)
u.log('BLADE_WAVE_UPGRADE_INSPECT_PASS')
