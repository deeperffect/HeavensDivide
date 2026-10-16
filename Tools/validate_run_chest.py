from pathlib import Path
import unreal as u
path=Path(u.Paths.project_dir()).resolve()/'Tools/validate_run_fixes.py'
exec(compile(path.read_text(),str(path),'exec'),dict(MAPS=['Lvl_OldVillage']))
