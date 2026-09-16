"""Apply the new Tornado radii and Ninja targeting without changing presentation assets."""
from pathlib import Path
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
for name in ['Samurai', 'Ninja']:
    path = '/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_' + name
    source = root / 'Content' / (path.removeprefix('/Game/') + '.uasset')
    backup = root / 'Saved/Backups/ComboAbilityTuning' / source.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists(): shutil.copy2(source, backup)
    bp = u.load_asset(path)
    defaults = u.get_default_object(u.EditorAssetLibrary.load_blueprint_class(path))
    settings = defaults.get_editor_property('combo_ability')
    presentation = {key: settings.get_editor_property(key) for key in ['montage', 'sound', 'vfx']}
    if name == 'Samurai':
        settings.set_editor_property('initial_radius', 360.0)
        settings.set_editor_property('final_radius', 1000.0)
    else:
        settings.set_editor_property('face_enemy_pack', True)
    defaults.set_editor_property('combo_ability', settings)
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    defaults = u.get_default_object(u.EditorAssetLibrary.load_blueprint_class(path))
    saved = defaults.get_editor_property('combo_ability')
    for key, value in presentation.items():
        assert saved.get_editor_property(key) == value, key + ' changed unexpectedly'
    if name == 'Samurai':
        assert saved.get_editor_property('initial_radius') == 360.0
        assert saved.get_editor_property('final_radius') == 1000.0
    else:
        assert saved.get_editor_property('face_enemy_pack')
    assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
    u.log('COMBO_TUNING_UPDATED ' + name)
