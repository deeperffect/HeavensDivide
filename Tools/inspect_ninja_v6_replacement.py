import unreal as u
from pathlib import Path
import json

root = Path(u.Paths.project_dir()).resolve()
out = root/'Saved/NinjaV6Replacement'
out.mkdir(parents=True, exist_ok=True)
bp = u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja')
cdo = u.get_default_object(bp.generated_class())
component = cdo.get_editor_property('mesh')
mesh = component.get_skinned_asset()
skeleton = mesh.get_editor_property('skeleton')
modifier = u.SkeletonModifier()
assert modifier.set_skeletal_mesh(mesh)

def transform(t):
    return {'translation': [t.translation.x, t.translation.y, t.translation.z],
            'rotation': [t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w],
            'scale': [t.scale3d.x, t.scale3d.y, t.scale3d.z]}

report = {
    'mesh': mesh.get_path_name(), 'skeleton': skeleton.get_path_name(),
    'physics': str(mesh.get_editor_property('physics_asset')),
    'mesh_component': {k: str(component.get_editor_property(k)) for k in
        ['relative_location','relative_rotation','relative_scale3d','anim_class','override_materials']},
    'bones': [{'name': str(n), 'parent': str(modifier.get_parent_name(n)),
               'local': transform(modifier.get_bone_transform(n, False)),
               'world': transform(modifier.get_bone_transform(n, True))} for n in modifier.get_all_bone_names()],
    'materials': [{'slot': str(m.get_editor_property('material_slot_name')),
                  'material': str(m.get_editor_property('material_interface'))} for m in mesh.get_editor_property('materials')],
    'cloth': str(mesh.get_editor_property('mesh_clothing_assets')),
}
registry = u.AssetRegistryHelpers.get_asset_registry()
report['animation_assets'] = [a.object_path if hasattr(a,'object_path') else a.get_asset().get_path_name()
    for a in registry.get_assets_by_path('/Game/Assets/PlayerCharacters/Ninja/RetargetedAnimations', True)]
(out/'CurrentNinja.json').write_text(json.dumps(report, indent=2, default=str))
task = u.AssetExportTask()
task.object = mesh
task.filename = str(out/'CurrentNinja_Rig.fbx')
task.automated = True
task.prompt = False
task.replace_identical = True
task.options = u.FbxExportOption()
task.options.set_editor_property('export_morph_targets', False)
task.options.set_editor_property('level_of_detail', False)
task.options.set_editor_property('bake_material_inputs', u.FbxMaterialBakeMode.DISABLED)
assert u.Exporter.run_asset_export_task(task)
(out/'CurrentNinja.json').write_text(json.dumps(report, indent=2, default=str))
u.log('NINJA_V6_CURRENT_EXPORT_PASS '+str(out))
