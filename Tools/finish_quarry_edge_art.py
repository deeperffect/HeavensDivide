from pathlib import Path
from datetime import datetime
import unreal as u,sys,shutil
root=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(root/'Tools'))
from quarry_edge_apron import install
L=u.get_editor_subsystem(u.LevelEditorSubsystem)
backup=root/'Saved/SurvivalMaps/ObstaclePolish/Backups'/(datetime.now().strftime('%Y%m%d_%H%M%S')+'_GroundEdge');backup.mkdir(parents=True,exist_ok=True)
shutil.copy2(root/'Content/Maps/Lvl_EmberQuarry.umap',backup/'Lvl_EmberQuarry.umap')
assert L.load_level('/Game/Maps/Lvl_EmberQuarry');install();assert L.save_current_level()
MAP_KEYS=['EmberQuarry'];PREVIEW_ONLY=True;OVERVIEW_ONLY=True;SKIP_AUTOMATION=True;REPORT_NAME='ObstaclePolish/FinalArtValidation.json'
exec(compile((root/'Tools/review_survival_maps.py').read_text(encoding='utf-8'),'review_survival_maps.py','exec'))
