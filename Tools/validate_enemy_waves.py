"""Read-only verification of saved wave configuration in a fresh Unreal process."""
import runpy
from pathlib import Path

runpy.run_path(str(Path(__file__).with_name('configure_enemy_waves.py')), init_globals={'VALIDATE_ONLY': True})
