"""Read-only export of the slash system and Samurai autoattack configuration."""
from pathlib import Path
import unreal as u

out = Path(u.Paths.project_saved_dir(), 'SamuraiSlash11').resolve()
out.mkdir(parents=True, exist_ok=True)
paths = {
    'slash11': '/Game/Assets/VFX/SlashesV1/Particles/NiagaraSystems/NS_Slash_11',
    'autoattack': '/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_AutoAttackSamurai',
    'samurai': '/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai',
}
for name, path in paths.items():
    asset = u.load_asset(path)
    assert asset, path
    task = u.AssetExportTask()
    task.object = asset
    task.exporter = u.ObjectExporterT3D()
    task.filename = str(out / (name + '.copy'))
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    assert u.Exporter.run_asset_export_task(task)
u.log('SAMURAI_SLASH11_INSPECT_PASS')
