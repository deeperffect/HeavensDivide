"""Save the final material fixes, then capture the unsaved review scene."""
from pathlib import Path
import runpy,unreal as u
root=Path(u.Paths.project_dir()).resolve()
runpy.run_path(str(root/'Tools/create_encounter_wall_material.py'))
runpy.run_path(str(root/'Tools/capture_gameplay_polish.py'))
