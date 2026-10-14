"""Select a saved floor/rim preset. Unreal console example:
py import runpy; runpy.run_path("Tools/select_arena_variation.py", init_globals={"VARIANT_ID":"09_MoonlitSlate"})
The editor must have Level1 open. Saves only that map, with a dated backup.
Review callers pass SAVE=False to make transient material assignments.
"""
from pathlib import Path
from datetime import datetime
import json, shutil
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve()
CONFIG=json.loads((ROOT/'Art/ArenaVariations/Variants.json').read_text())
SELECTED=globals().get('VARIANT_ID',CONFIG['default'])
assert SELECTED in [v['id'] for v in CONFIG['variants']],SELECTED
SAVE=globals().get('SAVE',True)
FOLDER='/Game/HeavensDivide/Materials/ArenaVariations'
LEVEL=u.get_editor_subsystem(u.LevelEditorSubsystem)
ACTORS=u.get_editor_subsystem(u.EditorActorSubsystem)
actors=ACTORS.get_all_level_actors()
floors=[a for a in actors if a.get_actor_label()=='Plane' and isinstance(a,u.StaticMeshActor)]
assert len(floors)==1 and '/Game/Maps/Lvl_B1_Lvl1.' in floors[0].get_path_name(),'Open Level1 before selecting a variation'
rim=next(a for a in actors if a.get_actor_label()=='TerracottaArenaRim')
floor_material=u.load_asset(FOLDER+'/MI_'+SELECTED);rim_material=u.load_asset(FOLDER+'/MI_Rim_'+SELECTED)
assert floor_material and rim_material
if SAVE:
    file=ROOT/'Content/Maps/Lvl_B1_Lvl1.umap'
    dest=ROOT/'Saved/ArenaVariations/Backups'/datetime.now().strftime('%Y%m%d_%H%M%S_%f')/'Content/Maps/Lvl_B1_Lvl1.umap'
    dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,dest)
    floors[0].modify(True);rim.modify(True)
floors[0].static_mesh_component.set_material(0,floor_material)
for comp in rim.get_components_by_class(u.HierarchicalInstancedStaticMeshComponent):
    for slot in range(len(comp.static_mesh.get_editor_property('static_materials'))):comp.set_material(slot,rim_material)
if SAVE:
    assert LEVEL.set_current_level_by_name('Lvl_B1_Lvl1');assert LEVEL.save_current_level()
u.log('ARENA_VARIATION_SELECTED '+SELECTED)
