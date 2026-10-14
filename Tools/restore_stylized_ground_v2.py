"""Restore September 27 V2 on Level 1 using retained sources and original blend.

Run in Unreal Editor. The dedicated instance/textures preserve shared materials
used by other maps. APPLY=False verifies the saved selection after loading it.
"""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import re
import runpy
import shutil
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/RestoreStylizedGroundV2'
OUT.mkdir(parents=True, exist_ok=True)
DEST = '/Game/HeavensDivide/Materials/StylizedGroundV2'
PARENT = '/Game/HeavensDivide/Materials/StylizedGround/M_StylizedGround'
MAP = '/Game/Maps/Lvl_B1_Lvl1'
LEVEL = u.get_editor_subsystem(u.LevelEditorSubsystem)
ACTORS = u.get_editor_subsystem(u.EditorActorSubsystem)
LIB = u.MaterialEditingLibrary
ASSETS = u.EditorAssetLibrary
APPLY = globals().get('APPLY', True)
SCALARS = dict(GrassTileSize=1000, DirtTileSize=1000, StoneTileSize=1400,
               PatchSize=1500, GrassCoverage=.56, StoneCoverage=.43,
               BlendSoftness=.075, MacroVariation=.16, Brightness=1,
               Saturation=.9, UsePaint=0, Roughness=.93, Specular=.12)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def snapshot():
    result = runpy.run_path(str(ROOT / 'Tools/inspect_layered_ground.py'),
                           init_globals={'SNAPSHOT_ONLY': True})['REPORT']
    for actor in result.values():
        for component in actor['components'].values():
            if 'materials' in component:
                component['materials'] = [re.sub(r'(\.MID_[^.]+)_\d+$', r'\1', p) if p else None
                                          for p in component['materials']]
    return result


