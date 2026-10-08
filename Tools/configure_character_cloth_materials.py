"""Finish the split clothing materials without changing either material-slot selection."""
from pathlib import Path
import shutil
import unreal as u

root=Path(u.Paths.project_dir()).resolve()
backup=root/'Saved/Backups/CharacterClothPass2_20261008/Materials'
paths=['/Game/Assets/PlayerCharacters/Samurai/SamuraiClothes','/Game/Assets/PlayerCharacters/Samurai/SamuraiCharacterV4_Mat','/Game/Assets/PlayerCharacters/Ninja/CharacterNinjaClothesV3_Mat']
assets=[]
for p in paths:
    source=root/'Content'/(p.removeprefix('/Game/')+'.uasset')
    dest=backup/(p.removeprefix('/Game/')+'.uasset')
    if not dest.exists():
        dest.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,dest)
    assets.append(u.load_asset(p))
cloth,body,ninja=assets
assert isinstance(cloth,u.MaterialInstanceConstant)
# The imported SamuraiClothes instance used default textures. Inherit the original
# character's textured shader while preserving the separately selected cloth slot.
u.MaterialEditingLibrary.set_material_instance_parent(cloth,body)
overrides=cloth.get_editor_property('base_property_overrides')
overrides.set_editor_property('override_two_sided',True)
overrides.set_editor_property('two_sided',True)
cloth.set_editor_property('base_property_overrides',overrides)
u.MaterialEditingLibrary.update_material_instance(cloth)
ninja.set_editor_property('two_sided',True)
for mat in [body,ninja]:
    u.MaterialEditingLibrary.set_base_material_usage(mat,u.MaterialUsage.MATUSAGE_CLOTHING,True)
    u.MaterialEditingLibrary.recompile_material(mat)
for asset in assets:
    assert u.EditorAssetLibrary.save_loaded_asset(asset,False),asset.get_path_name()
u.log('CHARACTER_CLOTH_MATERIALS_PASS')
