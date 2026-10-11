"""Apply the Bloodshift enemy health/status treatment, retaining all gameplay bindings."""
from pathlib import Path
import json
import re
import shutil
import uuid
import unreal as u

project = Path(u.Paths.project_dir()).resolve()
folder = '/Game/HeavensDivide/Blueprints/UI/EnemyUI'
backup_root = project / 'Saved/Backups/EnemyUIStyle'
before = json.loads((project / 'Saved/EnemyUIStyle/Before.json').read_text())

def backup(path):
    relative = path.split('.')[0].removeprefix('/Game/') + '.uasset'
    src, dst = project / 'Content' / relative, backup_root / relative
    if src.exists() and not dst.exists():
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)

for name in ['WBP_EnemyHealthBar', 'BleedIcon', 'PoisonIcon']:
    backup(folder + '/' + name)
for row in before['enemies']:
    backup(row['path'])

textures = {}
for name, source in [('BleedIcon', 'BleedIcon_v2.png'), ('PoisonIcon', 'PoisonIcon_v2.png'),
                     ('T_EnemyHealthFrame', 'HealthFrame.png'), ('T_EnemyHealthFill', 'HealthFill.png')]:
    task = u.AssetImportTask()
    task.filename = str(project / 'Art/EnemyUI' / source)
    task.destination_path = folder
    task.destination_name = name
    task.automated = task.replace_existing = task.save = True
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    assert task.imported_object_paths, name
    tex = u.load_asset(folder + '/' + name)
    tex.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property('mip_gen_settings', u.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('srgb', True)
    tex.set_editor_property('max_texture_size', 256)
    tex.set_editor_property('never_stream', True)
    assert u.EditorAssetLibrary.save_loaded_asset(tex, False)
    textures[name] = tex

def brush(texture, width, height, box=False):
    b = u.WidgetLibrary.make_brush_from_texture(texture, width, height)
    b.set_editor_property('draw_as', u.SlateBrushDrawType.BOX if box else u.SlateBrushDrawType.IMAGE)
    b.set_editor_property('tint_color', u.SlateColor(specified_color=u.LinearColor(1,1,1,1)))
    if box: b.set_editor_property('margin', u.Margin(8/128, 5/16, 8/128, 5/16))
    return b

bp = u.load_asset(folder + '/WBP_EnemyHealthBar')
tree = u.find_object(None, bp.get_path_name() + ':WidgetTree')
bar = u.find_object(None, tree.get_path_name() + '.PB_EnemyHealth')
assert bar
root = u.find_object(None, tree.get_path_name() + '.HealthFrameOverlay')
if not root:
    root = u.new_object(u.Overlay, outer=tree, name='HealthFrameOverlay')
    frame = u.new_object(u.Image, outer=tree, name='HealthFrame')
    root.add_child_to_overlay(frame)
    root.add_child_to_overlay(bar)
    assert u.SwapVFXSetupLibrary.set_property_text(tree, 'RootWidget', root.get_path_name())
else:
    frame = u.find_object(None, tree.get_path_name() + '.HealthFrame')

frame.set_brush(brush(textures['T_EnemyHealthFrame'], 128, 16, True))
for widget in [frame, bar]:
    widget.slot.set_horizontal_alignment(u.HorizontalAlignment.H_ALIGN_FILL)
    widget.slot.set_vertical_alignment(u.VerticalAlignment.V_ALIGN_FILL)
    widget.set_visibility(u.SlateVisibility.HIT_TEST_INVISIBLE)
bar.slot.set_padding(u.Margin(8, 5, 8, 5))
style = bar.get_editor_property('widget_style')
empty = u.SlateBrush()
empty.set_editor_property('draw_as', u.SlateBrushDrawType.NO_DRAW_TYPE)
style.set_editor_property('background_image', empty)
style.set_editor_property('fill_image', brush(textures['T_EnemyHealthFill'], 128, 8))
style.set_editor_property('enable_fill_animation', False)
bar.set_editor_property('widget_style', style)
bar.set_fill_color_and_opacity(u.LinearColor(1,1,1,1))
bar.set_editor_property('bar_fill_type', u.ProgressBarFillType.LEFT_TO_RIGHT)
bar.set_editor_property('bar_fill_style', u.ProgressBarFillStyle.MASK)
bar.set_editor_property('border_padding', u.Vector2D(0,0))

export = u.AssetExportTask()
export.object = bp
export.filename = str(project / 'Saved/EnemyUIStyle/Styled.copy')
export.automated = True
export.prompt = False
assert u.Exporter.run_asset_export_task(export)
source_text = Path(export.filename).read_text(encoding='utf-8-sig')
guid_map = re.search(r'^\s*WidgetVariableNameToGuidMap=(.+)$', source_text, re.M).group(1).strip()
for name in ['HealthFrameOverlay', 'HealthFrame']:
    if '"' + name + '"' not in guid_map:
        guid_map = guid_map[:-1] + ',("%s", %s))' % (name, uuid.uuid5(uuid.NAMESPACE_URL, bp.get_path_name() + name).hex.upper())
assert u.SwapVFXSetupLibrary.set_property_text(bp, 'WidgetVariableNameToGuidMap', guid_map)
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert u.EditorAssetLibrary.save_loaded_asset(bp, False)

after = []
for row in before['enemies']:
    enemy_bp = u.load_asset(row['path'])
    cdo = u.get_default_object(enemy_bp.generated_class())
    small = row['health_bar_draw_size'][0] < 80
    health_size = u.Vector2D(60,16) if small else u.Vector2D(120,18)
    status_size = 24 if row['status_indicator_draw_size'][0] < 30 else 42
    cdo.set_editor_property('health_bar_draw_size', health_size)
    cdo.set_editor_property('status_indicator_draw_size', u.Vector2D(status_size,status_size))
    cdo.set_editor_property('status_indicator_spacing', 2 if status_size == 24 else 4)
    cdo.set_editor_property('status_indicator_health_bar_gap', 3)
    cdo.set_editor_property('status_stack_font_size', 9 if status_size == 24 else 10)
    assert u.EditorAssetLibrary.save_loaded_asset(enemy_bp, False)
    after.append({'enemy': row['path'], 'health': [health_size.x,health_size.y],
                  'status': status_size, 'font': cdo.get_editor_property('status_stack_font_size')})
(project / 'Saved/EnemyUIStyle/Applied.json').write_text(json.dumps(after, indent=2))
u.log('ENEMY_UI_STYLE_APPLIED: %d enemy defaults; health binding retained.' % len(after))
