import json,shutil,unreal as u
from pathlib import Path
from datetime import datetime
root=Path(u.Paths.project_dir());out=root/'Saved/EncounterObjects'
backup=out/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S')
level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
report={}
for path in ['/Game/Maps/Lvl_B1_Lvl1','/Game/Maps/Lvl_B1_Arena']:
 source=root/'Content'/(path.removeprefix('/Game/')+'.umap');dest=backup/source.relative_to(root)
 dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dest)
 assert level.load_level(path)
 all_actors=actors.get_all_level_actors()
 areas=[a for a in all_actors if isinstance(a,u.EnemySpawnArea)]
 if path.endswith('Arena') and not areas:
  floor=next(a for a in all_actors if a.get_actor_label()=='Plane')
  center,extent,radius=u.SystemLibrary.get_component_bounds(floor.get_component_by_class(u.StaticMeshComponent))
  area=actors.spawn_actor_from_class(u.EnemySpawnArea,center);area.set_actor_label('Boss Arena Spawn Bounds')
  area.get_component_by_class(u.BoxComponent).set_box_extent(u.Vector(extent.x-100,extent.y-100,500),True)
  areas=[area]
 assert len(areas)==1
 box=areas[0].get_component_by_class(u.BoxComponent)
 maps=[a for a in actors.get_all_level_actors() if isinstance(a,u.MinimapBounds)]
 if not maps:
  maps=[actors.spawn_actor_from_class(u.MinimapBounds,box.get_world_location())];maps[0].set_actor_label('Bloodshift Minimap Bounds')
 assert len(maps)==1
 bounds=maps[0];bounds.modify(True)
 bounds.set_actor_transform(box.get_world_transform(),False,False)
 bounds.get_component_by_class(u.BoxComponent).set_box_extent(box.get_unscaled_box_extent(),True)
 assert level.save_current_level()
 report[path]={'area':areas[0].get_actor_label(),'center':str(box.get_world_location()),'extent':str(box.get_scaled_box_extent()),'minimap_aligned':True}
for path in report:
 assert level.load_level(path)
 all_actors=actors.get_all_level_actors()
 area=next(a for a in all_actors if isinstance(a,u.EnemySpawnArea))
 bounds=next(a for a in all_actors if isinstance(a,u.MinimapBounds))
 a=area.get_component_by_class(u.BoxComponent);b=bounds.get_component_by_class(u.BoxComponent)
 assert (a.get_world_location()-b.get_world_location()).length()<.1
 assert (a.get_scaled_box_extent()-b.get_scaled_box_extent()).length()<.1
 report[path]['reload_verified']=True
(out/'MapBounds.json').write_text(json.dumps(report,indent=2))
u.log('ENCOUNTER_OBJECT_BOUNDS_APPLIED')
