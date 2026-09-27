from pathlib import Path
import json
import unreal as u
out=Path(u.Paths.project_saved_dir())/'SamuraiClothRepair';out.mkdir(parents=True,exist_ok=True)
report={}
for label,path in [('original','/Game/__SamuraiRepairReference/SamuraiCharacterV4'),('new','/Game/Assets/PlayerCharacters/Samurai/fdsafdsa')]:
    mesh=u.load_asset(path)
    modifier=u.SkeletonModifier();assert modifier.set_skeletal_mesh(mesh)
    names=modifier.get_all_bone_names()
    bones=[]
    for name in names:
        t=modifier.get_bone_transform(name,False)
        bones.append({'name':str(name),'parent':str(modifier.get_parent_name(name)),
                      'translation':[t.translation.x,t.translation.y,t.translation.z],
                      'rotation':[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],
                      'scale':[t.scale3d.x,t.scale3d.y,t.scale3d.z]})
    report[label]={'path':mesh.get_path_name(),'skeleton':str(mesh.get_editor_property('skeleton')),'bones':bones,
                   'materials':[str(x.get_editor_property('material_slot_name')) for x in mesh.get_editor_property('materials')],
                   'clothing':str(mesh.get_editor_property('mesh_clothing_assets'))}
(out/'inspection.json').write_text(json.dumps(report,indent=2))
u.log('SAMURAI_CLOTH_INSPECT_PASS '+json.dumps(report))
