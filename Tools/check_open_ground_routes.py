"""Distinguish physically open nav pockets from nav inside closed scenery."""
from pathlib import Path
import unreal as u
SKIP_VIEWS=True
EXHAUSTIVE_ROUTES=True
REPORT_NAME='OpenGroundRoutes.json'
ROOT=Path(u.Paths.project_dir()).resolve()
exec(compile((ROOT/'Tools/validate_map_scenery.py').read_text(),'validate_map_scenery.py','exec'))
