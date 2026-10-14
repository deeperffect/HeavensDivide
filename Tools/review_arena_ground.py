"""Render the current arena grass/dirt pass with the saved map lighting.

Uses the reference-ground capture harness with stone stamps visible. Pass
-GroundApplyFirst to apply, -GroundReviewFolder=<name> to name the capture set.
The underlying harness supports command-file iteration; see review_reference_ground.py.
"""
from pathlib import Path
import unreal as u
root=Path(u.Paths.project_dir())
source=(root/'Tools/review_reference_ground.py').read_text(encoding='utf-8-sig')
old, replacement = 'OUTPUT = ROOT / "Saved/GroundReferencePass"','OUTPUT = ROOT / "Saved/ArenaGrass"'
assert source.count(old)==1, 'Shared review harness changed: '+old
source=source.replace(old,replacement)
old, replacement = '"Tools/apply_reference_ground.py"','"Tools/apply_arena_grass.py"'
assert source.count(old)==1, 'Shared review harness changed: '+old
source=source.replace(old,replacement)
old, replacement = 'actor != STATE["floor"] and actor.get_components_by_class(u.MeshComponent)', 'actor != STATE["floor"] and actor.get_actor_label() != "GroundRockStamps" and actor.get_components_by_class(u.MeshComponent)'
assert source.count(old)==1, 'Shared review harness changed: '+old
source=source.replace(old,replacement)
old, replacement = '"GameplayMeadowLoop", "kind": "gameplay", "focus": [1500, 3000, start.z]', '"GameplayGrassIsland", "kind": "gameplay", "focus": [1500, -1550, start.z]'
assert source.count(old)==1, 'Shared review harness changed: '+old
source=source.replace(old,replacement)
old, replacement = '"GroundArtCloseup", "kind": "ground", "focus": [2200, 800, ground_z], "width": 4000', '"GrassCloseup", "kind": "ground", "focus": [1500, -1550, ground_z], "width": 1800'
assert source.count(old)==1, 'Shared review harness changed: '+old
source=source.replace(old,replacement)
exec(compile(source,str(root/'Tools/review_reference_ground.py'),'exec'),globals())
