"""Render the saved ground blend; use -AllowCommandletRendering -NoTextureStreaming."""
from pathlib import Path
import unreal as u

folder = '/Game/HeavensDivide/Materials/StylizedGround/'
for layer in ['Grass', 'Dirt', 'Stone']:
    texture = u.load_asset(folder + 'T_StylizedGround_' + layer)
    assert texture
    texture.set_force_mip_levels_to_be_resident(30)
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
material = u.load_asset(folder + 'M_StylizedGround_Preview')
u.MaterialEditingLibrary.recompile_material(material)
target = u.RenderingLibrary.create_render_target2d(world, 1024, 1024, u.TextureRenderTargetFormat.RTF_RGBA8)
u.RenderingLibrary.draw_material_to_render_target(world, target, material)
pixels = u.RenderingLibrary.read_render_target(world, target, False)
assert sum(1 for p in pixels if p.g > p.r * 1.1) > 50000
out = Path(u.Paths.project_saved_dir(), 'StylizedGround').resolve()
for name in ['blend_preview.png', 'blend_preview_v3.png']:
    u.RenderingLibrary.export_render_target(world, target, str(out), name)
u.log('STYLIZED_GROUND_RENDER_PASS')
