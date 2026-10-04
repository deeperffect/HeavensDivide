"""Reduce only grass texture size; preserve blend coverage, stone, and dirt tuning."""
from pathlib import Path
import json
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
folder = '/Game/HeavensDivide/Materials/StylizedGround/'
out = root / 'Saved/StylizedGround'
out.mkdir(parents=True, exist_ok=True)
backup = root / 'Saved/Backups/StylizedGroundGrassScale'
backup.mkdir(parents=True, exist_ok=True)
lib = u.MaterialEditingLibrary
report = []

def preserve(asset):
    source = root / 'Content' / (asset.get_path_name().split('.')[0].removeprefix('/Game/') + '.uasset')
    target = backup / source.name
    if not target.exists():
        shutil.copy2(source, target)

# Record instance overrides before changing parent defaults.
instances = []
for name in ['MI_StylizedGround', 'MI_StylizedGround_Landscape']:
    instance = u.load_asset(folder + name)
    assert instance
    overrides = instance.get_editor_property('scalar_parameter_values')
    for entry in overrides:
        if str(entry.parameter_info.name) == 'GrassTileSize':
            instances.append((instance, float(entry.parameter_value)))

for name in ['M_StylizedGround', 'M_StylizedGround_Landscape', 'M_StylizedGround_Preview']:
    material = u.load_asset(folder + name)
    assert material
    nodes = [e for e in lib.get_material_expressions(material)
             if isinstance(e, u.MaterialExpressionScalarParameter)
             and str(e.get_editor_property('parameter_name')) == 'GrassTileSize']
    assert len(nodes) == 1, name
    old = float(nodes[0].get_editor_property('default_value'))
    preserve(material)
    nodes[0].set_editor_property('default_value', 350.0)
    lib.recompile_material(material)
    assert u.EditorAssetLibrary.save_loaded_asset(material, False)
    assert nodes[0].get_editor_property('default_value') == 350.0
    report.append({'asset': material.get_path_name(), 'before_cm': old, 'after_cm': 350})

for instance, old in instances:
    preserve(instance)
    assert lib.set_material_instance_scalar_parameter_value(instance, 'GrassTileSize', 350.0)
    lib.update_material_instance(instance)
    assert u.EditorAssetLibrary.save_loaded_asset(instance, False)
    report.append({'asset': instance.get_path_name(), 'before_cm': old, 'after_cm': 350})

for name in ['MI_StylizedGround', 'MI_StylizedGround_Landscape']:
    assert lib.get_material_instance_scalar_parameter_value(u.load_asset(folder + name), 'GrassTileSize') == 350.0
(out / 'grass_scale_change.json').write_text(json.dumps(report, indent=2))
u.log('GRASS_SCALE_PASS ' + json.dumps(report))
