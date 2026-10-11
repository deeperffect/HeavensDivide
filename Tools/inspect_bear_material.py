import json, unreal as u
from pathlib import Path
out=Path(u.Paths.project_dir())/'Saved/BearMaterial'
out.mkdir(parents=True,exist_ok=True)
report={}
for name in ['Enemy_Bear_Mat','Enemy_Bear','Enemy_Bear_base']:
    obj=u.load_asset('/Game/Assets/EnemyCharacters/Bear/'+name)
    assert obj,name
    task=u.AssetExportTask();task.object=obj;task.exporter=u.ObjectExporterT3D()
    task.filename=str(out/(name+'.copy'));task.automated=True;task.prompt=False;task.replace_identical=True
    u.Exporter.run_asset_export_task(task)
    report[name]={'class':obj.get_class().get_name()}
    if isinstance(obj,u.SkeletalMesh):
        report[name]['materials']=[m.material_interface.get_path_name() if m.material_interface else None for m in obj.get_editor_property('materials')]
for name,folder in [('EnemyBear','Mobs'),('WarDrummer','Tactical')]:
    bp=u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/'+folder+'/BP_'+name)
    cdo=u.get_default_object(bp.generated_class());mesh=cdo.get_editor_property('mesh')
    report[name]={'mesh':mesh.get_editor_property('skeletal_mesh_asset').get_path_name(),
                  'overrides':[m.get_path_name() if m else None for m in mesh.get_editor_property('override_materials')],
                  'materials':[mesh.get_material(i).get_path_name() if mesh.get_material(i) else None for i in range(mesh.get_num_materials())]}
(out/'Inspection.json').write_text(json.dumps(report,indent=2))
u.log('BEAR_MATERIAL_INSPECTION_COMPLETE')
