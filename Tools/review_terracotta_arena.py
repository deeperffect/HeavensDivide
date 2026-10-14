"""Review the terracotta arena under saved Level 1 lighting.

-GroundApplyFirst applies the backed-up asset/map pass first.
-GroundReviewFolder=<name> selects the capture folder. The shared harness's
command-file controls live in Saved/TerracottaArena.
"""
from pathlib import Path
import unreal as u
root=Path(u.Paths.project_dir())
source=(root/'Tools/review_reference_ground.py').read_text(encoding='utf-8-sig')
changes={
 'if not LEVEL.load_level(MAP):':'if not apply and not LEVEL.load_level(MAP):',
 'OUTPUT = ROOT / "Saved/GroundReferencePass"':'OUTPUT = ROOT / "Saved/TerracottaArena"',
 '"Tools/apply_reference_ground.py"':'"Tools/apply_terracotta_arena.py"',
 'MATERIAL = "/Game/HeavensDivide/Materials/EnvironmentPolish/MI_Lvl1_Ground"':'MATERIAL = "/Game/HeavensDivide/Materials/TerracottaArena/MI_TerracottaGround"',
 'PARENT = "/Game/HeavensDivide/Materials/EnvironmentPolish/M_Lvl1_Ground"':'PARENT = "/Game/HeavensDivide/Materials/TerracottaArena/M_TerracottaGround"',
 'for name in ("GrassTexture", "DirtTexture", "StoneTexture"):':'for name in ("GroundTexture",):',
 'if map_hashes() != initial_hashes:':'if not apply and map_hashes() != initial_hashes:',
 'STATE["map_hashes"] = initial_hashes':'STATE["map_hashes"] = map_hashes()',
 'actor != STATE["floor"] and actor.get_components_by_class(u.MeshComponent)':'actor != STATE["floor"] and actor.get_actor_label() not in ("GroundRockStamps","TerracottaArenaRim") and actor.get_components_by_class(u.MeshComponent)',
 '"GameplayMeadowLoop", "kind": "gameplay", "focus": [1500, 3000, start.z]':'"GameplayCenter", "kind": "gameplay", "focus": [1500, -1550, start.z]',
 '"GroundArtCloseup", "kind": "ground", "focus": [2200, 800, ground_z], "width": 4000':'"GroundArtCloseup", "kind": "ground", "focus": [2200, 800, ground_z], "width": 2200',
 '"width": 15200':'"width": 12800',
}
for old,new in changes.items():
    assert source.count(old)==1,'Shared ground review changed: '+old
    source=source.replace(old,new)
# Include the stone frame in a real gameplay-camera view.
needle='STATE["specs"] = ['
assert source.count(needle)==1
source=source.replace(needle,needle+'\n        {"name": "GameplayStoneRim", "kind": "gameplay", "focus": [0, 4500, start.z], "size": (1600, 900)},')
# Check the saved geometry as well as material and camera readback.
needle='STATE["report"]["material"] = inspect_material()'
assert source.count(needle)==1
source=source.replace(needle,needle+'\n    STATE["report"]["scene"] = runpy.run_path(str(ROOT / "Tools/inspect_terracotta_arena.py"))["REPORT"]')
exec(compile(source,str(root/'Tools/review_reference_ground.py'),'exec'),globals())
