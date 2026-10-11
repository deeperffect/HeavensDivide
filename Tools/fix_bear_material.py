"""Repair vertex-to-pixel transfer in the bear palette; preserve texture and dissolve graphs."""
import json, shutil, hashlib, runpy, unreal as u
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();out=root/'Saved/BearMaterial'
backup=out/'Backups';backup.mkdir(parents=True,exist_ok=True)
path='/Game/Assets/EnemyCharacters/Bear/Enemy_Bear_Mat'
source=root/'Content'/path.removeprefix('/Game/');source=source.with_suffix('.uasset')
if not (backup/source.name).exists():shutil.copy2(source,backup/source.name)
protected=['Content/Assets/EnemyCharacters/Bear/Enemy_Bear.uasset',
 'Content/Assets/EnemyCharacters/Bear/Enemy_Bear_base.uasset',
 'Content/Assets/EnemyCharacters/Bear/Textures/T_Bear_Patchwork_Fabric.uasset',
 'Content/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyBear.uasset']
hashes={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in protected}
mat=u.load_asset(path);lib=u.MaterialEditingLibrary
nodes=lib.get_material_expressions(mat)
position=next(n for n in nodes if isinstance(n,u.MaterialExpressionPreSkinnedPosition))
custom=next(n for n in nodes if isinstance(n,u.MaterialExpressionCustom) and n.get_editor_property('description')=='Bear Patchwork Palette')
code=custom.get_editor_property('code')
mask=lib.get_material_property_input_node(mat,u.MaterialProperty.MP_OPACITY_MASK)
interpolator=next((n for n in nodes if isinstance(n,u.MaterialExpressionVertexInterpolator)),None)
if interpolator is None:interpolator=lib.create_material_expression(mat,u.MaterialExpressionVertexInterpolator,-470,-670)
assert lib.connect_material_expressions(position,'',interpolator,'VS')
assert lib.connect_material_expressions(interpolator,'',custom,'LocalPosition')
assert custom.get_editor_property('code')==code
assert lib.get_material_property_input_node(mat,u.MaterialProperty.MP_OPACITY_MASK)==mask
lib.recompile_material(mat)
assert u.EditorAssetLibrary.save_loaded_asset(mat,False)
assert hashes=={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in protected}
runpy.run_path(str(root/'Tools/verify_enemy_palette.py'))
runpy.run_path(str(root/'Tools/inspect_bear_material.py'))
(out/'Repair.json').write_text(json.dumps({'material':path,'vertex_interpolator_added':True,
 'palette_code_unchanged':True,'dissolve_unchanged':True,'protected_hashes':hashes},indent=2))
u.log('BEAR_MATERIAL_REPAIRED')
