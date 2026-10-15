"""Keep stems in every bamboo LOD; don't alter shared vendor assets."""
from pathlib import Path
import unreal as u
E=u.EditorAssetLibrary
SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.get_default_object(u.StaticMeshEditorSubsystem)
DEST='/Game/HeavensDivide/Environment/OldVillage/Polish/Meshes'
def repaired_mesh(variant):
    name='SM_TreeBamboo%02da'%variant;path=DEST+'/'+name
    source=u.load_asset('/Game/Assets/Environment/JapaneseShrine/Meshes/'+name)
    mesh=E.load_asset(path) if E.does_asset_exist(path) else E.duplicate_asset(source.get_path_name(),path)
    if SM.get_lod_count(mesh)!=2:
        assert SM.remove_lods(mesh)
        assert SM.set_lod_from_static_mesh(mesh,1,source,1,True)==1
        assert E.save_loaded_asset(mesh,False)
    for lod in range(SM.get_lod_count(mesh)):
        slots={SM.get_lod_material_slot(mesh,lod,i) for i in range(mesh.get_num_sections(lod))}
        assert {0,1}.issubset(slots),(path,lod,slots)
    return mesh

def repair_current_level():
    from polish_old_village import bamboo_leaves
    from setup_old_village_navigation_camera import fade_material
    meshes=[repaired_mesh(1),repaired_mesh(2)];cache={};report=[]
    actors=sorted(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors(),key=lambda a:a.get_name())
    for a in actors:
        if not isinstance(a,u.StaticMeshActor):continue
        c=a.static_mesh_component
        if not c.static_mesh or not c.static_mesh.get_name().startswith('SM_TreeBamboo'):continue
        mesh=meshes[len(report)%2];c.set_static_mesh(mesh)
        c.set_material(0,fade_material(mesh.get_material(0),cache));c.set_material(1,fade_material(bamboo_leaves(),cache))
        c.set_forced_lod_model(0);c.set_cull_distance(6500)
        c.set_editor_property('evaluate_world_position_offset',False)
        report.append({'actor':a.get_name(),'variant':mesh.get_name(),'lod_vertices':[SM.get_number_verts(mesh,i) for i in range(SM.get_lod_count(mesh))]})
    return report
