"""Apply Bloodshift's page typography and spacing to the saved main-menu widget."""
from datetime import datetime
import json
from pathlib import Path
import shutil
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
path = '/Game/HeavensDivide/Blueprints/UI/MainMenu/WBP_MainMenu'
widget = unreal.load_asset(path)
assert widget
defaults = unreal.get_default_object(widget.generated_class())
backup = root / 'Saved/Backups/MenuWindows' / datetime.now().strftime('%Y%m%d_%H%M%S_%f')
backup.mkdir(parents=True)
shutil.copy2(root / 'Content/HeavensDivide/Blueprints/UI/MainMenu/WBP_MainMenu.uasset', backup / 'WBP_MainMenu.uasset')

body = defaults.get_editor_property('secondary_body_font')
body.set_editor_property('font_object', unreal.load_asset('/Engine/EngineFonts/Roboto'))
body.set_editor_property('typeface_font_name', 'Regular')
body.set_editor_property('size', 20)
heading = defaults.get_editor_property('secondary_heading_font')
heading.set_editor_property('size', 32)
settings = {
    'secondary_body_font': body,
    'secondary_heading_font': heading,
    'secondary_body_color': unreal.LinearColor(.94, .86, .69, 1),
    'secondary_heading_color': unreal.LinearColor(.94, .86, .69, 1),
    'reset_popup_title_color': unreal.LinearColor(.95, .40, .27, 1),
    'collection_content_padding': unreal.Margin(44, 40, 44, 32),
    'settings_popup_padding': unreal.Margin(44, 40, 44, 32),
    'reset_popup_padding': unreal.Margin(44, 40, 44, 32),
    'collection_description_font_size': 20,
    'collection_details_artwork_size': unreal.Vector2D(196, 196),
    'collection_page_offset': unreal.Vector2D(0, 0),
    'settings_page_offset': unreal.Vector2D(0, 0),
    'reset_popup_offset': unreal.Vector2D(0, 0),
}
for name, value in settings.items():
    defaults.set_editor_property(name, value)
unreal.BlueprintEditorLibrary.compile_blueprint(widget)
assert unreal.EditorAssetLibrary.save_loaded_asset(widget, False)
report = {'widget': path, 'backup': str(backup.relative_to(root)),
          'properties': {key: str(value) for key, value in settings.items()}}
out = root / 'Saved/MenuRedesign'
out.mkdir(parents=True, exist_ok=True)
(out / 'configured.json').write_text(json.dumps(report, indent=2) + '\n')
unreal.log('BLOODSHIFT_WINDOWS_CONFIGURED')
