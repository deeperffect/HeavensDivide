"""Read/export current enemy rigs and their animation dependencies."""
import unreal as u,json,shutil
from pathlib import Path
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/EnemyModelReplacement';OUT.mkdir(parents=True,exist_ok=True)
registry=u.AssetRegistryHelpers.get_asset_registry();registry.search_all_assets(True)
def transform(t):return {'translation':[t.translation.x,t.translation.y,t.translation.z],'rotation':[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],'scale':[t.scale3d.x,t.scale3d.y,t.scale3d.z]}
report={}
for name in ['AshSeer','OgreWarden','HexSniper']:
    path='/Game/HeavensDivide/Blueprints/EnemyCharacters/Tactical/BP_'+name
    bp=u.load_asset(path);c=u.get_default_object(bp.generated_class());comp=c.get_editor_property('mesh');mesh=comp.get_skinned_asset()
    backup=OUT/'Backup'/Path(path.removeprefix('/Game/')+'.uasset');backup.parent.mkdir(parents=True,exist_ok=True)
    if not backup.exists():shutil.copy2(ROOT/'Content'/Path(path.removeprefix('/Game/')+'.uasset'),backup)
    mod=u.SkeletonModifier();assert mod.set_skeletal_mesh(mesh)
    r={'blueprint':path,'mesh':mesh.get_path_name(),'skeleton':mesh.get_editor_property('skeleton').get_path_name(),
       'animation_class':comp.get_editor_property('anim_class').get_path_name(),
       'component':{k:str(comp.get_editor_property(k)) for k in ['relative_location','relative_rotation','relative_scale3d','override_materials']},
       'cast_montage':str(c.get_editor_property('cast_montage')),
       'capsule':{k:str(c.get_component_by_class(u.CapsuleComponent).get_editor_property(k)) for k in ['relative_scale3d','capsule_half_height','capsule_radius']},
       'bones':[{'name':str(n),'parent':str(mod.get_parent_name(n)),'local':transform(mod.get_bone_transform(n,False)),'world':transform(mod.get_bone_transform(n,True))} for n in mod.get_all_bone_names()]}
    abp=r['animation_class'].split('.')[0]
    r['dependencies']=[str(x) for x in registry.get_dependencies(abp,u.AssetRegistryDependencyOptions(include_hard_package_references=True))]
    task=u.AssetExportTask();task.object=mesh;task.filename=str(OUT/(name+'_Donor.fbx'));task.automated=True;task.prompt=False;task.replace_identical=True
    task.options=u.FbxExportOption();task.options.set_editor_property('export_morph_targets',False);task.options.set_editor_property('level_of_detail',False);task.options.set_editor_property('bake_material_inputs',u.FbxMaterialBakeMode.DISABLED)
    report[name]=r
    (OUT/'DonorRigs.json').write_text(json.dumps(report,indent=2))
    assert u.Exporter.run_asset_export_task(task)
(OUT/'DonorRigs.json').write_text(json.dumps(report,indent=2))
u.log('ENEMY_DONOR_EXPORT_COMPLETE')
