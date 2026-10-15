from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('test_old_village_navigation_camera.py')),
               init_globals={'CAMERA_ONLY': True, 'RUN_SPAWNING': True})
