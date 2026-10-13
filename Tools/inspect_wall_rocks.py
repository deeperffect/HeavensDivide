"""Read the candidate wall mesh bounds/materials without changing any assets."""
import json
from pathlib import Path
import unreal as u

report = []
for name in ['SM_PondRock01a', 'SM_PondRock02a', 'SM_PondRock03a',
             'SM_Rock_Ground_01a', 'SM_Rock_Ground_02a', 'SM_Rock_Ground_03a']:
    path = '/Game/Assets/Environment/JapaneseShrine/Meshes/' + name
    mesh = u.load_asset(path)
    assert isinstance(mesh, u.StaticMesh), path
    bounds = mesh.get_bounds()
    report.append(dict(path=mesh.get_path_name(), origin=str(bounds.origin),
                       extent=str(bounds.box_extent),
                       materials=[str(m.material_interface.get_path_name())
                                  for m in mesh.get_editor_property('static_materials')]))
out = Path(u.Paths.project_saved_dir()) / 'WallWave' / 'RockAssets.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2))
u.log('WALL_ROCK_ASSETS_OK')
