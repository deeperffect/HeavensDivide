"""Give only the crawler a restrained color boost, preserving its shared source material."""
import unreal as u,json,shutil,hashlib
from pathlib import Path
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/BearPatchwork';BACKUP=OUT/'Backup';BACKUP.mkdir(exist_ok=True)
BP='/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyGoblinCrawler'
SOURCE='/Game/Assets/EnemyCharacters/Goblin/Enemy_Goblin_Mat'
DEST='/Game/Assets/EnemyCharacters/Goblin/M_EnemyCrawler_SubtleColor'
M=u.MaterialEditingLibrary;E=u.EditorAssetLibrary
bpfile=ROOT/'Content'/Path(BP.removeprefix('/Game/')+'.uasset')
backup=BACKUP/'BP_EnemyGoblinCrawler.uasset'
if not backup.exists():shutil.copy2(bpfile,backup)
source=u.load_asset(SOURCE);assert source
sourcefile=ROOT/'Content'/Path(SOURCE.removeprefix('/Game/')+'.uasset');source_hash=hashlib.sha256(sourcefile.read_bytes()).hexdigest()
mat=u.load_asset(DEST) if E.does_asset_exist(DEST) else u.AssetToolsHelpers.get_asset_tools().duplicate_asset(DEST.rsplit('/',1)[1],DEST.rsplit('/',1)[0],source)
existing={str(n.get_editor_property('parameter_name')):n for n in M.get_material_expressions(mat) if isinstance(n,u.MaterialExpressionScalarParameter)}
if 'ColorSaturation' not in existing:
 base=M.get_material_property_input_node(mat,u.MaterialProperty.MP_BASE_COLOR);assert isinstance(base,u.MaterialExpressionTextureSample)
 saturation=M.create_material_expression(mat,u.MaterialExpressionScalarParameter,-750,-430);saturation.set_editor_property('parameter_name','ColorSaturation')
 invert=M.create_material_expression(mat,u.MaterialExpressionOneMinus,-530,-430);assert M.connect_material_expressions(saturation,'',invert,'')
 desat=M.create_material_expression(mat,u.MaterialExpressionDesaturation,-320,-250);assert M.connect_material_expressions(base,'RGB',desat,'');assert M.connect_material_expressions(invert,'',desat,'Fraction')
 brightness=M.create_material_expression(mat,u.MaterialExpressionScalarParameter,-300,-450);brightness.set_editor_property('parameter_name','ColorBrightness')
 multiply=M.create_material_expression(mat,u.MaterialExpressionMultiply,-50,-240);assert M.connect_material_expressions(desat,'',multiply,'A');assert M.connect_material_expressions(brightness,'',multiply,'B');assert M.connect_material_property(multiply,'',u.MaterialProperty.MP_BASE_COLOR)
else:saturation=existing['ColorSaturation'];brightness=existing['ColorBrightness']
saturation.set_editor_property('default_value',1.20);brightness.set_editor_property('default_value',1.04)
assert M.get_material_property_input_node(mat,u.MaterialProperty.MP_OPACITY_MASK)
assert any(isinstance(n,u.MaterialExpressionScalarParameter) and str(n.get_editor_property('parameter_name'))=='DissolveAmount' for n in M.get_material_expressions(mat))
M.recompile_material(mat);assert E.save_loaded_asset(mat,False)
bp=u.load_asset(BP);cdo=u.get_default_object(bp.generated_class());comp=cdo.get_editor_property('mesh');mesh=comp.get_skinned_asset();before={'mesh':mesh.get_path_name(),'anim_class':comp.get_editor_property('anim_class').get_path_name()}
for i in range(comp.get_num_materials()):
 if comp.get_material(i)==source or comp.get_material(i)==mat:comp.set_material(i,mat)
u.BlueprintEditorLibrary.compile_blueprint(bp);assert 'ERROR' not in str(bp.get_editor_property('status'));assert E.save_loaded_asset(bp,False)
comp=u.get_default_object(bp.generated_class()).get_editor_property('mesh')
after={'mesh':comp.get_skinned_asset().get_path_name(),'anim_class':comp.get_editor_property('anim_class').get_path_name()}
assert before['mesh']==after['mesh'] and before['anim_class']==after['anim_class'];assert comp.get_material(0)==mat
assert hashlib.sha256(sourcefile.read_bytes()).hexdigest()==source_hash
(OUT/'CrawlerApplied.json').write_text(json.dumps({'blueprint':BP,'material':DEST,'saturation':1.20,'brightness':1.04,'shared_source_unchanged':True,'mesh':mesh.get_path_name(),'dissolve_preserved':True},indent=2))
u.log('CRAWLER_COLOR_APPLIED')
