from pathlib import Path
import json,unreal as u
M=u.MaterialEditingLibrary;out={}
for name in ['Tree_Medium_Green','Tree_Small_Red']:
    mesh=u.load_asset('/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Trees/'+name);rows=[]
    for slot in mesh.get_editor_property('static_materials'):
        m=slot.material_interface;row={'name':str(slot.material_slot_name),'material':m.get_path_name()}
        if isinstance(m,u.MaterialInstanceConstant):
            row['scalars']={str(n):M.get_material_instance_scalar_parameter_value(m,n) for n in M.get_scalar_parameter_names(m)}
            row['vectors']={str(n):str(M.get_material_instance_vector_parameter_value(m,n)) for n in M.get_vector_parameter_names(m)}
            row['textures']={str(n):str(M.get_material_instance_texture_parameter_value(m,n)) for n in M.get_texture_parameter_names(m)}
        rows.append(row)
    out[name]=rows
Path(u.Paths.project_dir(),'Saved/OldVillage/BackdropMaterials.json').write_text(json.dumps(out,indent=2))
