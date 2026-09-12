"""Export existing card illustrations for style matching (read-only assets)."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = root / 'Art/UpgradeCards/References'
out.mkdir(parents=True, exist_ok=True)
records = []
for name in ['Samurai/BladeWave', 'Samurai/Deathblow', 'Samurai/CrossingBlades', 'Ninja/VenomousKunai', 'Ninja/ShadowStep', 'Synergy/Hemotoxin']:
    asset = unreal.load_asset('/Game/HeavensDivide/Blueprints/UI/CardArt2/' + name)
    if not asset:
        continue
    filename = out / (name.replace('/', '_') + '.png')
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = str(filename)
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.TextureExporterPNG()
    assert unreal.Exporter.run_asset_export_task(task), name
    records.append({'asset': asset.get_path_name(), 'file': str(filename)})
(out / 'references.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
unreal.log('ART_REFERENCES_EXPORTED: ' + str(len(records)))
