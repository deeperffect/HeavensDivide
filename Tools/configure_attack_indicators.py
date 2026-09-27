"""Shared black/red timed attack warnings; preserve attack dimensions and timing."""
from pathlib import Path
import json
import shutil
import unreal as u

root=Path(u.Paths.project_dir()).resolve()
folder='/Game/HeavensDivide/Materials'
lib=u.MaterialEditingLibrary
def backup(path):
 source=root/'Content'/(path.removeprefix('/Game/').split('.')[0]+'.uasset')
 target=root/'Saved/Backups/AttackIndicators'/source.relative_to(root/'Content')
 if source.exists() and not target.exists():
  target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)

common=r'''
float t=Clock*1.5;
float progress=saturate(Fill);
SHAPE
float aa=max(fwidth(d),0.0008);
float silhouette=1-smoothstep(-aa,aa,d);
float rim=1-smoothstep(0.003,0.009,abs(d+0.005));
float fringe=exp(-abs(d+0.015)*140)*pow(0.5+0.5*sin(curl-t*3),5);
float flooded=1-smoothstep(progress-0.006,progress+0.006,travel);
flooded*=step(0.0001,progress);
flooded=lerp(flooded,1,step(0.9999,progress));
float leading=exp(-abs(travel-progress)*180)*step(0.001,progress)*(1-step(0.999,progress));
float3 ink=float3(0.001,0.0001,0.0002);
float3 rgb=lerp(ink,Tint.rgb*0.65,flooded);
rgb=lerp(rgb,Edge.rgb*2.5,rim);
rgb+=Edge.rgb*fringe*0.45+Tint.rgb*leading*0.8;
return float4(rgb,silhouette);
'''
circle=r'''
float2 p=UV-0.5;
float angle=atan2(p.y,p.x);
float r=length(p);
float wave=0.0025*sin(angle*13+t*2.1)+0.0015*sin(angle*23-t*1.7+sin(angle*7+t));
float d=r-0.495+wave;
float travel=saturate(r/0.495);
float curl=angle*19+r*85;
'''
rectangle=r'''
float2 uv=COORDS;
float aspect=max(Aspect,0.05);
float2 p=(uv-0.5)*float2(aspect,1);
float side=0.003*sin(p.y*47+t*2.1)+0.0015*sin(p.y*103-t*1.7+sin(p.y*29+t));
float cap=0.0025*sin(p.x*83-t*1.8)+0.0015*sin(p.x*161+t*2.7);
float d=max(abs(p.x)-(aspect*0.5-0.006)+side,abs(p.y)-0.494+cap);
float travel=uv.y;
float curl=p.y*76+p.x*101;
'''
materials={}
for shape in ['Rectangle','Circle']:
 for decal in [False,True]:
  name='M_AttackIndicator'+shape+('_Decal' if decal else '')
  path=folder+'/'+name;backup(path)
  mat=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,u.Material,u.MaterialFactoryNew())
  lib.delete_all_material_expressions(mat)
  mat.set_editor_property('material_domain',u.MaterialDomain.MD_DEFERRED_DECAL if decal else u.MaterialDomain.MD_SURFACE)
  mat.set_editor_property('blend_mode',u.BlendMode.BLEND_TRANSLUCENT)
  mat.set_editor_property('shading_model',u.MaterialShadingModel.MSM_DEFAULT_LIT if decal else u.MaterialShadingModel.MSM_UNLIT)
  mat.set_editor_property('two_sided',True)
  def node(cls,x,y):return lib.create_material_expression(mat,cls,x,y)
  uv=node(u.MaterialExpressionTextureCoordinate,-600,0)
  clock=node(u.MaterialExpressionTime,-600,130)
  params={}
  for i,(key,value) in enumerate([('FillAmount',0.),('LaneAspect',.3)]):
   n=node(u.MaterialExpressionScalarParameter,-600,260+i*130);n.set_editor_property('parameter_name',key);n.set_editor_property('default_value',value);params[key]=n
  for i,(key,value) in enumerate([('FillColor',u.LinearColor(1,.02,.01,1)),('BorderColor',u.LinearColor(1,.004,.085,1))]):
   n=node(u.MaterialExpressionVectorParameter,-600,520+i*130);n.set_editor_property('parameter_name',key);n.set_editor_property('default_value',value);params[key]=n
  custom=node(u.MaterialExpressionCustom,-100,0)
  custom.set_editor_property('description','Black ground / swirling crimson rim / timed '+shape.lower()+' fill')
  custom.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT4)
  inputs=[]
  for key in ['UV','Clock','Fill','Aspect','Tint','Edge']:
   item=u.CustomInput();item.set_editor_property('input_name',key);inputs.append(item)
  custom.set_editor_property('inputs',inputs)
  code=common.replace('SHAPE',circle if shape=='Circle' else rectangle.replace('COORDS','UV' if decal else 'UV.yx'))
  custom.set_editor_property('code',code)
  for src,pin in [(uv,'UV'),(clock,'Clock'),(params['FillAmount'],'Fill'),(params['LaneAspect'],'Aspect'),(params['FillColor'],'Tint'),(params['BorderColor'],'Edge')]:
   assert lib.connect_material_expressions(src,'',custom,pin)
  for is_rgb,prop in [(True,u.MaterialProperty.MP_EMISSIVE_COLOR),(False,u.MaterialProperty.MP_OPACITY)]:
   mask=node(u.MaterialExpressionComponentMask,250,0 if is_rgb else 150)
   for channel in ['r','g','b','a']:mask.set_editor_property(channel,(channel!='a') if is_rgb else channel=='a')
   lib.connect_material_expressions(custom,'',mask,'');lib.connect_material_property(mask,'',prop)
  if decal:
   black=node(u.MaterialExpressionConstant,250,300);black.set_editor_property('r',0.)
   lib.connect_material_property(black,'',u.MaterialProperty.MP_BASE_COLOR)
  lib.recompile_material(mat)
  assert u.EditorAssetLibrary.save_loaded_asset(mat,False)
  materials[shape+('Decal' if decal else '')]=mat

updated=[]
registry=u.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()
for data in registry.get_assets_by_path('/Game/HeavensDivide/Blueprints',recursive=True):
 if str(data.asset_class_path.asset_name)!='Blueprint':continue
 bp=data.get_asset();cls=bp.generated_class()
 if not cls:continue
 cdo=u.get_default_object(cls)
 changes={}
 if isinstance(cdo,u.TankMeleeEnemyBase):
  shape=cdo.get_editor_property('attack_shape')
  changes['attack_telegraph_material']=materials['CircleDecal' if shape==u.TankSlamAttackShape.CIRCLE else 'Rectangle']
 elif isinstance(cdo,u.FinalBossBase):
  changes={'rectangle_telegraph_material':materials['Rectangle'],'circle_telegraph_material':materials['CircleDecal']}
 if not changes:continue
 backup(bp.get_path_name())
 for key,value in changes.items():cdo.set_editor_property(key,value)
 u.BlueprintEditorLibrary.compile_blueprint(bp)
 assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
 updated.append(bp.get_path_name())
report={'materials':{k:v.get_path_name() for k,v in materials.items()},'blueprints':updated}
out=root/'Saved/AttackIndicators';out.mkdir(parents=True,exist_ok=True)
(out/'setup.json').write_text(json.dumps(report,indent=2))
u.log('ATTACK_INDICATORS_SETUP_PASS '+json.dumps(report))
