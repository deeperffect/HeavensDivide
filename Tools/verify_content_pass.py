from pathlib import Path
exec(compile(Path(__file__).with_name('configure_content_pass.py').read_text(encoding='utf-8-sig'),__file__,'exec'),{'__file__':__file__,'VERIFY_ONLY':True})
import unreal
assert unreal.CharacterSimulationSetupCommandlet.verify_simulation('Ninja'), 'Ninja hair stability after cloth removal'
