"""Read the authored menu style and export reference textures without modifying assets."""
import json
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir()).resolve() / 'MenuStyleReferences'
out.mkdir(parents=True, exist_ok=True)
bp = unreal.load_asset('/Game/HeavensDivide/Blueprints/UI/MainMenu/WBP_MainMenu')
cdo = unreal.get_default_object(bp.generated_class())
result = {}
for name in ['collection_panel_texture', 'details_panel_texture', 'ink_brush_texture',
             'horizontal_brush_texture', 'vertical_divider_texture', 'corner_brush_texture',
             'main_menu_logo', 'secondary_heading_color', 'secondary_body_color',
             'secondary_heading_font', 'secondary_body_font', 'menu_button_font']:
    value = cdo.get_editor_property(name)
    result[name] = str(value)
    if isinstance(value, unreal.Texture2D):
        result[name] = value.get_path_name()
        task = unreal.AssetExportTask()
        task.object = value
        task.filename = str(out / (name + '.png'))
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        task.exporter = unreal.TextureExporterPNG()
        assert unreal.Exporter.run_asset_export_task(task), name
    if 'font' in name:
        obj = value.get_editor_property('font_object')
        result[name] = {'object': obj.get_path_name() if obj else None,
                        'size': value.get_editor_property('size'),
                        'typeface': str(value.get_editor_property('typeface_font_name'))}
(out / 'style.json').write_text(json.dumps(result, indent=2))
unreal.log('MENU_STYLE_REFERENCES_EXPORTED ' + str(out))
