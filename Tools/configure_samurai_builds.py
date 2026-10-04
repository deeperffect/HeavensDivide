"""Current Samurai authoring entry point. -ValidateSamuraiBuilds is read-only."""
from pathlib import Path
import unreal
script=Path(unreal.Paths.project_dir())/'Tools/overhaul_blood_stance.py'
exec(compile(script.read_text(),str(script),'exec'),{'__name__':'__main__'})
