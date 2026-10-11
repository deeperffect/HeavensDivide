"""Read back saved enemy UI defaults and the shared widget's gameplay binding."""
import json
from pathlib import Path
import unreal as u

out = Path(u.Paths.project_saved_dir()).resolve() / 'EnemyUIStyle'
before = json.loads((out / 'Before.json').read_text())
applied = json.loads((out / 'Applied.json').read_text())
for expected, original in zip(applied, before['enemies']):
    assert expected['enemy'] == original['path']
    bp = u.load_asset(expected['enemy'])
    cdo = u.get_default_object(bp.generated_class())
    health = cdo.get_editor_property('health_bar_draw_size')
    status = cdo.get_editor_property('status_indicator_draw_size')
    assert [health.x, health.y] == expected['health'], expected['enemy']
    assert status.x == status.y == expected['status'], expected['enemy']
    assert cdo.get_editor_property('status_stack_font_size') == expected['font']
    widget_class = cdo.get_editor_property('health_bar_widget_class')
    assert (widget_class.get_path_name() if widget_class else None) == original['health_bar_widget_class']

folder = '/Game/HeavensDivide/Blueprints/UI/EnemyUI/'
bp = u.load_asset(folder + 'WBP_EnemyHealthBar')
bar = u.find_object(None, bp.get_path_name() + ':WidgetTree.PB_EnemyHealth')
assert bar.get_parent().get_name() == 'HealthFrameOverlay'
style = bar.get_editor_property('widget_style')
assert style.fill_image.resource_object.get_name() == 'T_EnemyHealthFill'
assert not style.enable_fill_animation
export = u.AssetExportTask()
export.object = bp
export.filename = str(out / 'Verified.copy')
export.automated = True
export.prompt = False
assert u.Exporter.run_asset_export_task(export)
assert 'PropertyName="Percent",FunctionName="GetHealthPercent"' in Path(export.filename).read_text(encoding='utf-8-sig')
for name in ['BleedIcon', 'PoisonIcon', 'T_EnemyHealthFrame', 'T_EnemyHealthFill']:
    tex = u.load_asset(folder + name)
    assert tex and tex.get_editor_property('lod_group') == u.TextureGroup.TEXTUREGROUP_UI
    assert tex.get_editor_property('max_texture_size') == 256

(out / 'Verification.json').write_text(json.dumps({'saved_enemy_defaults_verified': len(applied),
    'health_percent_binding_preserved': True, 'shared_textures_verified': 4}, indent=2))
u.log('ENEMY_UI_STYLE_VERIFIED: 24 saved enemy defaults, health binding and four textures.')
