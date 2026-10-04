"""Compatibility entry point: Heavy Blade is now ordinary shared Attack Damage."""
from pathlib import Path
import unreal

script = Path(unreal.Paths.project_dir()) / 'Tools/configure_samurai_shared_scaling.py'
exec(compile(script.read_text(encoding='utf-8'), str(script), 'exec'), {'__name__': '__main__'})
