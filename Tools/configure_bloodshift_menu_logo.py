"""Import the approved Bloodshift wordmark and assign it to the main menu.

Run with UnrealEditor-Cmd -run=pythonscript, with PythonScriptPlugin and
EditorScriptingUtilities enabled. Existing assets are backed up before saving.
"""

from datetime import datetime
import json
from pathlib import Path
import shutil

import unreal


root = Path(unreal.Paths.project_dir()).resolve()
source = root / 'Art/Steam/Bloodshift_Logo_BlackBacking.png'
asset_root = '/Game/HeavensDivide/Blueprints/UI/MainMenu/'
texture_path = asset_root + 'T_BloodshiftLogo'
widget_path = asset_root + 'WBP_MainMenu'
assert source.is_file(), source
widget = unreal.load_asset(widget_path)
assert widget, widget_path
defaults = unreal.get_default_object(widget.generated_class())


def layout():
    result = {}
    for name in ('main_menu_logo_size', 'main_menu_logo_offset', 'main_menu_buttons_offset'):
        value = defaults.get_editor_property(name)
        result[name] = [value.x, value.y]
    result['main_menu_logo_bottom_spacing'] = defaults.get_editor_property('main_menu_logo_bottom_spacing')
    logo = defaults.get_editor_property('main_menu_logo')
    result['main_menu_logo'] = logo.get_path_name() if logo else None
    return result


before = layout()
backup = root / 'Saved/Backups/MenuLogo' / datetime.now().strftime('%Y%m%d_%H%M%S_%f')
backup.mkdir(parents=True)
for name in ('WBP_MainMenu', 'T_BloodshiftLogo'):
    original = root / 'Content/HeavensDivide/Blueprints/UI/MainMenu' / (name + '.uasset')
    if original.is_file():
        shutil.copy2(original, backup / original.name)
(backup / 'before.json').write_text(json.dumps(before, indent=2) + '\n')

task = unreal.AssetImportTask()
task.filename = str(source)
task.destination_path = asset_root.rstrip('/')
task.destination_name = 'T_BloodshiftLogo'
task.automated = True
task.replace_existing = True
task.save = False
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
assert task.imported_object_paths, 'Bloodshift logo import failed'
texture = unreal.load_asset(texture_path)
assert isinstance(texture, unreal.Texture2D), texture_path
for name, value in {
    'srgb': True,
    'lod_group': unreal.TextureGroup.TEXTUREGROUP_UI,
    'compression_settings': unreal.TextureCompressionSettings.TC_EDITOR_ICON,
    'mip_gen_settings': unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
    'address_x': unreal.TextureAddress.TA_CLAMP,
    'address_y': unreal.TextureAddress.TA_CLAMP,
    'never_stream': True,
    'max_texture_size': 0,
}.items():
    texture.set_editor_property(name, value)
assert unreal.EditorAssetLibrary.save_loaded_asset(texture, False)

# Preserve the wordmark's native aspect ratio and keep the first button fixed.
# The enclosing vertical stack is centered, so a logo height change moves its
# following buttons by half that amount before the buttons' render offset.
width = 560.0
height = width * texture.blueprint_get_size_y() / texture.blueprint_get_size_x()
old_height = before['main_menu_logo_size'][1]
button_x, button_y = before['main_menu_buttons_offset']
defaults.set_editor_property('main_menu_logo', texture)
defaults.set_editor_property('main_menu_logo_size', unreal.Vector2D(width, height))
defaults.set_editor_property('main_menu_logo_offset', unreal.Vector2D(-55.0, -200.0))
defaults.set_editor_property('main_menu_buttons_offset',
                             unreal.Vector2D(button_x, button_y - (height - old_height) * 0.5))
unreal.BlueprintEditorLibrary.compile_blueprint(widget)
assert unreal.EditorAssetLibrary.save_loaded_asset(widget, False)
defaults = unreal.get_default_object(widget.generated_class())
assert defaults.get_editor_property('main_menu_logo') == texture
after = layout()
assert abs((after['main_menu_logo_size'][1] - old_height) * 0.5
           + after['main_menu_buttons_offset'][1] - button_y) < 0.001
assert after['main_menu_logo_bottom_spacing'] == before['main_menu_logo_bottom_spacing']

report = {'source': str(source.relative_to(root)), 'texture': texture.get_path_name(),
          'widget': widget_path, 'before': before, 'after': after,
          'backup': str(backup.relative_to(root))}
output = root / 'Saved/MenuLogoSetup'
output.mkdir(parents=True, exist_ok=True)
(output / 'configured.json').write_text(json.dumps(report, indent=2) + '\n')
unreal.log('BLOODSHIFT_MENU_LOGO_CONFIGURED ' + json.dumps(report))
