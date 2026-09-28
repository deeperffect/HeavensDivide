"""Repair section bindings and validate both characters, without replacing masks."""
from pathlib import Path
import unreal as u

assert u.CharacterSimulationSetupCommandlet.repair_cloth_bindings()
assert u.CharacterSimulationSetupCommandlet.verify_simulation('Samurai')
exec((Path(u.Paths.project_dir()) / 'Tools/configure_ninja_hair_simulation.py').read_text(encoding='utf-8-sig'))
u.log('CHARACTER_SIMULATION_FINAL_PASS')
