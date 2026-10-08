import unreal as u
from pathlib import Path
import json,shutil
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/NinjaV6Replacement'
bp_path='/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja'
bp_file=ROOT/'Content/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja.uasset'
backup=OUT/'Backup/Content/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja.uasset'
backup.parent.mkdir(parents=True,exist_ok=True)
if not backup.exists():shutil.copy2(bp_file,backup)
bp=u.load_asset(bp_path);abp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/ABP_Ninja')
mesh=u.load_asset('/Game/Assets/PlayerCharacters/Ninja/V6/SK_NinjaV6')
assert mesh.get_editor_property('skeleton')==abp.get_editor_property('target_skeleton')
assert len(mesh.get_editor_property('mesh_clothing_assets'))==1
cdo=u.get_default_object(bp.generated_class());component=cdo.get_editor_property('mesh')
original_transform={k:str(component.get_editor_property(k)) for k in ['relative_location','relative_rotation','relative_scale3d']}
component.set_skeletal_mesh_asset(mesh)
component.set_editor_property('override_materials',[])
assert component.get_editor_property('anim_class')==abp.generated_class()
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert 'ERROR' not in str(bp.get_editor_property('status'))
component=u.get_default_object(bp.generated_class()).get_editor_property('mesh')
assert component.get_skinned_asset()==mesh
assert {k:str(component.get_editor_property(k)).split('{')[-1] for k in original_transform}=={k:v.split('{')[-1] for k,v in original_transform.items()}
assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
report={'blueprint':bp.get_path_name(),'mesh':mesh.get_path_name(),'animation_blueprint':abp.get_path_name(),
    'skeleton':mesh.get_editor_property('skeleton').get_path_name(),
    'materials':[{'slot':str(m.material_slot_name),'material':m.material_interface.get_path_name()} for m in mesh.get_editor_property('materials')],
    'cloth_assets':len(mesh.get_editor_property('mesh_clothing_assets')),'backup':str(backup)}
(OUT/'Replacement.json').write_text(json.dumps(report,indent=2))
u.log('NINJA_V6_REPLACEMENT_PASS '+json.dumps(report))
