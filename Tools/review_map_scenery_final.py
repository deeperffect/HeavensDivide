"""Render final foliage and verify saved navigation in gameplay after pruning."""
from pathlib import Path
import unreal as u
PREVIEW_ONLY=False
EXHAUSTIVE_ROUTES=True
REPORT_NAME='FinalVisualValidation.json'
ROOT=Path(u.Paths.project_dir()).resolve()
exec(compile((ROOT/'Tools/validate_map_scenery.py').read_text(),'validate_map_scenery.py','exec'))
