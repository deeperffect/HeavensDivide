"""Connect the existing NS_SamuraiAbility scale input to the combo hitbox radius."""
from pathlib import Path
import shutil
import unreal as u

path = '/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai'
root = Path(u.Paths.project_dir()).resolve()
source = root / 'Content' / (path.removeprefix('/Game/') + '.uasset')
backup = root / 'Saved/Backups/SamuraiAbilityScale' / source.name
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(source, backup)
bp = u.load_asset(path)
cdo = u.get_default_object(u.EditorAssetLibrary.load_blueprint_class(path))
settings = cdo.get_editor_property('combo_ability')
assert settings.get_editor_property('vfx') == u.load_asset('/Game/Assets/VFX/Free_Spells/VFX_Niagara/NS_SamuraiAbility')
preserved = {key: settings.get_editor_property(key) for key in
             ['vfx', 'montage', 'sound', 'initial_radius', 'final_radius', 'effect_duration', 'vfx_scale']}
settings.set_editor_property('vfx_radius_parameter', 'User.Scale_All')
# Preserve the authored starting size (Scale_All=1 at radius 600 cm), then grow proportionally.
# This calibration stays fixed if the gameplay radii are tuned later.
settings.set_editor_property('vfx_reference_radius', 600.0)
cdo.set_editor_property('combo_ability', settings)
u.BlueprintEditorLibrary.compile_blueprint(bp)
saved = u.get_default_object(u.EditorAssetLibrary.load_blueprint_class(path)).get_editor_property('combo_ability')
for key, value in preserved.items():
    assert saved.get_editor_property(key) == value, key + ' changed'
assert str(saved.get_editor_property('vfx_radius_parameter')) == 'User.Scale_All'
assert saved.get_editor_property('vfx_reference_radius') == 600.0
assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
u.log('SAMURAI_ABILITY_SCALE configured: User.Scale_All = radius / 600; existing presentation and gameplay preserved')
