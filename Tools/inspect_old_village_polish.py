"""Read-only mesh/material and gate diagnostics for the scenery repair."""
from pathlib import Path
import json
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
M=u.MaterialEditingLibrary
def material(m):
    row={'path':m.get_path_name()}
    if isinstance(m,u.MaterialInstanceConstant):
        row['parent']=m.get_editor_property('parent').get_path_name()
        row['scalars']={str(n):M.get_material_instance_scalar_parameter_value(m,n) for n in M.get_scalar_parameter_names(m)}
        row['textures']={str(n):str(M.get_material_instance_texture_parameter_value(m,n)) for n in M.get_texture_parameter_names(m)}
        row['switches']={str(n):M.get_material_instance_static_switch_parameter_value(m,n) for n in M.get_static_switch_parameter_names(m)}
    return row
paths=['/Game/Assets/Environment/JapaneseShrine/Meshes/SM_TreeBamboo01a',
       '/Game/Assets/Environment/JapaneseShrine/Meshes/SM_TreeBamboo02a',
       '/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Bushes/SM_Bush_01',
       '/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Flowers/RosesAndDandelions/SM_Rose_Bush_Large_01']
r={'meshes':[],'actors':[]}
for path in paths:
    mesh=u.load_asset(path);bounds=mesh.get_bounds()
    r['meshes'].append({'path':path,'bounds':str(bounds),'collision':str(mesh.get_editor_property('body_setup')),
                        'materials':[material(slot.material_interface) for slot in mesh.get_editor_property('static_materials')]})
u.get_editor_subsystem(u.LevelEditorSubsystem).load_level('/Game/Maps/Lvl_OldVillage')
for actor in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor,u.BossToriiGate):
        row={'label':actor.get_actor_label(),'location':str(actor.get_actor_location()),'components':[]}
        for key in ['gate_visual','unlock_vfx']:
            c=actor.get_editor_property(key)
            row['components'].append({'name':key,'transform':str(c.get_world_transform()),'relative':str(c.get_editor_property('relative_location')),
                                       'asset':str(c.get_editor_property('asset') if key=='unlock_vfx' else c.static_mesh),
                                       'bounds':str(u.SystemLibrary.get_component_bounds(c))})
        r['actors'].append(row)
    elif actor.get_actor_label()=='Compound bamboo screen' and not any(x.get('bamboo') for x in r['actors']):
        c=actor.static_mesh_component
        r['actors'].append({'bamboo':True,'bounds':str(actor.get_actor_bounds(False)),'scale':str(actor.get_actor_scale3d()),
                            'materials':[material(c.get_material(i)) for i in range(c.get_num_materials())]})
(root/'Saved/OldVillage/PolishInspection.json').write_text(json.dumps(r,indent=2))
u.log('OLD_VILLAGE_POLISH_INSPECTED')
