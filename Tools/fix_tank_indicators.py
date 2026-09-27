"""Update the Ogre's saved material without modifying boss assets or combat tuning."""
from pathlib import Path
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
material = u.load_asset('/Game/HeavensDivide/Materials/M_AttackIndicatorRectangle')
assert material
for name in ['BP_EnemyOgre', 'BP_EnemyGorilla']:
    path = '/Game/HeavensDivide/Blueprints/EnemyCharacters/Elites/' + name
    source = root / 'Content' / (path.removeprefix('/Game/') + '.uasset')
    backup = root / 'Saved/Backups/TankIndicatorFix' / (name + '.uasset')
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(source, backup)
    bp = u.load_asset(path)
    cdo = u.get_default_object(bp.generated_class())
    if cdo.get_editor_property('attack_shape') == u.TankSlamAttackShape.BOX:
        cdo.set_editor_property('attack_telegraph_material', material)
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
u.log('TANK_INDICATOR_FIX_PASS')
