"""Inspect effective blend overrides and the tree opacity graph."""
from pathlib import Path
import json, unreal as u
R=Path(u.Paths.project_dir()).resolve();E=u.EditorAssetLibrary;M=u.MaterialEditingLibrary;D={}
paths=[
'/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Grass/_Materials/M_StylizedFoliage',
'/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Trees/_Materials/MI_TreeLeaves',
'/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Trees/_Materials/MI_TreeLeaves_Red_2',
'/Game/HeavensDivide/Environment/OldVillage/CameraFade/M_StylizedFoliage_Fade_79b4c857',
'/Game/HeavensDivide/Environment/SceneryRepair/Materials/MI_TreeLeaves_Fade_f712dd5c_119b04_Stationary']
for p in paths:
 o=u.load_asset(p);row={'class':o.get_class().get_name()};D[p]=row
 if isinstance(o,u.MaterialInstanceConstant):
  over=o.get_editor_property('base_property_overrides')
  row['overrides']={k:str(over.get_editor_property(k)) for k in ['override_blend_mode','blend_mode','override_opacity_mask_clip_value','opacity_mask_clip_value','override_two_sided','two_sided']}
  row['scalars']={str(n):M.get_material_instance_scalar_parameter_value(o,n) for n in M.get_scalar_parameter_names(o)}
 else:
  row['properties']={k:str(o.get_editor_property(k)) for k in ['blend_mode','opacity_mask_clip_value','two_sided','use_material_attributes']}
  seen={}
  def walk(n):
   if not n or n.get_name() in seen:return
   v={'class':n.get_class().get_name()};seen[n.get_name()]=v
   for k in ['parameter_name','default_value','texture','material_function','const_a','const_b','constant','r']:
    try:v[k]=str(n.get_editor_property(k))
    except:pass
   ns=M.get_inputs_for_material_expression(o,n);v['inputs']=[x.get_name() for x in ns if x]
   for x in ns:walk(x)
  node=M.get_material_property_input_node(o,u.MaterialProperty.MP_MATERIAL_ATTRIBUTES if o.get_editor_property('use_material_attributes') else u.MaterialProperty.MP_OPACITY_MASK)
  row['opacity_root']=node.get_name() if node else None;walk(node);row['opacity_graph']=seen
  task=u.AssetExportTask();task.object=o;task.filename=str(R/'Saved/MapSceneryAudit'/(o.get_name()+'.t3d'));task.exporter=u.ObjectExporterT3D();task.automated=True;task.prompt=False;task.replace_identical=True;u.Exporter.run_asset_export_task(task)
for p in ['/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Trees/_Textures/T_Leaves_BC','/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Grass/_Textures/T_SimpleGrass_01_A_MROH']:
 t=u.load_asset(p);D[p]={k:str(t.get_editor_property(k)) for k in ['mip_gen_settings','do_scale_mips_for_alpha_coverage','alpha_coverage_thresholds','compression_settings','lod_bias','compression_no_alpha']}
(R/'Saved/MapSceneryAudit/TreeOverrides.json').write_text(json.dumps(D,indent=2))
