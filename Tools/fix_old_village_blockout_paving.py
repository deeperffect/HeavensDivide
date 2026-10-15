"""Match the blockout's low courtyard paving to the visual-only dressed paving."""
from pathlib import Path
from datetime import datetime
import shutil
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
source=root/'Content/Maps/Lvl_OldVillage_Blockout.umap'
backup=root/'Saved/OldVillage/Backups'/(datetime.now().strftime('%Y%m%d_%H%M%S')+'_Paving')
backup.mkdir(parents=True,exist_ok=True)
shutil.copy2(source,backup/source.name)
level=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/Lvl_OldVillage_Blockout')
actor=next(a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label()=='BLOCK courtyard medallion')
actor.static_mesh_component.set_collision_profile_name('NoCollision')
assert u.EnemyNavigationLibrary.bake_old_village_navigation(u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world())
assert level.save_current_level()
u.log('OLD_VILLAGE_BLOCKOUT_PAVING_FIXED')
