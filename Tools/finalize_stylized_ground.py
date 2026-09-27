"""Apply the first version's broad texture scale and export its display-gamma preview."""
from pathlib import Path
import unreal as u

folder = '/Game/HeavensDivide/Materials/StylizedGround/'
lib = u.MaterialEditingLibrary
for name in ['M_StylizedGround', 'M_StylizedGround_Landscape', 'M_StylizedGround_Preview']:
    material = u.load_asset(folder + name)
    assert material
    for expr in lib.get_material_expressions(material):
        if isinstance(expr, u.MaterialExpressionScalarParameter):
            key = str(expr.get_editor_property('parameter_name'))
            if key in ['GrassTileSize', 'DirtTileSize', 'StoneTileSize']:
                expr.set_editor_property('default_value', 1400 if key == 'StoneTileSize' else 1000)
        elif name.endswith('Preview') and isinstance(expr, u.MaterialExpressionCustom):
            code = expr.get_editor_property('code')
            expr.set_editor_property('code', code.replace(
                'return max(0.0, lerp(grey.xxx, rgb, Saturation) * Brightness * variation);',
                'return pow(max(0.0, lerp(grey.xxx, rgb, Saturation) * Brightness * variation), 1.0/2.2);'))
    lib.recompile_material(material)
    assert u.EditorAssetLibrary.save_loaded_asset(material, False)
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
rt = u.RenderingLibrary.create_render_target2d(world, 1024, 1024, u.TextureRenderTargetFormat.RTF_RGBA8)
u.RenderingLibrary.draw_material_to_render_target(world, rt, u.load_asset(folder + 'M_StylizedGround_Preview'))
pixels = u.RenderingLibrary.read_render_target(world, rt, False)
assert sum(1 for p in pixels if p.g > p.r * 1.1) > 50000
out = Path(u.Paths.project_saved_dir(), 'StylizedGround').resolve()
u.RenderingLibrary.export_render_target(world, rt, str(out), 'blend_preview.png')
u.log('STYLIZED_GROUND_FINAL_PASS')
