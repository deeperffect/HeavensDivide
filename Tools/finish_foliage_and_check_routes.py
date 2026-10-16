from pathlib import Path
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
for name in ['restore_foliage_parameters.py','check_map_connectivity.py']:
 exec(compile((root/'Tools'/name).read_text(),name,'exec'),{'__name__':'__main__'})
