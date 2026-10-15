"""Add the missing run actor without rebuilding or moving any environment actors."""
from pathlib import Path
from datetime import datetime
import json
import shutil
import sys
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
sys.path.insert(0, str(root / 'Tools'))
from old_village_gameplay import ensure_run_spawner, repair_spawn_collision

level = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
backup = root / 'Saved/OldVillage/Backups' / (datetime.now().strftime('%Y%m%d_%H%M%S') + '_Spawning')
backup.mkdir(parents=True, exist_ok=True)
report = {}


def snapshot():
    result = {}
    for actor in actors.get_all_level_actors():
        p = actor.get_actor_location()
        r = actor.get_actor_rotation()
        s = actor.get_actor_scale3d()
        result[actor.get_path_name()] = (p.x, p.y, p.z, r.pitch, r.yaw, r.roll, s.x, s.y, s.z)
    return result


for name in ['Lvl_OldVillage', 'Lvl_OldVillage_Blockout']:
    disk = root / 'Content/Maps' / (name + '.umap')
    shutil.copy2(disk, backup / disk.name)
    assert level.load_level('/Game/Maps/' + name)
    before = snapshot()
    actor = ensure_run_spawner()
    collision_count = repair_spawn_collision()
    after = snapshot()
    changed = [key for key, value in before.items() if after.get(key) != value]
    assert not changed, 'Existing actors changed: ' + str(changed)
    assert level.save_current_level()
    report[name] = {'actors_before': len(before), 'actors_after': len(after), 'collision_components': collision_count, 'spawner': actor.get_class().get_path_name(), 'backup': str(backup / disk.name)}
(root / 'Saved/OldVillage/SpawnerFix.json').write_text(json.dumps(report, indent=2))
u.log('OLD_VILLAGE_SPAWNER_FIX_COMPLETE')
