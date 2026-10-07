"""Change the level-up Cascade's blue layers to red; preserve purple and timing.

Run with Unreal Python. Add -ValidateLevelUpColors for a read-only check.
"""
import json
import re
import shutil
from pathlib import Path
import unreal as u

ASSET = '/Game/Assets/VFX/MagicProjectilesVol4/AoE_MagicAbilities/Particles/P_AoE_GrowingBeam_LevelUp'
root = Path(u.Paths.project_dir()).resolve()
output = root / 'Saved/LevelUpColors'
output.mkdir(parents=True, exist_ok=True)
validate_only = '-ValidateLevelUpColors' in u.SystemLibrary.get_command_line()
effect = u.load_asset(ASSET)
assert isinstance(effect, u.ParticleSystem)


def export():
    task = u.AssetExportTask()
    task.object = effect
    task.filename = str(output / 'effect.t3d')
    task.exporter = u.ObjectExporterT3D()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    assert u.Exporter.run_asset_export_task(task)
    data = Path(task.filename).read_bytes()
    return data.decode('utf-16' if data.startswith(b'\xff\xfe') else 'utf-8-sig')


exported = export()
paths = sorted(set(re.findall(
    r"ExportPath=\"/Script/Engine\.DistributionVector[^']*'([^']+:ParticleModuleColor[^']+)'\"",
    exported)))
colors = []
for path in paths:
    distribution = u.find_object(None, path)
    assert distribution, path
    # This asset uses constant/particle-parameter color vectors. Curves are left intact.
    try:
        color = distribution.get_editor_property('constant')
    except Exception:
        continue
    colors.append((distribution, (color.x, color.y, color.z)))
assert colors, 'No Cascade color distributions found'


def is_blue(color):
    r, g, b = color
    return b > 0.3 and r < b * 0.15 and g < b * 0.8


def is_red(color):
    r, g, b = color
    return r > 0.3 and g < r * 0.15 and b < r * 0.15


blue = [(distribution, color) for distribution, color in colors if is_blue(color)]
preserved = [(distribution, color) for distribution, color in colors if not is_blue(color)]
changes = []
if not validate_only and blue:
    source = root / 'Content' / (ASSET.removeprefix('/Game/') + '.uasset')
    backup = root / 'Saved/Backups/LevelUpColors' / source.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(source, backup)
    for distribution, before in blue:
        after = (before[2], before[1] * 0.08, before[2] * 0.025)
        distribution.set_editor_property('constant', u.Vector(*after))
        changes.append({'object': distribution.get_path_name(), 'before': before, 'after': after})
    assert u.EditorAssetLibrary.save_loaded_asset(effect, False)
    (output / 'changes.json').write_text(json.dumps(changes, indent=2), encoding='utf-8')
    export()

for distribution, before in preserved:
    current = distribution.get_editor_property('constant')
    assert (current.x, current.y, current.z) == before, 'Non-blue color changed'
current_colors = [distribution.get_editor_property('constant') for distribution, _ in colors]
assert not any(is_blue((c.x, c.y, c.z)) for c in current_colors), 'Blue layer remains'
red_count = sum(is_red((c.x, c.y, c.z)) for c in current_colors)
purple_count = sum(c.z > 0.3 and c.x >= c.z * 0.3 and c.y < c.z * 0.2 for c in current_colors)
assert red_count > 0 and purple_count > 0, 'Both red and purple layers must be present'
u.log('LEVEL_UP_COLORS_OK ' + json.dumps({'red_layers': red_count, 'purple_layers': purple_count,
                                       'changed_layers': len(changes), 'validate_only': validate_only}))
