"""Save reduced urn schedules without changing objectives, rewards or combat."""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal as u

root=Path(u.Paths.project_dir()).resolve()
out=root/'Saved/PickupReduction';out.mkdir(parents=True,exist_ok=True)
backup=out/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S')
bp_path='/Game/HeavensDivide/Blueprints/Encounters/BP_EncounterDirector'
targets=dict(first_urn_seconds=36.,urn_interval_min=76.,urn_interval_max=110.,
             first_magnet_seconds=150.,magnet_interval_min=200.,magnet_interval_max=280.)
protected=['first_march_seconds','march_interval_min','march_interval_max','max_march_enemies']
report=dict(targets=targets,blueprint={},maps={},backup=str(backup))

def preserve(path,extension):
    source=root/'Content'/(path.removeprefix('/Game/')+extension)
    dest=backup/source.relative_to(root);dest.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(source,dest)

def snapshot(actor):
    return {key:actor.get_editor_property(key) for key in [*targets,*protected]}

def tune(actor):
    before=snapshot(actor);actor.modify(True)
    for key,value in targets.items():actor.set_editor_property(key,value)
    after=snapshot(actor)
    assert all(abs(after[k]-v)<.001 for k,v in targets.items())
    assert all(before[k]==after[k] for k in protected)
    return dict(before=before,after=after)

preserve(bp_path,'.uasset')
bp=u.load_asset(bp_path);assert bp
report['blueprint']=tune(u.get_default_object(bp.generated_class()))
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
assert snapshot(u.get_default_object(bp.generated_class()))==report['blueprint']['after']
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for path in ['/Game/Maps/Lvl_B1_Lvl1','/Game/Maps/TestMap','/Game/Maps/Lvl_B1_Arena']:
    assert levels.load_level(path)
    directors=[a for a in actors.get_all_level_actors() if isinstance(a,u.EncounterDirector)]
    if not directors:continue
    preserve(path,'.umap')
    rows={a.get_path_name():tune(a) for a in directors}
    assert levels.save_current_level()
    assert levels.load_level('/Game/Maps/Lvl_MainMenu');assert levels.load_level(path)
    for a in actors.get_all_level_actors():
        if isinstance(a,u.EncounterDirector):assert snapshot(a)==rows[a.get_path_name()]['after']
    report['maps'][path]=rows

# Any authored chest overrides must use the smaller shower too.
registry=u.AssetRegistryHelpers.get_asset_registry();registry.search_all_assets(True)
report['chests']={}
for asset in registry.get_assets_by_path('/Game/HeavensDivide',recursive=True):
    if str(asset.asset_class_path.asset_name)!='Blueprint':continue
    # The project currently uses the native chest; only load possible chest assets.
    if 'Chest' not in str(asset.asset_name):continue
    blueprint=asset.get_asset();cls=blueprint.generated_class()
    if not cls:continue
    cdo=u.get_default_object(cls)
    if not isinstance(cdo,u.EliteRewardChest):continue
    path=str(asset.package_name);preserve(path,'.uasset')
    before=cdo.get_editor_property('orb_count');cdo.set_editor_property('orb_count',12)
    assert u.EditorAssetLibrary.save_loaded_asset(blueprint,False)
    report['chests'][path]=dict(before=before,after=12)

baseline_path=root/'Config/BloodshiftBalanceBaseline.json'
baseline=json.loads(baseline_path.read_text(encoding='utf-8-sig'))
shutil.copy2(baseline_path,backup/'BloodshiftBalanceBaseline.json')
cdo=u.get_default_object(bp.generated_class())
values=baseline.setdefault(cdo.get_path_name(),{})
for key,value in targets.items():values[''.join(part.title() for part in key.split('_'))]=f'{value:.6f}'
for path in report['chests']:
    chest=u.get_default_object(u.load_asset(path).generated_class())
    baseline.setdefault(chest.get_path_name(),{})['OrbCount']='12'
baseline_path.write_text(json.dumps(baseline,indent=2)+'\n')
healing=u.get_default_object(u.load_class(None,'/Script/HeavensDivide.HealingPickupDropSettings'))
experience=u.get_default_object(u.load_class(None,'/Script/HeavensDivide.ExperiencePickupDropSettings'))
report['healing']={k:healing.get_editor_property(''.join(part.title() for part in k.split('_')))
                   for k in ['normal_drop_chance','elite_drop_chance','boss_drop_chance','heal_drop_cooldown']}
report['xp_grouping']={k:experience.get_editor_property(''.join(part.title() for part in k.split('_')))
                      for k in ['rewards_per_pickup','merge_radius']}
assert report['healing']['normal_drop_chance']<.008
assert report['xp_grouping']['rewards_per_pickup']==4
(out/'Applied.json').write_text(json.dumps(report,indent=2))
u.log('PICKUP_REDUCTION_APPLIED')