if APPLY:
    backup = OUT / 'Backups' / datetime.now().strftime('%Y%m%d_%H%M%S_%f')
    map_file = ROOT / 'Content/Maps/Lvl_B1_Lvl1.umap'
    target = backup / map_file.relative_to(ROOT)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(map_file, target)
    files = list((ROOT / 'Content/HeavensDivide/Materials').rglob('*.uasset'))
    files += [p for p in (ROOT / 'Content/Maps').glob('*.umap') if p != map_file]
    owned = ROOT / 'Content/HeavensDivide/Materials/StylizedGroundV2'
    protected = {str(p.relative_to(ROOT)): sha(p) for p in files if not p.is_relative_to(owned)}
    for p in owned.glob('*.uasset'):
        target = backup / p.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(p, target)
    assert LEVEL.load_level(MAP)
    assert LEVEL.set_current_level_by_name('Lvl_B1_Lvl1')
    before = snapshot()
    sources = {}
    textures = {}
    for layer in ['Grass', 'Dirt', 'Stone']:
        source = ROOT / 'Art/StylizedGround/Previous' / (layer + '_BaseColor_v2.png')
        assert source.is_file()
        sources[str(source.relative_to(ROOT))] = sha(source)
        task = u.AssetImportTask()
        task.filename = str(source)
        task.destination_path = DEST
        task.destination_name = 'T_V2_' + layer
        task.automated = True
        task.replace_existing = True
        task.save = False
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = u.load_asset(DEST + '/T_V2_' + layer)
        assert texture
        texture.set_editor_property('srgb', True)
        texture.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_WORLD)
        texture.set_editor_property('address_x', u.TextureAddress.TA_MIRROR)
        texture.set_editor_property('address_y', u.TextureAddress.TA_MIRROR)
        texture.set_editor_property('power_of_two_mode', u.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO)
        texture.set_editor_property('max_texture_size', 2048)
        # Same lossless RGB build setting as the original diagnostic textures.
        texture.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_EDITOR_ICON)
        assert ASSETS.save_loaded_asset(texture, False)
        textures[layer + 'Texture'] = texture
    ground = u.load_asset(DEST + '/MI_StylizedGroundV2') if ASSETS.does_asset_exist(DEST + '/MI_StylizedGroundV2') else None
    if not ground:
        ground = u.AssetToolsHelpers.get_asset_tools().create_asset(
            'MI_StylizedGroundV2', DEST, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    assert ground
    LIB.set_material_instance_parent(ground, u.load_asset(PARENT))
    # UE 5.8 setters can return false after a successful write; verify readback.
    for name, value in SCALARS.items():
        LIB.set_material_instance_scalar_parameter_value(ground, name, value)
        assert abs(LIB.get_material_instance_scalar_parameter_value(ground, name) - value) < .0001, name
    for layer in ['Grass', 'Dirt', 'Stone']:
        LIB.set_material_instance_vector_parameter_value(ground, layer + 'Tint', u.LinearColor(1, 1, 1, 1))
    for name, texture in textures.items():
        LIB.set_material_instance_texture_parameter_value(ground, name, texture)
        assert LIB.get_material_instance_texture_parameter_value(ground, name) == texture, name
    LIB.update_material_instance(ground)
    assert ASSETS.save_loaded_asset(ground, False)
    floor = next(a for a in ACTORS.get_all_level_actors() if a.get_actor_label() == 'Plane')
    floor.modify(True)
    floor.static_mesh_component.modify(True)
    floor.static_mesh_component.set_material(0, ground)
    assert snapshot() == before, 'Unrelated scene changed'
    assert LEVEL.save_current_level()
    # Reload the saved map before verifying or rendering it.
    assert LEVEL.load_level(MAP)
else:
    applied = json.loads((OUT / 'Applied.json').read_text())
    before, protected = applied['scene_snapshot'], applied['protected_assets']
    sources, backup = applied['source_hashes'], applied['backup']

floor = next(a for a in ACTORS.get_all_level_actors() if a.get_actor_label() == 'Plane')
ground = floor.static_mesh_component.get_material(0)
assert ground.get_path_name().split('.')[0] == DEST + '/MI_StylizedGroundV2'
assert ground.get_base_material().get_path_name().split('.')[0] == PARENT
for name, value in SCALARS.items():
    assert abs(LIB.get_material_instance_scalar_parameter_value(ground, name) - value) < .0001, name
for layer in ['Grass', 'Dirt', 'Stone']:
    texture = LIB.get_material_instance_texture_parameter_value(ground, layer + 'Texture')
    assert texture.get_path_name().split('.')[0] == DEST + '/T_V2_' + layer
    tint = LIB.get_material_instance_vector_parameter_value(ground, layer + 'Tint')
    assert [tint.r, tint.g, tint.b, tint.a] == [1, 1, 1, 1]
for actor in ACTORS.get_all_level_actors():
    if actor.get_actor_label() in ['GroundRockStamps', 'TerracottaArenaRim']:
        assert all(not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
                   for c in actor.get_components_by_class(u.MeshComponent))
assert snapshot() == before, 'Unrelated scene changed on reload'
assert all(sha(ROOT / p) == h for p, h in protected.items()), 'Shared asset changed'
assert all(sha(ROOT / p) == h for p, h in sources.items()), 'Original source changed'

if APPLY:
    # Repeat the historical unlit 45m diagnostic to verify the chosen V2 look.
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    preview = u.MaterialLibrary.create_dynamic_material_instance(
        world, u.load_asset(PARENT + '_Preview'))
    for name, value in SCALARS.items():
        preview.set_scalar_parameter_value(name, value)
    for layer in ['Grass', 'Dirt', 'Stone']:
        preview.set_vector_parameter_value(layer + 'Tint', u.LinearColor(1, 1, 1, 1))
        preview.set_texture_parameter_value(layer + 'Texture', u.load_asset(DEST + '/T_V2_' + layer))
    u.AutomationLibrary.finish_loading_before_screenshot()
    rt = u.RenderingLibrary.create_render_target2d(world, 1024, 1024, u.TextureRenderTargetFormat.RTF_RGBA8)
    u.RenderingLibrary.draw_material_to_render_target(world, rt, preview)
    pixels = u.RenderingLibrary.read_render_target(world, rt, False)
    assert any(p.g > p.r * 1.1 for p in pixels), 'Diagnostic grass missing'
    u.RenderingLibrary.export_render_target(world, rt, str(OUT), 'RestoredBlend.png')

REPORT = dict(passed=True, material=DEST + '/MI_StylizedGroundV2',
              original_v2_sources=True, original_v2_parameters=SCALARS,
              stone_decals_hidden=True, later_rim_hidden=True,
              unrelated_scene_unchanged=True, protected_assets=protected,
              scene_snapshot=before, source_hashes=sources, backup=str(backup))
if APPLY:
    (OUT / 'Applied.json').write_text(json.dumps(REPORT, indent=2) + '\n')
u.log('STYLIZED_GROUND_V2_RESTORED ' + REPORT['material'])
