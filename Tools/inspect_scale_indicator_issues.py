import unreal as u,json
from pathlib import Path
out=Path(u.Paths.project_saved_dir())/'ScaleIndicatorFix';out.mkdir(parents=True,exist_ok=True)
report={}
for name in ['M_AttackIndicatorRectangle','M_AttackIndicatorRectangle_Decal','M_AttackIndicatorCircle','M_AttackIndicatorCircle_Decal','M_SamuraiLaneIndicator']:
 m=u.load_asset('/Game/HeavensDivide/Materials/'+name)
 report[name]={'domain':str(m.get_editor_property('material_domain')),'blend':str(m.get_editor_property('blend_mode')),'parameters':{}}
 for e in u.MaterialEditingLibrary.get_material_expressions(m):
  if isinstance(e,(u.MaterialExpressionScalarParameter,u.MaterialExpressionVectorParameter)):report[name]['parameters'][str(e.get_editor_property('parameter_name'))]=str(e.get_editor_property('default_value'))
  if isinstance(e,u.MaterialExpressionCustom):(out/(name+'.hlsl')).write_text(e.get_editor_property('code'))
for n in ['Elites/BP_EnemyOgre','Elites/BP_EnemyGorilla','Mobs/BP_EnemyGoblinBomb']:
 bp=u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/'+n);c=u.get_default_object(bp.generated_class());r={}
 for k in dir(c):
  if any(x in k for x in ['color','material','opacity','indicator','telegraph','aura']):
   try:r[k]=str(c.get_editor_property(k))
   except:pass
 report[n]=r
for n in ['OverkillBurst','SamuraiActive']:
 s=u.load_asset('/Game/HeavensDivide/VFX/Upgrades/NS_'+n)
 t=u.AssetExportTask();t.object=s;t.exporter=u.ObjectExporterT3D();t.filename=str(out/(n+'.copy'));t.automated=True;t.prompt=False;t.replace_identical=True
 assert u.Exporter.run_asset_export_task(t)
bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai');report['ability']=str(u.get_default_object(bp.generated_class()).get_editor_property('combo_ability'))
(out/'audit.json').write_text(json.dumps(report,indent=2))
u.log('SCALE_INDICATOR_INSPECTION_READY')
