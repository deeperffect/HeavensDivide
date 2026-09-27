"""Render the saved lane indicator at empty/half/full fill; requires commandlet rendering."""
from pathlib import Path
import json
import unreal as u

world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
cls = u.EditorAssetLibrary.load_blueprint_class('/Game/HeavensDivide/Blueprints/Objectives/BP_SamuraiTechniqueTrial')
defaults = u.get_default_object(cls)
tick = defaults.get_editor_property('primary_actor_tick')
assert tick.get_editor_property('start_with_tick_enabled')
assert not defaults.get_editor_property('draw_strike_damage_debug_boxes'), 'Strike debug rectangles should be opt-in'
material = defaults.get_editor_property('lane_indicator_material')
assert material, 'Saved Blueprint must have a lane material'
parent = u.load_asset('/Game/HeavensDivide/Materials/M_SamuraiLaneIndicator')
assert material.get_editor_property('parent') == parent
u.MaterialEditingLibrary.recompile_material(parent)
mid = u.MaterialLibrary.create_dynamic_material_instance(world, material)
mid.set_vector_parameter_value('FillColor', u.LinearColor(1, .02, .01, 1))
target = u.RenderingLibrary.create_render_target2d(world, 240, 704, u.TextureRenderTargetFormat.RTF_RGBA8)
output = Path(u.Paths.project_saved_dir(), 'SamuraiLaneIndicator').resolve()
output.mkdir(parents=True, exist_ok=True)
counts = []
for name, amount in [('empty', 0.0), ('half', 0.5), ('full', 1.0)]:
    mid.set_scalar_parameter_value('FillAmount', amount)
    u.RenderingLibrary.clear_render_target2d(world, target, u.LinearColor(.08, .10, .14, 1))
    u.RenderingLibrary.draw_material_to_render_target(world, target, mid)
    pixels = u.RenderingLibrary.read_render_target(world, target, False)
    counts.append(sum(1 for p in pixels if p.r > 70 and p.r > p.g * 2 and p.r > p.b * 2))
    u.RenderingLibrary.export_render_target(world, target, str(output), name + '.png')
assert counts[0] > 100, counts
assert counts[1] > counts[0] + 30000, counts
assert counts[2] > counts[1] + 30000, counts
color_counts = {}
for name, tint in [('green', u.LinearColor(.01, 1, .01, 1)), ('blue', u.LinearColor(.01, .01, 1, 1))]:
    mid.set_vector_parameter_value('FillColor', tint)
    mid.set_scalar_parameter_value('FillAmount', 1)
    u.RenderingLibrary.clear_render_target2d(world, target, u.LinearColor(.08, .10, .14, 1))
    u.RenderingLibrary.draw_material_to_render_target(world, target, mid)
    pixels = u.RenderingLibrary.read_render_target(world, target, False)
    channel = 'g' if name == 'green' else 'b'
    color_counts[name] = sum(1 for p in pixels if getattr(p, channel) > 70 and getattr(p, channel) > p.r * 2)
    assert color_counts[name] > 100000, color_counts
    u.RenderingLibrary.export_render_target(world, target, str(output), 'fill_' + name + '.png')
(output / 'verification.json').write_text(json.dumps({'red_pixel_counts': counts, 'fill_color_pixels': color_counts, 'result': 'PASS'}, indent=2))
u.log('SAMURAI_LANE_RENDER_PASS ' + str(counts))
