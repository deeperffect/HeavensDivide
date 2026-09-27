"""Reimport the third-pass painted ground textures without changing material tuning."""
from pathlib import Path
import json
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
folder = '/Game/HeavensDivide/Materials/StylizedGround'
backup = root / 'Saved/Backups/StylizedGroundV3'
backup.mkdir(parents=True, exist_ok=True)
out = root / 'Saved/StylizedGround'
out.mkdir(parents=True, exist_ok=True)
old_preview = out / 'blend_preview.png'
if old_preview.exists() and not (out / 'blend_preview_v2.png').exists():
    shutil.copy2(old_preview, out / 'blend_preview_v2.png')

report = []
for layer in ['Grass', 'Dirt', 'Stone']:
    name = 'T_StylizedGround_' + layer
    path = folder + '/' + name
    source = root / 'Content' / (path.removeprefix('/Game/') + '.uasset')
    if not (backup / source.name).exists():
        shutil.copy2(source, backup / source.name)
    texture = u.load_asset(path)
    assert texture
    settings = {key: texture.get_editor_property(key) for key in
                ['srgb', 'lod_group', 'address_x', 'address_y', 'power_of_two_mode', 'max_texture_size', 'compression_settings']}
    task = u.AssetImportTask()
    task.filename = str(root / 'Art/StylizedGround' / (layer + '_BaseColor.png'))
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = False
    task.save = False
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = u.load_asset(path)
    assert texture
    for key, value in settings.items():
        texture.set_editor_property(key, value)
    assert u.EditorAssetLibrary.save_loaded_asset(texture, False)
    report.append({'layer': layer, 'asset': path})

for name in ['M_StylizedGround', 'M_StylizedGround_Landscape', 'M_StylizedGround_Preview']:
    material = u.load_asset(folder + '/' + name)
    assert material
    u.MaterialEditingLibrary.recompile_material(material)
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
target = u.RenderingLibrary.create_render_target2d(world, 1024, 1024, u.TextureRenderTargetFormat.RTF_RGBA8)
u.RenderingLibrary.draw_material_to_render_target(world, target, u.load_asset(folder + '/M_StylizedGround_Preview'))
pixels = u.RenderingLibrary.read_render_target(world, target, False)
assert sum(1 for p in pixels if p.g > p.r * 1.1) > 50000, 'Grass missing from preview'
assert sum(1 for p in pixels if p.r > p.g * 1.05) > 50000, 'Warm ground missing from preview'
u.RenderingLibrary.export_render_target(world, target, str(out), 'blend_preview.png')
u.RenderingLibrary.export_render_target(world, target, str(out), 'blend_preview_v3.png')
(out / 'v3_verification.json').write_text(json.dumps({'updated': report, 'result': 'PASS'}, indent=2))
u.log('STYLIZED_GROUND_V3_PASS ' + json.dumps(report))
