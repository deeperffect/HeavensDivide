"""Compatibility entry point for the complete player-facing upgrade copy pass."""
import runpy
from pathlib import Path
import unreal
root = Path(unreal.Paths.project_dir()).resolve()
script = 'validate_production_upgrade_copy.py' if '-ValidateCardDescriptions' in unreal.SystemLibrary.get_command_line() else 'publish_upgrade_copy.py'
runpy.run_path(str(root / 'Tools' / script))
