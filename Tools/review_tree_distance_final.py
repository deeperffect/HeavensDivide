"""Review Nanite foliage shape preservation; preserve the gameplay report."""
from pathlib import Path
import unreal as u
PREVIEW_ONLY=True
OVERVIEWS_ONLY=True
MAP_KEYS=['OldVillage','MoonlitForest','SunkenSanctuary']
REPORT_NAME='TreeDistanceValidation.json'
ROOT=Path(u.Paths.project_dir()).resolve()
exec(compile((ROOT/'Tools/validate_map_scenery.py').read_text(),'validate_map_scenery.py','exec'))
