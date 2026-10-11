"""Apply the generated patchwork look while preserving the bear's UV detail and dissolve."""
import unreal as u,json,shutil,hashlib
from pathlib import Path
ROOT=Path(u.Paths.project_dir()).resolve();ART=ROOT/'Art/Enemies/Bear/Patchwork';OUT=ROOT/'Saved/BearPatchwork';BACKUP=OUT/'Backup';BACKUP.mkdir(exist_ok=True)
M=u.MaterialEditingLibrary;E=u.EditorAssetLibrary
MAT='/Game/Assets/EnemyCharacters/Bear/Enemy_Bear_Mat';TEX='/Game/Assets/EnemyCharacters/Bear/Textures/T_Bear_Patchwork_Fabric'
matfile=ROOT/'Content'/Path(MAT.removeprefix('/Game/')+'.uasset');backup=BACKUP/'Enemy_Bear_Mat.uasset'
if not backup.exists():shutil.copy2(matfile,backup)
protected=['Content/Assets/EnemyCharacters/Bear/Enemy_Bear.uasset','Content/Assets/EnemyCharacters/Bear/Enemy_Bear_base.uasset','Content/Assets/EnemyCharacters/Bear/Enemy_Bear_Skeleton.uasset','Content/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyBear.uasset']
hashes={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in protected}
t=u.AssetImportTask();t.filename=str(ART/'T_Bear_Patchwork_Refined_2K.png');t.destination_path=TEX.rsplit('/',1)[0];t.destination_name=TEX.rsplit('/',1)[1];t.automated=True;t.replace_existing=True;t.save=True;u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
texture=u.load_asset(TEX);assert texture;texture.set_editor_property('srgb',True);texture.set_editor_property('max_texture_size',2048);texture.set_editor_property('lod_group',u.TextureGroup.TEXTUREGROUP_CHARACTER);assert E.save_loaded_asset(texture,False)
mat=u.load_asset(MAT);nodes=M.get_material_expressions(mat)
source=next(n for n in nodes if isinstance(n,u.MaterialExpressionTextureSample) and n.get_editor_property('texture') and n.get_editor_property('texture').get_path_name()=='/Game/Assets/EnemyCharacters/Bear/Enemy_Bear_base.Enemy_Bear_base')
custom=next((n for n in nodes if isinstance(n,u.MaterialExpressionCustom) and n.get_editor_property('description')=='Bear Patchwork Palette'),None)
mask=M.get_material_property_input_node(mat,u.MaterialProperty.MP_OPACITY_MASK)
if custom is None:
 fabric=M.create_material_expression(mat,u.MaterialExpressionTextureSample,-750,-450);fabric.set_editor_property('texture',texture)
 position=M.create_material_expression(mat,u.MaterialExpressionPreSkinnedPosition,-750,-670)
 custom=M.create_material_expression(mat,u.MaterialExpressionCustom,-100,-220);custom.set_editor_property('description','Bear Patchwork Palette');custom.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT3)
 inputs=[]
 for name in ['SourceColor','FabricColor','LocalPosition']:
  item=u.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
 custom.set_editor_property('inputs',inputs)
 assert M.connect_material_expressions(source,'RGB',custom,'SourceColor');assert M.connect_material_expressions(fabric,'RGB',custom,'FabricColor')
nodes=M.get_material_expressions(mat)
position=next(n for n in nodes if isinstance(n,u.MaterialExpressionPreSkinnedPosition))
interpolator=next((n for n in nodes if isinstance(n,u.MaterialExpressionVertexInterpolator)),None)
if interpolator is None:interpolator=M.create_material_expression(mat,u.MaterialExpressionVertexInterpolator,-470,-670)
# PreSkinnedPosition is vertex-only in UE 5.8; interpolate it before pixel color evaluation.
assert M.connect_material_expressions(position,'',interpolator,'VS')
assert M.connect_material_expressions(interpolator,'',custom,'LocalPosition')
custom.set_editor_property('code',(ART/'Bear_Patchwork_Palette.hlsl').read_text())
assert M.connect_material_property(custom,'',u.MaterialProperty.MP_BASE_COLOR)
assert M.get_material_property_input_node(mat,u.MaterialProperty.MP_OPACITY_MASK)==mask
assert any(isinstance(n,u.MaterialExpressionScalarParameter) and str(n.get_editor_property('parameter_name'))=='DissolveAmount' for n in M.get_material_expressions(mat))
M.recompile_material(mat);assert E.save_loaded_asset(mat,False)
bp=u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyBear');comp=u.get_default_object(bp.generated_class()).get_editor_property('mesh');assert comp.get_material(0)==mat
assert hashes=={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in protected}
(OUT/'BearApplied.json').write_text(json.dumps({'material':MAT,'fabric_texture':TEX,'original_mesh_texture_rig_blueprint_unchanged':True,'dissolve_preserved':True,'original_detail_preserved':True,'protected_hashes':hashes},indent=2))
u.log('BEAR_PATCHWORK_APPLIED')
