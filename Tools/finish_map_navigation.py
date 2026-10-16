"""Bake only the player-connected navigation component into each saved arena."""
from pathlib import Path
from datetime import datetime
import json, shutil, unreal as u
ROOT=Path(u.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/MapSceneryAudit'
backup=OUT/'Backups'/('BeforeNavigationPrune_'+datetime.now().strftime('%Y%m%d_%H%M%S'))
backup.mkdir(parents=True,exist_ok=True)
L=u.get_editor_subsystem(u.LevelEditorSubsystem)
saved=[]
for key in ['OldVillage','MoonlitForest','SunkenSanctuary','EmberQuarry']:
 disk=ROOT/'Content/Maps'/('Lvl_'+key+'.umap')
 shutil.copy2(disk,backup/disk.name)
 assert L.load_level('/Game/Maps/Lvl_'+key)
 world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
 assert u.EnemyNavigationLibrary.bake_old_village_navigation(world)
 assert L.save_current_level()
 saved.append(key)
(OUT/'NavigationPrune.json').write_text(json.dumps({'saved_maps':saved,'backup':str(backup)},indent=2))
exec(compile((ROOT/'Tools/check_map_connectivity.py').read_text(),'check_map_connectivity.py','exec'))
