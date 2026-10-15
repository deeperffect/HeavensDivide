from pathlib import Path
import unreal as u
MAP_KEYS=['EmberQuarry']
REPORT_NAME='ObstaclePolish/QuarryFinalValidation.json'
root=Path(u.Paths.project_dir()).resolve()
exec(compile((root/'Tools/finish_quarry_boundary.py').read_text(encoding='utf-8'),'finish_quarry_boundary.py','exec'),{'__name__':'__main__'})
exec(compile((root/'Tools/review_survival_obstacle_polish.py').read_text(encoding='utf-8'),'review_survival_obstacle_polish.py','exec'))
