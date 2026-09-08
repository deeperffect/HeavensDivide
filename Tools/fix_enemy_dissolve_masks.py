"""Repair UE 5.8 inline opacity overrides left by programmatic graph connections."""
from pathlib import Path
import shutil
import unreal

root = Path(unreal.Paths.project_content_dir()).resolve()
backups = Path(unreal.Paths.project_saved_dir()).resolve() / 'Backups/EnemyDissolveMaskFix'
paths = Path(unreal.Paths.project_saved_dir(), 'enemy_death_migration.txt').read_text().splitlines()
fixed = []
for path in paths:
    if not path.startswith('/Game/Assets/EnemyCharacters/'): continue
    material = unreal.load_asset(path)
    assert material, path
    node = unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY_MASK)
    assert node, path
    rel = Path(path.split('.')[0].removeprefix('/Game/') + '.uasset')
    backup = backups / rel
    if not backup.exists():
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root / rel, backup)
    material.modify()
    data = material.get_editor_property('editor_only_data')
    # UseConstant is protected in Python. The native SETNOPEC command imports only
    # this struct member, preserving Expression, OutputIndex, and channel masks.
    unreal.SystemLibrary.execute_console_command(None,
        'setnopec ' + data.get_path_name() + ' OpacityMask (UseConstant=False)')
    assert unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY_MASK) == node
    assert not unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    fixed.append(path)
assert len(fixed) == 12
unreal.log('ENEMY_DISSOLVE_MASK_FIX_OK: ' + str(len(fixed)))
