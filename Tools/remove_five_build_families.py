"""Remove the retired Lotus Mines, Shadow Shuriken, Smoke Lattice, Thunder Wire, and Caltrop Trail families and their exclusive artwork.

Run in Unreal's Python commandlet. Backups are kept under Saved/Backups.
Re-run with -ValidateFamilyRemoval to inspect saved assets without editing.
"""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
ids={'DoubleVeil', 'HiddenBlade', 'ForkedTrail', 'CaltropTrailPower', 'SteelRicochet', 'SplittingStar', 'ShadowShurikenHaste', 'SmokeLatticeHaste', 'CaltropTrailHaste', 'LotusMinesHaste', 'ReturningStar', 'SmokeLattice', 'CrossWire', 'VenomCable', 'LotusExecution', 'ToxicStar', 'BarbedExit', 'ShadowShurikenArea', 'CaltropTrail', 'VenomPetals', 'LotusMines', 'SmokeLatticeArea', 'GroundedSteel', 'PersistentSpikes', 'DoublePetals', 'SmokeLatticePower', 'LotusMinesPower', 'ThunderWire', 'SpikedOpening', 'LotusMinesArea', 'CaltropTrailArea', 'ThunderWireHaste', 'ShadowShurikenPower', 'ChokingFinale', 'LiveWire', 'ThunderWireArea', 'ShadowShuriken', 'ThunderWirePower', 'DriftingSmoke', 'PatientLotus'}
retired={'SmokeLattice', 'ShadowShuriken', 'CaltropTrail', 'LotusMines', 'ThunderWire'}
synergies={'HiddenBlade', 'SteelRicochet', 'GroundedSteel', 'LotusExecution', 'SpikedOpening'}
paths=['/Game/HeavensDivide/Upgrades/'+('Synergy/DA_BuildSynergy_' if uid in synergies else 'Ninja/DA_Upgrade_Ninja')+uid for uid in sorted(ids)]
art=['/Game/HeavensDivide/Blueprints/UI/BuildFamilyArt/T_'+uid for uid in sorted(ids)]
controller='/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
validate=unreal.SystemLibrary.get_command_line().find('-ValidateFamilyRemoval')>=0
backup=root/'Saved/Backups/FiveFamilyRemoval'/datetime.now().strftime('%Y%m%d_%H%M%S')

def backup_file(path):
    path=path.resolve()
    relative=path.relative_to(root)
    target=backup/relative
    target.parent.mkdir(parents=True,exist_ok=True)
    if path.exists():shutil.copy2(path,target)

if not validate:
    # Confirm references before deleting anything. Shared assets are not retired.
    for path in paths+art:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            refs=unreal.EditorAssetLibrary.find_package_referencers_for_asset(path,True)
            unexpected=set(str(r) for r in refs)-set(paths+art+[controller,path])
            if unexpected:raise RuntimeError('Unexpected references to '+path+': '+str(unexpected))
            backup_file(root/'Content'/(path.removeprefix('/Game/')+'.uasset'))
    backup_file(root/'Content'/(controller.removeprefix('/Game/')+'.uasset'))
    bp=unreal.load_asset(controller)
    component=unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
    pool=component.get_editor_property('upgrade_pool')
    retained=[a for a in pool if a and str(a.get_editor_property('upgrade_id')) not in ids]
    assert len(pool)-len(retained) in (0,40), 'Unexpected removed upgrade count'
    component.set_editor_property('upgrade_pool',retained)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
    for path in paths+art:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            assert unreal.EditorAssetLibrary.delete_asset(path),path
    for uid in ids:
        source=(root/'Art/UpgradeCards/Generated'/(uid+'.png')).resolve()
        assert source.is_relative_to(root/'Art/UpgradeCards/Generated')
        if source.exists():backup_file(source);source.unlink()
    unreal.log('FAMILY_REMOVAL_BACKUP: '+str(backup))

for path in paths+art:
    assert not unreal.EditorAssetLibrary.does_asset_exist(path),path
bp=unreal.load_asset(controller)
pool=unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component').get_editor_property('upgrade_pool')
assert all(a and str(a.get_editor_property('upgrade_id')) not in ids for a in pool)
for a in pool:
    assert str(a.get_editor_property('build_family_id'))not in retired,a.get_path_name()
    assert not ids.intersection(str(v) for v in a.get_editor_property('prerequisite_upgrade_ids')),a.get_path_name()
    for req in a.get_editor_property('prerequisite_requirements'):
        assert str(req.get_editor_property('upgrade_id')) not in ids,a.get_path_name()
unreal.log('FAMILY_REMOVAL_OK: 40 upgrades and 40 textures absent; no retired family or prerequisite in '+str(len(pool))+' pool entries')
