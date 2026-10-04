import unreal as u
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve()
for script in ['create_combat_metasounds.py','configure_combat_audio.py']:
 exec(compile((root/'Tools'/script).read_text(),str(root/'Tools'/script),'exec'),{'__name__':'__main__'})
