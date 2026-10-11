"""Validate saved enemy palette assets and intact dissolve graphs without modifying them."""
import unreal as u,json
from pathlib import Path
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/BearPatchwork';M=u.MaterialEditingLibrary;report={}
for label,bp_path,mat_path in [('Bear','/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyBear','/Game/Assets/EnemyCharacters/Bear/Enemy_Bear_Mat'),('Crawler','/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyGoblinCrawler','/Game/Assets/EnemyCharacters/Goblin/M_EnemyCrawler_SubtleColor')]:
 bp=u.load_asset(bp_path);comp=u.get_default_object(bp.generated_class()).get_editor_property('mesh');mat=u.load_asset(mat_path);assert comp.get_material(0)==mat
 assert mat.get_editor_property('blend_mode')==u.BlendMode.BLEND_MASKED
 mask=M.get_material_property_input_node(mat,u.MaterialProperty.MP_OPACITY_MASK);assert isinstance(mask,u.MaterialExpressionAdd)
 nodes=M.get_material_expressions(mat);params={str(n.get_editor_property('parameter_name')):n.get_editor_property('default_value') for n in nodes if isinstance(n,u.MaterialExpressionScalarParameter)}
 assert params['DissolveAmount']==0
 base=M.get_material_property_input_node(mat,u.MaterialProperty.MP_BASE_COLOR);assert base
 if label=='Bear':
  assert isinstance(base,u.MaterialExpressionCustom);assert base.get_editor_property('code')==(ROOT/'Art/Enemies/Bear/Patchwork/Bear_Patchwork_Palette.hlsl').read_text()
  inputs=M.get_inputs_for_material_expression(mat,base);assert len(inputs)==3
  interpolator=next(n for n in inputs if isinstance(n,u.MaterialExpressionVertexInterpolator))
  assert any(isinstance(n,u.MaterialExpressionPreSkinnedPosition) for n in M.get_inputs_for_material_expression(mat,interpolator))
  textures=[n.get_editor_property('texture').get_path_name() for n in inputs if isinstance(n,u.MaterialExpressionTextureSample)]
  assert '/Game/Assets/EnemyCharacters/Bear/Enemy_Bear_base.Enemy_Bear_base' in textures
  assert '/Game/Assets/EnemyCharacters/Bear/Textures/T_Bear_Patchwork_Fabric.T_Bear_Patchwork_Fabric' in textures
 else:
  assert abs(params['ColorSaturation']-1.2)<.0001;assert abs(params['ColorBrightness']-1.04)<.0001
 report[label]={'material':mat.get_path_name(),'assigned':True,'dissolve_connected':True,'parameters':params,'mesh':comp.get_skinned_asset().get_path_name(),'animation':comp.get_editor_property('anim_class').get_path_name()}
bomb=u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyGoblinBomb');comp=u.get_default_object(bomb.generated_class()).get_editor_property('mesh')
assert all(comp.get_material(i).get_path_name()!='/Game/Assets/EnemyCharacters/Goblin/M_EnemyCrawler_SubtleColor.M_EnemyCrawler_SubtleColor' for i in range(comp.get_num_materials()) if comp.get_material(i))
report['other_goblin_unaffected']=True
(OUT/'SavedAssetsVerified.json').write_text(json.dumps(report,indent=2));u.log('ENEMY_PALETTE_VERIFICATION_PASSED')
