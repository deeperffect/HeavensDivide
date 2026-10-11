"""Make the existing run-upgrade catalog available and remove retired trials.
Run with UnrealEditor-Cmd -ExecutePythonScript=<this file> after compiling.
Backups and a verification report go to Saved/TrialRemoval; no player save is read.
"""
from pathlib import Path
import json, shutil, unreal as u
root=Path(u.Paths.project_dir()).resolve()
out=root/'Saved/TrialRemoval'; out.mkdir(parents=True,exist_ok=True)
backup=out/'AssetBackups'
report={'upgrades':[], 'changed_assets':[], 'maps':[], 'trial_classes_cleared':[]}
def backup_package(package, extension='.uasset'):
    rel=Path('Content')/Path(package.removeprefix('/Game/')+extension)
    src=root/rel; dst=backup/rel
    dst.parent.mkdir(parents=True,exist_ok=True)
    if src.exists() and not dst.exists(): shutil.copy2(src,dst)
def clear_trials(obj):
    changed=False
    for prop,value in [('bEnableCharacterTrials',False),('SamuraiTrialClass',None),('NinjaTrialClass',None),('TwinSoulTrialClass',None)]:
        if obj.get_editor_property(prop)!=value:
            obj.set_editor_property(prop,value); changed=True
    report['trial_classes_cleared'].append(obj.get_path_name())
    return changed
registry=u.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(['/Game/HeavensDivide/Upgrades'],True)
for data in registry.get_assets_by_path('/Game/HeavensDivide/Upgrades',True):
    if str(data.asset_class_path.asset_name)!='UpgradeDefinition': continue
    asset=data.get_asset(); package=str(data.package_name)
    before=(asset.get_editor_property('requires_meta_unlock'),asset.get_editor_property('unlocked_by_default'))
    if before!=(False,True):
        backup_package(package)
        asset.modify(True)
        asset.set_editor_property('requires_meta_unlock',False)
        asset.set_editor_property('unlocked_by_default',True)
        assert u.EditorAssetLibrary.save_loaded_asset(asset,False), package
        report['changed_assets'].append(package)
    assert not asset.get_editor_property('requires_meta_unlock')
    assert asset.get_editor_property('unlocked_by_default')
    report['upgrades'].append({'path':package,'id':str(asset.get_editor_property('upgrade_id'))})
assert report['upgrades'], 'No upgrade definitions found'
package='/Game/HeavensDivide/Blueprints/Objectives/BP_RunObjectiveDirector'
bp=u.load_asset(package); assert bp
backup_package(package)
if clear_trials(u.get_default_object(bp.generated_class())):
    assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
    report['changed_assets'].append(package)
editor=u.get_editor_subsystem(u.EditorActorSubsystem)
retired=(u.TechniqueTrialBase,u.NinjaTechniqueTrial,u.TwinSoulTrial)
for file in sorted((root/'Content/Maps').glob('*.umap')):
    package='/Game/Maps/'+file.stem
    world=u.EditorLoadingAndSavingUtils.load_map(package); assert world,package
    actors=editor.get_all_level_actors()
    targets=[a for a in actors if isinstance(a,retired)]
    directors=[a for a in actors if isinstance(a,u.RunObjectiveDirector)]
    changes=False
    if targets or directors: backup_package(package,'.umap')
    removed=[]
    for actor in targets:
        removed.append(actor.get_actor_label())
        assert editor.destroy_actor(actor)
        changes=True
    for actor in directors: changes=clear_trials(actor) or changes
    if changes:
        assert u.EditorLoadingAndSavingUtils.save_map(world,package)
        report['changed_assets'].append(package)
    assert not any(isinstance(a,retired) for a in editor.get_all_level_actors()),package
    report['maps'].append({'path':package,'removed_trials':removed,'directors':len(directors),'saved':changes})
(out/'AvailabilityMigration.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
u.log('ALL_UPGRADES_AVAILABLE '+str(len(report['upgrades']))+' definitions; all project maps checked')
