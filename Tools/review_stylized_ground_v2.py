"""Render restored V2 through the existing four-view ground review harness."""
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir())
source = (root / 'Tools/review_reference_ground.py').read_text(encoding='utf-8-sig')
changes = {
    'OUTPUT = ROOT / "Saved/GroundReferencePass"': 'OUTPUT = ROOT / "Saved/RestoreStylizedGroundV2"',
    'MATERIAL = "/Game/HeavensDivide/Materials/EnvironmentPolish/MI_Lvl1_Ground"':
        'MATERIAL = "/Game/HeavensDivide/Materials/StylizedGroundV2/MI_StylizedGroundV2"',
    'PARENT = "/Game/HeavensDivide/Materials/EnvironmentPolish/M_Lvl1_Ground"':
        'PARENT = "/Game/HeavensDivide/Materials/StylizedGround/M_StylizedGround"',
    '"Tools/apply_reference_ground.py"': '"Tools/restore_stylized_ground_v2.py"',
    'if not LEVEL.load_level(MAP):': 'if not apply and not LEVEL.load_level(MAP):',
    'if map_hashes() != initial_hashes:': 'if not apply and map_hashes() != initial_hashes:',
    'STATE["map_hashes"] = initial_hashes': 'STATE["map_hashes"] = map_hashes()',
    'STATE["report"]["material"] = inspect_material()':
        'STATE["report"]["material"] = inspect_material()\n    STATE["report"]["restoration"] = runpy.run_path(str(ROOT / "Tools/restore_stylized_ground_v2.py"), init_globals={"APPLY": False})["REPORT"]',
}
for old, new in changes.items():
    assert source.count(old) == 1, old
    source = source.replace(old, new)
exec(compile(source, str(root / 'Tools/review_reference_ground.py'), 'exec'), globals())
