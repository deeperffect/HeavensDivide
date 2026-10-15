"""Render and validate expanded maps, then run navigation and event PIE checks."""
from pathlib import Path
import runpy
import unreal as u
runpy.run_path(str(Path(u.Paths.project_dir()).resolve()/'Tools/review_old_village.py'),
               init_globals={'RUN_GAMEPLAY_CHECKS':True})
