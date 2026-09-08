"""Migrate standard enemy assets to EnemyDeathComponent. Run through Unreal Python.
Backups are written once under Saved/Backups/EnemyDeathMigration.
"""
from pathlib import Path
import shutil
import unreal

E = unreal.EditorAssetLibrary
M = unreal.MaterialEditingLibrary
root = Path(unreal.Paths.project_content_dir())
backup_root = Path(unreal.Paths.project_saved_dir()) / 'Backups/EnemyDeathMigration'
changed = []

def backup(asset):
    package = asset.get_path_name().split('.')[0]
    rel = Path(package.removeprefix('/Game/') + '.uasset')
    src, dst = root / rel, backup_root / rel
    if src.exists() and not dst.exists():
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)

def save(asset):
    assert E.save_loaded_asset(asset, False), asset.get_path_name()
    changed.append(asset.get_path_name())

materials = {}
for path in E.list_assets('/Game/HeavensDivide/Blueprints/EnemyCharacters', recursive=True):
    if '/BP_' not in path: continue
    bp = unreal.load_asset(path)
    if not isinstance(bp, unreal.Blueprint): continue
    cdo = unreal.get_default_object(bp.generated_class())
    if not isinstance(cdo, unreal.EnemyBase): continue
    if isinstance(cdo, unreal.FinalBossBase):
        assert cdo.get_editor_property('death_montage'), 'Boss montage must be preserved'
        continue
    backup(bp)
    # Include attached weapon component templates as well as the character mesh.
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, bp)
        if not isinstance(obj, unreal.MeshComponent) or isinstance(obj, unreal.WidgetComponent): continue
        for mat in obj.get_materials():
            while isinstance(mat, unreal.MaterialInstance): mat = mat.get_editor_property('parent')
            if mat and mat.get_path_name().startswith('/Game/Assets/EnemyCharacters/'):
                materials[mat.get_path_name()] = mat
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(bp)  # Drops serialized properties removed from the native base.

# Include Ogre weapon even if its Blueprint component template is unavailable.
weapon = unreal.load_asset('/Game/Assets/EnemyCharacters/Ogre/OgreWeaponMat')
materials[weapon.get_path_name()] = weapon
noise = unreal.load_asset('/Game/Assets/VFX/MixedVFX/Textures/T_Noise_05')
assert noise
for material in materials.values():
    backup(material)
    unreal.EnemyDeathMigrationLibrary.remove_legacy_collapse_nodes(material)
    has_dissolve = any(isinstance(e, unreal.MaterialExpressionScalarParameter) and
                       str(e.get_editor_property('parameter_name')) == 'DissolveAmount'
                       for e in M.get_material_expressions(material))
    if not has_dissolve:
        assert not M.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY_MASK), material.get_path_name()
        def node(cls, x, y): return M.create_material_expression(material, cls, x, y)
        sample = node(unreal.MaterialExpressionTextureSample, -1000, 600)
        sample.set_editor_property('texture', noise)
        # Bound noise away from 0 and 1 so both dissolve endpoints are exact.
        scale = node(unreal.MaterialExpressionMultiply, -750, 600)
        scale.set_editor_property('const_b', 0.98)
        assert M.connect_material_expressions(sample, 'R', scale, 'A')
        bias = node(unreal.MaterialExpressionAdd, -550, 600)
        bias.set_editor_property('const_b', 0.01)
        assert M.connect_material_expressions(scale, '', bias, 'A')
        amount = node(unreal.MaterialExpressionScalarParameter, -550, 850)
        amount.set_editor_property('parameter_name', 'DissolveAmount')
        amount.set_editor_property('default_value', 0.0)
        subtract = node(unreal.MaterialExpressionSubtract, -350, 600)
        assert M.connect_material_expressions(bias, '', subtract, 'A')
        assert M.connect_material_expressions(amount, '', subtract, 'B')
        mask = node(unreal.MaterialExpressionAdd, -150, 600)
        mask.set_editor_property('const_b', 0.333)
        assert M.connect_material_expressions(subtract, '', mask, 'A')
        assert M.connect_material_property(mask, '', unreal.MaterialProperty.MP_OPACITY_MASK)
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        material.set_editor_property('opacity_mask_clip_value', 0.333)
    # UE 5.8 can keep the root input's inline constant enabled after a Python
    # graph connection. It takes precedence over Expression during compilation.
    data = material.get_editor_property('editor_only_data')
    unreal.SystemLibrary.execute_console_command(None,
        'setnopec ' + data.get_path_name() + ' OpacityMask (UseConstant=False)')
    assert not M.recompile_material(material)
    save(material)

# The unused standard-enemy montage assets are backed up before removal.
for path in ['/Game/HeavensDivide/Blueprints/EnemyCharacters/Montages/AM_EnemyGruntDeath',
             '/Game/HeavensDivide/Blueprints/EnemyCharacters/Montages/AM_EnemyDevilRangedDeath']:
    if not E.does_asset_exist(path): continue
    refs = E.find_package_referencers_for_asset(path, True)
    assert not refs, 'Unexpected montage references: ' + str(refs)
    backup(unreal.load_asset(path))
    # Delete these files after the editor exits: montage data controllers hold
    # transient references that prevent EditorAssetLibrary.delete_asset in UE 5.8.
    changed.append('SAFE_TO_DELETE_AFTER_EDITOR_EXIT ' + path)
Path(unreal.Paths.project_saved_dir(), 'enemy_death_migration.txt').write_text('\n'.join(changed))
unreal.log('ENEMY_DEATH_MIGRATION_OK ' + str(len(changed)))
