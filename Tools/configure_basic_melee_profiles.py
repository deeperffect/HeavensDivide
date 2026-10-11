"""Apply wobble/timing to basic chasers only; run verify_basic_melee.py in a fresh process."""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
profiles = json.loads((root / 'Tools/basic_melee_profiles.json').read_text())
backup = root / 'Saved/Backups/MeleeWobble' / datetime.now().strftime('%Y%m%d_%H%M%S')
presentation = {'use_procedural_attack_motion': True, 'attack_wobble_angle': 12.0}
prepared = []
for path, values in profiles.items():
    bp = u.load_asset(path)
    assert bp, path
    cdo = u.get_default_object(bp.generated_class())
    assert isinstance(cdo, u.MeleeEnemyBase) and not isinstance(cdo, u.MontageMeleeEnemyBase), path
    # Validate the whole roster before the first asset write.
    for key in dict(values, **presentation):
        cdo.get_editor_property(key)
    relative = Path(path.removeprefix('/Game/') + '.uasset')
    destination = backup / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / 'Content' / relative, destination)
    prepared.append((path, bp, cdo, values))

for path, bp, cdo, values in prepared:
    for key, value in dict(values, **presentation).items():
        cdo.set_editor_property(key, value)
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = u.get_default_object(bp.generated_class())
    for key, value in dict(values, **presentation).items():
        assert abs(cdo.get_editor_property(key) - value) < 0.0001, (path, key)
    assert u.EditorAssetLibrary.save_loaded_asset(bp, False), path
    u.log('MELEE_PROFILE_SAVED ' + path + ' ' + json.dumps(values))
u.log('MELEE_PROFILES_COMPLETE count=' + str(len(prepared)))
