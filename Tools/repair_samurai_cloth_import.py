from pathlib import Path
import json
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
base='/Game/Assets/PlayerCharacters/Samurai/'
mesh=u.load_asset(base+'fdsafdsa')
skeleton=u.load_asset(base+'SamuraiCharacterV4_Skeleton')
reference=json.loads((root/'Saved/SamuraiClothRepair/inspection.json').read_text())['original']['bones']
materials=list(mesh.get_editor_property('materials'))
assert len(materials)==2
modifier=u.SkeletonModifier();assert modifier.set_skeletal_mesh(mesh)
assert modifier.remove_bone('SamuraiCharacterV41',False)
assert modifier.parent_bone('root','None')
assert modifier.remove_bone('SamuraiCharacterV4',False)
assert set(map(str,modifier.get_all_bone_names()))=={b['name'] for b in reference},list(map(str,modifier.get_all_bone_names()))
for bone in reference:
    assert str(modifier.get_parent_name(bone['name']))==bone['parent'],bone['name']
    t=modifier.get_bone_transform(bone['name'],False)
    assert max(abs(a-b) for a,b in zip([t.translation.x,t.translation.y,t.translation.z],bone['translation']))<.0001,bone['name']
assert u.SwapVFXSetupLibrary.set_property_text(mesh,'Skeleton',"Skeleton'"+skeleton.get_path_name()+"'")
assert mesh.get_editor_property('skeleton')==skeleton
assert modifier.commit_skeleton_to_skeletal_mesh()
mesh.set_editor_property('physics_asset',u.load_asset(base+'SamuraiCharacterV4_PhysicsAsset'))
assert [str(m.get_editor_property('material_slot_name')) for m in mesh.get_editor_property('materials')]==[str(m.get_editor_property('material_slot_name')) for m in materials]
assert u.EditorAssetLibrary.save_loaded_asset(mesh,False)
bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai')
abp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/ABP_Samurai')
assert abp.get_editor_property('target_skeleton')==skeleton
for asset in [abp,bp]:
    u.BlueprintEditorLibrary.compile_blueprint(asset)
    assert 'ERROR' not in str(asset.get_editor_property('status'))
cdo=u.get_default_object(bp.generated_class())
component=cdo.get_editor_property('mesh')
assert component.get_skinned_asset()==mesh
assert component.get_editor_property('anim_class')==abp.generated_class()
assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
assert u.EditorAssetLibrary.save_loaded_asset(abp,False)
report={'mesh':mesh.get_path_name(),'skeleton':skeleton.get_path_name(),'bone_count':len(reference),
        'materials':[str(m.get_editor_property('material_slot_name')) for m in materials]}
(root/'Saved/SamuraiClothRepair/repair.json').write_text(json.dumps(report,indent=2))
u.log('SAMURAI_CLOTH_REPAIR_PASS '+json.dumps(report))
