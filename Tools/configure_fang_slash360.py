"""Make a full-circle Slash 15 variant and assign only Fang's return burst."""
from pathlib import Path
from datetime import datetime
import hashlib
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
folder = '/Game/HeavensDivide/VFX/Stances/'
vendor = '/Game/Assets/VFX/SlashesV1/'
bp_path = '/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja'
backup = root / 'Saved/Backups/FangSlash360' / datetime.now().strftime('%Y%m%d_%H%M%S_%f')
def disk(path): return root / 'Content' / (path.split('.')[0].removeprefix('/Game/') + '.uasset')
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
protected = {p: digest(p) for d in ['Assets/VFX/SlashesV1', 'HeavensDivide/Upgrades', 'HeavensDivide/Blueprints/PlayerCharacters/Montages'] for p in (root/'Content'/d).rglob('*.uasset')}
def back_up(path):
    src = disk(path)
    if src.exists():
        dest = backup / src.relative_to(root);dest.parent.mkdir(parents=True, exist_ok=True);shutil.copy2(src, dest)
def duplicate(source, target):
    back_up(target)
    return u.load_asset(target) if u.EditorAssetLibrary.does_asset_exist(target) else u.EditorAssetLibrary.duplicate_asset(source, target)
lib = u.MaterialEditingLibrary
for old, kind, coordinate_node in [('MI_MasterStrip_ADD5', 'Add', 'MaterialExpressionMultiply_6'), ('MI_MasterStrip_AB', 'Alpha', 'MaterialExpressionMultiply_8')]:
    source = u.load_asset(vendor + 'Materials/' + old)
    assert source
    parent = duplicate(source.get_editor_property('parent').get_path_name(), folder + 'M_Fang360_' + kind)
    assert parent
    # SM_Slash is already a closed ring. U is angle, V is the radial profile.
    # Sample the middle of the angular mask/beam, retaining V and the original
    # noise, dissolve, particle color and lifetime animation everywhere else.
    main = u.find_object(parent, 'MaterialExpressionTextureSampleParameter2D_0')
    mask = u.find_object(parent, 'MaterialExpressionTextureSampleParameter2D_3')
    old_coordinates = u.find_object(parent, coordinate_node)
    assert main and mask and old_coordinates
    uv = lib.create_material_expression(parent, u.MaterialExpressionTextureCoordinate, -1200, 900)
    midpoint = lib.create_material_expression(parent, u.MaterialExpressionConstant, -1200, 1100)
    midpoint.set_editor_property('r', .5)
    for target, coordinates in [(main, old_coordinates), (mask, uv)]:
        radial = lib.create_material_expression(parent, u.MaterialExpressionComponentMask, -1000, 900)
        radial.set_editor_property('r', False);radial.set_editor_property('g', True)
        radial.set_editor_property('b', False);radial.set_editor_property('a', False)
        joined = lib.create_material_expression(parent, u.MaterialExpressionAppendVector, -800, 900)
        assert lib.connect_material_expressions(coordinates, '', radial, '')
        assert lib.connect_material_expressions(midpoint, '', joined, 'A')
        assert lib.connect_material_expressions(radial, '', joined, 'B')
        assert lib.connect_material_expressions(joined, '', target, '')
    lib.recompile_material(parent)
    assert u.EditorAssetLibrary.save_loaded_asset(parent, False)
    mi = duplicate(source.get_path_name(), folder + 'MI_Fang360_' + old)
    lib.set_material_instance_parent(mi, parent)
    lib.update_material_instance(mi)
    assert u.EditorAssetLibrary.save_loaded_asset(mi, False)

system = duplicate(vendor + 'Particles/NiagaraSystems/NS_Slash_15', folder + 'NS_FangSlash_360')
assert system and u.SwapVFXSetupLibrary.configure_fang_circle_slash(system)
assert u.EditorAssetLibrary.save_loaded_asset(system, False)
bp = u.load_asset(bp_path);back_up(bp_path)
build = u.get_default_object(bp.generated_class()).get_component_by_class(u.NinjaBuildComponent)
preserved = {k: build.get_editor_property(k) for k in ['shuriken_burst_vfx', 'toxic_ground_vfx', 'venom_bloom_vfx', 'fang_projectile_class']}
build.set_editor_property('fang_return_burst_vfx', system)
u.BlueprintEditorLibrary.compile_blueprint(bp)
build = u.get_default_object(bp.generated_class()).get_component_by_class(u.NinjaBuildComponent)
assert build.get_editor_property('fang_return_burst_vfx') == system
assert all(build.get_editor_property(k) == v for k, v in preserved.items())
assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
assert all(digest(p) == h for p, h in protected.items()), 'Source VFX, montage or upgrade tuning changed'
u.log('FANG_SLASH360_OK: original Slash 15 preserved; full-circle material variant assigned; backup=' + str(backup))


