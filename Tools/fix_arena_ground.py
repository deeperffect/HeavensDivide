"""Restore the boss arena floor's ground trace response and align the placed boss."""
from pathlib import Path
from datetime import datetime
import json, shutil, unreal as u
root=Path(u.Paths.project_dir()).resolve();out=root/'Saved/ArenaGroundFix'
backup=out/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S');backup.mkdir(parents=True,exist_ok=True)
source=root/'Content/Maps/Lvl_B1_Arena.umap';shutil.copy2(source,backup/source.name)
level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert level.load_level('/Game/Maps/Lvl_B1_Arena')
floor=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='Plane')
component=floor.get_component_by_class(u.StaticMeshComponent)
assert component.get_editor_property('static_mesh').get_path_name()=='/Engine/BasicShapes/Plane.Plane'
before=component.get_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY_GROUND)
before_pawn=component.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN)
floor.modify(True);component.modify(True)
# Named BlockAll reinitializes custom trace defaults on reload; store explicit responses.
component.set_collision_profile_name('Custom')
component.set_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY_GROUND,u.CollisionResponseType.ECR_BLOCK)
assert component.get_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY_GROUND)==u.CollisionResponseType.ECR_BLOCK
assert component.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN)==before_pawn
boss=next(a for a in actors.get_all_level_actors() if isinstance(a,u.CinderOracleBoss))
location=boss.get_actor_location();boss_before=str(location)
hit=u.SystemLibrary.line_trace_single(boss,location+u.Vector(0,0,600),location-u.Vector(0,0,2000),u.TraceTypeQuery.ECC_ENEMY_GROUND,False,[boss],u.DrawDebugTrace.NONE,True)
assert hit,'Arena ground trace must hit after fixing collision'
# The authored floor is a flat Engine plane; its actor Z is its surface Z.
rotation=floor.get_actor_rotation()
assert abs(rotation.pitch)<.01 and abs(rotation.roll)<.01
ground=floor.get_actor_location()
location.z=ground.z+boss.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_half_height()
boss.modify(True);boss.set_actor_location(location,False,False)
assert level.save_current_level()
assert level.load_level('/Game/Maps/Lvl_B1_Lvl1')
assert level.load_level('/Game/Maps/Lvl_B1_Arena')
floor=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='Plane')
assert floor.get_component_by_class(u.StaticMeshComponent).get_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY_GROUND)==u.CollisionResponseType.ECR_BLOCK
(out/'MapRepair.json').write_text(json.dumps({'before':str(before),'after':'ECR_BLOCK','floor_z':ground.z,'boss_before':boss_before,'boss_after':str(location),'reload_verified':True,'backup':str(backup)},indent=2))
u.log('ARENA_FLOOR_RESPONSE_REPAIRED')
