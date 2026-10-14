"""Enable cloth shaders on the project-owned death silhouette, preserving its graph."""
from pathlib import Path
import json
import shutil
import unreal as u

root = Path(u.Paths.project_dir())
out = root / 'Saved/EclipseDeathCloth'
out.mkdir(parents=True, exist_ok=True)
material = u.load_asset('/Game/HeavensDivide/VFX/Death/M_DeathSilhouette')
assert material
before = bool(material.get_editor_property('used_with_clothing'))
if not before:
    source = root / 'Content/HeavensDivide/VFX/Death/M_DeathSilhouette.uasset'
    backup = out / 'Backups/M_DeathSilhouette.uasset'
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(source, backup)
    material.set_editor_property('used_with_clothing', True)
    u.MaterialEditingLibrary.recompile_material(material)
    assert u.EditorAssetLibrary.save_loaded_asset(material, False)
report = {'clothing_usage_before': before, 'clothing_usage_after': bool(material.get_editor_property('used_with_clothing')), 'characters': {}}
for name in ('Samurai', 'Ninja'):
    bp = u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_' + name)
    hero = u.get_default_object(bp.generated_class())
    mesh = hero.get_editor_property('mesh')
    asset = mesh.get_editor_property('skeletal_mesh_asset')
    report['characters'][name] = {'mesh': asset.get_path_name(), 'materials': [str(m) for m in mesh.get_materials()]}
(out / 'ClothMaterial.json').write_text(json.dumps(report, indent=2))
u.log('Death cloth material: ' + json.dumps(report))
