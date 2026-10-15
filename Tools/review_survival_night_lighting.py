from pathlib import Path
import runpy,unreal as u
root=Path(u.Paths.project_dir()).resolve()
runpy.run_path(str(root/'Tools/finish_survival_night_lighting.py'))
runpy.run_path(str(root/'Tools/review_survival_maps.py'),init_globals={'MAP_KEYS':['MoonlitForest'],'SKIP_AUTOMATION':True,'REPORT_NAME':'NightLightingValidation.json'})
