"""Tune the inspected current ground, preserving the user's textures/colors.

Run after review_ground_blending.py has captured Before. Changes only the
assigned instance and a dedicated parent copy; no map or texture saves.
Reapplying uses the original captured scale, so scaling does not accumulate.
"""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/GroundBlending'
BASELINE = json.loads((OUT / 'Before/Validation.json').read_text())['material']
LIB = u.MaterialEditingLibrary
ASSETS = u.EditorAssetLibrary
LEVEL = u.get_editor_subsystem(u.LevelEditorSubsystem)
ACTORS = u.get_editor_subsystem(u.EditorActorSubsystem)
SOURCE_PARENT = BASELINE['parent'].split('.')[0]
PARENT = '/Game/HeavensDivide/Materials/StylizedGround/M_StylizedGround_SoftBlend'
INSTANCE = BASELINE['material'].split('.')[0]
SCALARS = {
    'GrassTileSize': BASELINE['scalar_parameters']['GrassTileSize'] * 3.0,
    'BlendSoftness': max(.14, BASELINE['scalar_parameters']['BlendSoftness']),
    'TextureVariation': 1.0,
    'BlendBreakup': .08,
}


def asset_file(package):
    return ROOT / 'Content' / (package.removeprefix('/Game/') + '.uasset')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


assert LEVEL.load_level('/Game/Maps/Lvl_B1_Lvl1')
floor = next(a for a in ACTORS.get_all_level_actors() if a.get_actor_label() == 'Plane')
instance = floor.static_mesh_component.get_material(0)
assert instance.get_path_name().split('.')[0] == INSTANCE, 'Current assignment differs from inspection'
assert instance.get_base_material().get_path_name().split('.')[0] in (SOURCE_PARENT, PARENT)
assert SOURCE_PARENT == '/Game/HeavensDivide/Materials/StylizedGround/M_StylizedGround'
assert isinstance(instance, u.MaterialInstanceConstant)

# Back up exactly the editable assets; hash maps, all textures, and other ground
# materials so preservation is verifiable even across a fresh editor load.
backup = OUT / 'Backups' / datetime.now().strftime('%Y%m%d_%H%M%S_%f')
for package in (INSTANCE, PARENT):
    path = asset_file(package)
    if path.exists():
        target = backup / path.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
owned = {asset_file(INSTANCE), asset_file(PARENT)}
protected_files = list((ROOT / 'Content/HeavensDivide/Materials').rglob('*.uasset'))
protected_files += list((ROOT / 'Content/Maps').rglob('*.umap'))
protected = {str(p.relative_to(ROOT)): sha(p) for p in protected_files if p not in owned}

material = u.load_asset(PARENT) if ASSETS.does_asset_exist(PARENT) else ASSETS.duplicate_asset(SOURCE_PARENT, PARENT)
assert material
nodes = LIB.get_material_expressions(material)
custom = next(e for e in nodes if isinstance(e, u.MaterialExpressionCustom)
              and 'GrassTexture' in [str(p.get_editor_property('input_name')) for p in e.get_editor_property('inputs')])
pins = list(custom.get_editor_property('inputs'))
pin_names = [str(p.get_editor_property('input_name')) for p in pins]
new_parameters = [('TextureVariation', 1.0, 'Texture Variation', 'Random rotations and offsets of grass and dirt; 0 restores regular sampling.'),
                  ('BlendBreakup', .08, 'Blend', 'Subtle irregularity within soft layer transitions.')]
for index, (name, default, group, description) in enumerate(new_parameters):
    if name not in pin_names:
        pin = u.CustomInput()
        pin.set_editor_property('input_name', name)
        pins.append(pin)
custom.set_editor_property('inputs', pins)
for index, (name, default, group, description) in enumerate(new_parameters):
    parameter = next((e for e in nodes if isinstance(e, u.MaterialExpressionScalarParameter)
                      and str(e.get_editor_property('parameter_name')) == name), None)
    if parameter is None:
        parameter = LIB.create_material_expression(material, u.MaterialExpressionScalarParameter,
                                                   -900, 2100 + 160 * index)
        parameter.set_editor_property('parameter_name', name)
        parameter.set_editor_property('default_value', default)
        parameter.set_editor_property('group', group)
        parameter.set_editor_property('desc', description)
    assert LIB.connect_material_expressions(parameter, '', custom, name)
custom.set_editor_property('code', (ROOT / 'Tools/stylized_ground_blending.hlsl').read_text())
custom.set_editor_property('description', 'Larger grass, varied grass/soil sampling and feathered transitions')
LIB.recompile_material(material)
u.AutomationLibrary.finish_loading_before_screenshot()
assert ASSETS.save_loaded_asset(material, False)

LIB.set_material_instance_parent(instance, material)
for name, value in SCALARS.items():
    LIB.set_material_instance_scalar_parameter_value(instance, name, value)
LIB.update_material_instance(instance)
u.AutomationLibrary.finish_loading_before_screenshot()
for name, before in BASELINE['scalar_parameters'].items():
    actual = LIB.get_material_instance_scalar_parameter_value(instance, name)
    assert abs(actual - SCALARS.get(name, before)) < .0001, name
for name, before in BASELINE['vector_parameters'].items():
    color = LIB.get_material_instance_vector_parameter_value(instance, name)
    assert [color.r, color.g, color.b, color.a] == before, name
for name, before in BASELINE['texture_parameters'].items():
    texture = LIB.get_material_instance_texture_parameter_value(instance, name)
    assert texture.get_path_name().split('.')[0] == before['asset'], name
assert ASSETS.save_loaded_asset(instance, False)
assert all(sha(ROOT / p) == h for p, h in protected.items()), 'Unrelated asset changed'
report = {'passed': True, 'instance': INSTANCE, 'previous_parent': SOURCE_PARENT, 'parent': PARENT,
          'changed_scalars': SCALARS, 'original_scalars': BASELINE['scalar_parameters'],
          'backup': str(backup), 'protected_assets': protected,
          'maps_and_textures_unchanged': True, 'colors_preserved': True,
          'stone_scale_cm': LIB.get_material_instance_scalar_parameter_value(instance, 'StoneTileSize')}
(OUT / 'Applied.json').write_text(json.dumps(report, indent=2))
u.log('GROUND_BLENDING_APPLIED ' + INSTANCE)
