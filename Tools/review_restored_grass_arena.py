"""Capture and verify the restored grass arena; -GroundApplyFirst restores first."""
from pathlib import Path
import unreal as u
root = Path(u.Paths.project_dir())
source = (root / 'Tools/review_reference_ground.py').read_text(encoding='utf-8-sig')
changes = {
    'OUTPUT = ROOT / "Saved/GroundReferencePass"': 'OUTPUT = ROOT / "Saved/RestoreArenaGround"',
    '"Tools/apply_reference_ground.py"': '"Tools/restore_grass_arena.py"',
    'if not LEVEL.load_level(MAP):': 'if not apply and not LEVEL.load_level(MAP):',
    'if map_hashes() != initial_hashes:': 'if not apply and map_hashes() != initial_hashes:',
    'STATE["map_hashes"] = initial_hashes': 'STATE["map_hashes"] = map_hashes()',
    'actor != STATE["floor"] and actor.get_components_by_class(u.MeshComponent)':
        'actor != STATE["floor"] and actor.get_actor_label() != "GroundRockStamps" and actor.get_components_by_class(u.MeshComponent)',
    '"GameplayMeadowLoop", "kind": "gameplay", "focus": [1500, 3000, start.z]':
        '"GameplayBoundary", "kind": "gameplay", "focus": [0, 4500, start.z]',
    '"GroundArtCloseup", "kind": "ground", "focus": [2200, 800, ground_z], "width": 4000':
        '"GrassCloseup", "kind": "ground", "focus": [1500, -1550, ground_z], "width": 1800',
    'STATE["report"]["material"] = inspect_material()':
        'STATE["report"]["material"] = inspect_material()\n    STATE["report"]["scene"] = runpy.run_path(str(ROOT / "Tools/restore_grass_arena.py"), init_globals={"APPLY": False})["REPORT"]',
}
for old, new in changes.items():
    assert source.count(old) == 1, 'Shared review harness changed: ' + old
    source = source.replace(old, new)
exec(compile(source, str(root / 'Tools/review_reference_ground.py'), 'exec'), globals())
