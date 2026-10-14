import json
from pathlib import Path
import unreal as u
out=Path(u.Paths.project_dir())/'Saved/FangSizes'
out.mkdir(parents=True,exist_ok=True)
bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_NinjaProjectileFang')
cdo=u.get_default_object(bp.generated_class())
sub=u.get_engine_subsystem(u.SubobjectDataSubsystem)
rows=[]
for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
 data=u.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
 obj=u.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data,bp)
 if isinstance(obj,u.SceneComponent):
  row={'name':obj.get_name(),'class':obj.get_class().get_name(),'scale':str(obj.get_editor_property('relative_scale3d')),'location':str(obj.get_editor_property('relative_location')),'path':obj.get_path_name()}
  if isinstance(obj,u.StaticMeshComponent):
   mesh=obj.get_editor_property('static_mesh')
   row['mesh']=str(mesh)
   if mesh: row['bounds']=str(mesh.get_bounds())
  if isinstance(obj,u.NiagaraComponent):row['system']=str(obj.get_editor_property('asset'))
  rows.append(row)
combo=u.load_asset('/Game/HeavensDivide/Abilities/Combos/DA_Combo_HeavensDivide')
report={'actor_scale':str(cdo.get_actor_scale3d()),'root':cdo.get_editor_property('root_component').get_name(),'eclipse_scale':combo.get_editor_property('fang_presentation_scale'),'components':rows}
(out/'Before.json').write_text(json.dumps(report,indent=2))
u.log('FANG_SIZES '+json.dumps(report))
