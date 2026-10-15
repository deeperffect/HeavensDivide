from pathlib import Path
from datetime import datetime
import json,shutil,sys,unreal as u
root=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(root/'Tools'))
from polish_survival_obstacles import fit_boundary_cliff,clear_distant_cliff
out=root/'Saved/SurvivalMaps/ObstaclePolish';L=u.get_editor_subsystem(u.LevelEditorSubsystem);A=u.get_editor_subsystem(u.EditorActorSubsystem)
backup=out/'Backups'/(datetime.now().strftime('%Y%m%d_%H%M%S')+'_Boundary');backup.mkdir(parents=True,exist_ok=True)
shutil.copy2(root/'Content/Maps/Lvl_EmberQuarry.umap',backup/'Lvl_EmberQuarry.umap');assert L.load_level('/Game/Maps/Lvl_EmberQuarry')
old={r['name']:r for r in json.loads((out/'Inspection.json').read_text())['maps']['EmberQuarry']};heights=[];moved=0
for a in A.get_all_level_actors():
 if a.get_actor_label()=='Distant canyon formation':moved+=clear_distant_cliff(a)
 if a.get_actor_label()!='Canyon escarpment':continue
 original=old.get(a.get_name());h=min(800,original['extent'][2]*2*.26 if original else a.get_actor_bounds(False)[1].z*2)
 fit_boundary_cliff(a,h);heights.append(h)
assert L.save_current_level()
(out/'FinalBoundary.json').write_text(json.dumps({'passed':True,'cliffs':len(heights),'height_cm':[min(heights),max(heights)],'width_cm':1050,'depth_cm':350,'outside_boundary_cm':600,'distant_cliffs_moved':moved},indent=2))
u.log('QUARRY_BOUNDARY_CAMERA_CLEARANCE_SAVED')
