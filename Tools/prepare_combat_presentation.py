from pathlib import Path
import json
import unreal as u
out=Path(u.Paths.project_dir())/'Saved/CombatPresentation'
path='/Game/HeavensDivide/VFX/Death/M_DeathSilhouette'
m=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else None
if not m:
 m=u.AssetToolsHelpers.get_asset_tools().create_asset('M_DeathSilhouette','/Game/HeavensDivide/VFX/Death',u.Material,u.MaterialFactoryNew())
 m.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
 m.set_editor_property('two_sided',True)
 m.set_editor_property('used_with_skeletal_mesh',True)
 m.set_editor_property('used_with_clothing',True)
 c=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionVectorParameter)
 c.set_editor_property('parameter_name','SilhouetteColor')
 c.set_editor_property('default_value',u.LinearColor(.7,.003,.013,1))
 u.MaterialEditingLibrary.connect_material_property(c,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
 u.MaterialEditingLibrary.recompile_material(m)
 assert u.EditorAssetLibrary.save_loaded_asset(m,False)
mesh=u.load_asset('/Game/Assets/VFX/SlashesV1/Models/SM_Slash')
b=mesh.get_bounding_box()
(out/'SlashMeshBounds.json').write_text(json.dumps({'min':str(b.min),'max':str(b.max)},indent=2))
