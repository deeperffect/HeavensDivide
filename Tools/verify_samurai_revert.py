from pathlib import Path
import json
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
manifest=json.loads((root/'Saved/Backups/SamuraiRevert20260928/manifest.json').read_text())
base='/Game/Assets/PlayerCharacters/Samurai/'
mesh=u.load_asset(base+'SamuraiCharacterV4')
skeleton=u.load_asset(base+'SamuraiCharacterV4_Skeleton')
assert mesh and skeleton
assert mesh.get_editor_property('skeleton')==skeleton
assert mesh.get_editor_property('physics_asset')==u.load_asset(base+'SamuraiCharacterV4_PhysicsAsset')
bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai')
cdo=u.get_default_object(bp.generated_class())
component=cdo.get_editor_property('mesh')
assert component.get_skinned_asset()==mesh
checked=[]
for item in manifest['restored_files']:
    path='/Game/'+item['path'].removeprefix('Content/').removesuffix('.uasset')
    asset=u.load_asset(path)
    assert asset,path
    if isinstance(asset,u.AnimationAsset): assert asset.get_editor_property('skeleton')==skeleton,path
    if isinstance(asset,u.AnimBlueprint): assert asset.get_editor_property('target_skeleton')==skeleton,path
    if isinstance(asset,u.Blueprint):
        u.BlueprintEditorLibrary.compile_blueprint(asset)
        assert 'ERROR' not in str(asset.get_editor_property('status')),path
    checked.append(path)
registry=u.AssetRegistryHelpers.get_asset_registry();registry.wait_for_completion()
new_assets=[base+'SamuraiCharacterV5'+suffix for suffix in ['', '_PhysicsAsset','_Skeleton']]
references={p:list(map(str,u.EditorAssetLibrary.find_package_referencers_for_asset(p,False))) for p in new_assets}
report={'mesh':mesh.get_path_name(),'skeleton':skeleton.get_path_name(),'checked':checked,'v5_references':references}
(root/'Saved/Backups/SamuraiRevert20260928/verification.json').write_text(json.dumps(report,indent=2))
u.log('SAMURAI_REVERT_VERIFY_PASS '+json.dumps(report))
