"""Read-only audit of run actors and authored spawner configuration."""
from pathlib import Path
import json
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
report = {}
properties = ['spawning_enabled', 'run_time_update_interval', 'min_spawn_distance',
              'max_spawn_distance', 'active_spawn_area', 'require_nav_mesh_projection',
              'pressure_phases', 'enemy_spawn_entries']
for name in ['Lvl_B1_Lvl1', 'Lvl_OldVillage', 'Lvl_OldVillage_Blockout']:
    assert level.load_level('/Game/Maps/' + name)
    rows = []
    for actor in actors.get_all_level_actors():
        if isinstance(actor, u.EnemySpawner):
            values = {}
            for prop in properties:
                try:
                    values[prop] = str(actor.get_editor_property(prop))
                except Exception as error:
                    values[prop] = str(error)
            rows.append({'label': actor.get_actor_label(), 'class': actor.get_class().get_path_name(), 'properties': values})
    report[name] = rows
(root / 'Saved/OldVillage/SpawnerAudit.json').write_text(json.dumps(report, indent=2))
u.log('OLD_VILLAGE_SPAWNER_AUDIT_COMPLETE')
