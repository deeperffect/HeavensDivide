"""Select the approved animated courtyard movie in the existing menu media assets.

Run using UnrealEditor-Cmd with -run=pythonscript and PythonScriptPlugin plus
EditorScriptingUtilities enabled. Re-running verifies the saved configuration
without rewriting already configured assets.
"""

from datetime import datetime
import json
from pathlib import Path
import shutil

import unreal


root = Path(unreal.Paths.project_dir()).resolve()
movie = root / 'Content/Movies/CrimsonCourtyard_Animated_Loop.mp4'
assert movie.is_file() and movie.stat().st_size > 0, 'Animated courtyard video is missing'
asset_root = '/Game/HeavensDivide/Blueprints/UI/MainMenu/'
source = unreal.load_asset(asset_root + 'main_menu')
player = unreal.load_asset(asset_root + 'MP_MainMenu')
menu = unreal.load_asset(asset_root + 'WBP_MainMenu')
assert isinstance(source, unreal.FileMediaSource), 'Existing menu media source is missing'
assert isinstance(player, unreal.MediaPlayer), 'Existing menu media player is missing'
assert menu, 'Main menu widget is missing'
defaults = unreal.get_default_object(menu.generated_class())
assert defaults.get_editor_property('background_media_source') == source
assert defaults.get_editor_property('background_media_player') == player
texture = defaults.get_editor_property('background_media_texture')
assert texture, 'Main menu has no background media texture'
assert texture.get_editor_property('media_player') == player

expected_path = './Movies/CrimsonCourtyard_Animated_Loop.mp4'
previous_path = str(source.get_editor_property('file_path'))
previous_looping = bool(player.get_editor_property('loop'))
changes = []
if previous_path.replace('\\', '/') != expected_path:
    changes.append(('main_menu', source))
if not previous_looping:
    changes.append(('MP_MainMenu', player))

backup = None
if changes:
    backup = root / 'Saved/Backups/MenuMedia' / ('AnimatedCourtyard_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))
    backup.mkdir(parents=True)
    for name, _ in changes:
        original = root / 'Content/HeavensDivide/Blueprints/UI/MainMenu' / (name + '.uasset')
        shutil.copy2(original, backup / original.name)
    (backup / 'previous_configuration.json').write_text(json.dumps({
        'file_path': previous_path,
        'looping': previous_looping,
        'movie': str(movie),
    }, indent=2) + '\n')
    source.set_file_path(str(movie))
    player.set_looping(True)
    assert source.validate(), 'Unreal could not validate the new media file'
    assert bool(player.get_editor_property('loop')), 'Looping was not enabled'
    for _, asset in changes:
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

saved_path = str(source.get_editor_property('file_path')).replace('\\', '/')
assert saved_path == expected_path, 'Media path must remain portable under Content/Movies'
assert source.validate()
assert bool(player.get_editor_property('loop'))
result = {
    'status': 'configured' if changes else 'verified_saved_configuration',
    'movie': str(movie),
    'file_path': saved_path,
    'source': source.get_path_name(),
    'player': player.get_path_name(),
    'texture': texture.get_path_name(),
    'widget': menu.get_path_name(),
    'looping': bool(player.get_editor_property('loop')),
    'source_valid': bool(source.validate()),
    'changed_assets': [asset.get_path_name() for _, asset in changes],
    'backup': str(backup) if backup else None,
}
out = root / 'Saved/MenuBackgroundSetup'
out.mkdir(parents=True, exist_ok=True)
(out / (result['status'] + '.json')).write_text(json.dumps(result, indent=2) + '\n')
unreal.log('ANIMATED_MENU_BACKGROUND ' + json.dumps(result))
