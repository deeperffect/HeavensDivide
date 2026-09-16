"""Restyle existing dash/swap widgets without replacing their gameplay bindings."""
from pathlib import Path
import shutil
import re
import uuid
import unreal as u

project = Path(u.Paths.project_dir()).resolve()
folder = '/Game/HeavensDivide/Blueprints/UI'
paths = [folder + '/DashUI/WBP_DashCharge', folder + '/Swap/WBP_SwapCooldown', folder + '/WBP_PlayerHUD']
for path in paths:
    source = project / 'Content' / (path.removeprefix('/Game/') + '.uasset')
    backup = project / 'Saved/Backups/ActionHUD' / source.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists(): shutil.copy2(source, backup)

task = u.AssetImportTask()
task.filename = str(project / 'Art/ActionHUD/SwapInkArrows.png')
task.destination_path = folder + '/Swap'
task.destination_name = 'T_SwapInkArrows'
task.automated = task.replace_existing = task.save = True
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
assert task.imported_object_paths
icon = u.load_asset(task.imported_object_paths[0])
icon.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_UI)
icon.set_editor_property('mip_gen_settings', u.TextureMipGenSettings.TMGS_NO_MIPMAPS)
icon.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_EDITOR_ICON)
u.EditorAssetLibrary.save_loaded_asset(icon)
ink = u.load_asset(folder + '/Combo/T_ComboInkSlash')
font = u.load_asset('/Game/Assets/Fonts/Cinzel-Medium_Font')
ivory = u.LinearColor(.83, .78, .65, 1)
gold = u.LinearColor(1, .65, .1, 1)
dim = u.LinearColor(.19, .17, .13, .9)

def widget(bp, name):
    obj = u.find_object(None, bp.get_path_name() + ':WidgetTree.' + name)
    assert obj, name
    return obj

def brush(texture, color):
    b = u.SlateBrush()
    b.set_editor_property('resource_object', texture)
    b.set_editor_property('draw_as', u.SlateBrushDrawType.IMAGE)
    # Layout slots control size; stretch the full texture without tiling.
    b.set_editor_property('tint_color', u.SlateColor(specified_color=color))
    return b

dash, swap, hud = [u.load_asset(p) for p in paths]
empty = widget(dash, 'ChargeEmpty')
empty.set_brush(brush(ink, dim))
bar = widget(dash, 'ChargeFull')
style = bar.get_editor_property('widget_style')
style.set_editor_property('fill_image', brush(ink, u.LinearColor(1, 1, 1, 1)))
style.set_editor_property('enable_fill_animation', False)
bar.set_editor_property('widget_style', style)
bar.set_fill_color_and_opacity(ivory)
bar.set_editor_property('bar_fill_type', u.ProgressBarFillType.LEFT_TO_RIGHT)
bar.set_editor_property('bar_fill_style', u.ProgressBarFillStyle.MASK)
bar.set_editor_property('border_padding', u.Vector2D(0, 0))
widget(dash, 'SizeBox_0').set_width_override(60)
widget(dash, 'SizeBox_0').set_height_override(76)
widget(swap, 'IMG_SwapIcon').set_brush(brush(icon, u.LinearColor(1, 1, 1, 1)))
widget(swap, 'IMG_SwapIcon').set_color_and_opacity(gold)
text = widget(swap, 'TXT_Cooldown')
text.set_font(u.SlateFontInfo(font_object=font, size=16))
text.set_color_and_opacity(u.SlateColor(specified_color=u.LinearColor(.94, .90, .81, 1)))
text.set_shadow_color_and_opacity(u.LinearColor(0, 0, 0, 1))
text.set_shadow_offset(u.Vector2D(1, 2))

# Match the native combo meter's bottom-center row, allowing extra dash charges
# to grow leftwards without colliding with the combo bar.
row = widget(hud, 'DashChargeContainer').slot
row.set_anchors(u.Anchors(minimum=u.Vector2D(.5, 1), maximum=u.Vector2D(.5, 1)))
row.set_alignment(u.Vector2D(1, 0))
row.set_position(u.Vector2D(-216, -236))
row.set_auto_size(True)
slot = widget(hud, 'SwapCooldownWidget').slot
slot.set_anchors(u.Anchors(minimum=u.Vector2D(.5, 1), maximum=u.Vector2D(.5, 1)))
slot.set_alignment(u.Vector2D(.5, 0))
slot.set_position(u.Vector2D(260, -223))
slot.set_size(u.Vector2D(56, 56))

tree = u.find_object(None, hud.get_path_name() + ':WidgetTree')
canvas = widget(hud, 'CanvasPanel_40')
for name, caption, x, width in [('TXT_DashAction', 'Dash', -416, 200), ('TXT_SwapAction', 'Swap', 190, 140)]:
    label = u.find_object(None, tree.get_path_name() + '.' + name)
    if not label:
        label = u.new_object(u.TextBlock, outer=tree, name=name)
        canvas.add_child_to_canvas(label)
    label.set_text(caption)
    label.set_font(u.SlateFontInfo(font_object=font, size=14))
    label.set_color_and_opacity(u.SlateColor(specified_color=u.LinearColor(.9, .87, .8, 1)))
    label.set_editor_property('justification', u.TextJustify.CENTER)
    label.set_shadow_offset(u.Vector2D(1, 2))
    label.set_shadow_color_and_opacity(u.LinearColor(0, 0, 0, 1))
    label.slot.set_anchors(u.Anchors(minimum=u.Vector2D(.5, 1), maximum=u.Vector2D(.5, 1)))
    label.slot.set_position(u.Vector2D(x, -252))
    label.slot.set_size(u.Vector2D(width, 28))
    label.slot.set_z_order(50)

# New designer widgets need persistent GUIDs before Blueprint compilation.
export = u.AssetExportTask()
export.object = hud
export.filename = str(project / 'Saved/Backups/ActionHUD/HUD_before_compile.copy')
export.automated = True
export.prompt = False
assert u.Exporter.run_asset_export_task(export)
source_text = Path(export.filename).read_text(encoding='utf-8-sig')
guid_map = re.search(r'^\s*WidgetVariableNameToGuidMap=(.+)$', source_text, re.M).group(1).strip()
for name in ['TXT_DashAction', 'TXT_SwapAction']:
    if '"' + name + '"' not in guid_map:
        guid_map = guid_map[:-1] + ',("%s", %s))' % (name, uuid.uuid5(uuid.NAMESPACE_URL, hud.get_path_name() + name).hex.upper())
assert u.SwapVFXSetupLibrary.set_property_text(hud, 'WidgetVariableNameToGuidMap', guid_map)

for bp in [dash, swap, hud]:
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
u.log('ACTION_HUD_STYLED')






