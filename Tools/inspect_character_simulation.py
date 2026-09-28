import unreal as u
from pathlib import Path
import json
out=Path(u.Paths.project_saved_dir())/'CharacterSimulation';out.mkdir(parents=True,exist_ok=True)
report={}
for name in ['Ninja','Samurai']:
    bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_'+name)
    cdo=u.get_default_object(bp.generated_class());component=cdo.get_editor_property('mesh');mesh=component.get_skinned_asset()
    modifier=u.SkeletonModifier();modifier.set_skeletal_mesh(mesh)
    report[name]={'mesh':mesh.get_path_name(),'materials':[str(x.get_editor_property('material_slot_name')) for x in mesh.get_editor_property('materials')],
                  'cloth':str(mesh.get_editor_property('mesh_clothing_assets')),'physics':str(mesh.get_editor_property('physics_asset')),
                  'hair_bones':[{'name':str(n),'parent':str(modifier.get_parent_name(n)),'transform':str(modifier.get_bone_transform(n,True))} for n in modifier.get_all_bone_names() if any(token in str(n).lower() for token in ['hair','ponytail'])]}
bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/ABP_Ninja')
task=u.AssetExportTask();task.object=bp;task.filename=str(out/'ABP_Ninja.copy');task.automated=True;task.prompt=False;task.replace_identical=True
assert u.Exporter.run_asset_export_task(task)
(out/'inspection.json').write_text(json.dumps(report,indent=2))
u.log('CHARACTER_SIMULATION_INSPECT_PASS '+json.dumps(report))
