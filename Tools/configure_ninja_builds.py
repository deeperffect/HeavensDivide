"""Create Ninja prototype build cards; preserve all existing Samurai tuning.

-ValidateNinjaBuilds validates saved assets without editing.
"""
import json,shutil,hashlib
from pathlib import Path
from datetime import datetime
import unreal
root=Path(unreal.Paths.project_dir()).resolve()
rows=json.loads((root/'Tools/ninja_build_upgrades.json').read_text())
controller='/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
folder='/Game/HeavensDivide/Upgrades/Ninja'
validate='-ValidateNinjaBuilds' in unreal.SystemLibrary.get_command_line()
backup=root/'Saved/Backups/NinjaBuilds'/datetime.now().strftime('%Y%m%d_%H%M%S')
bp=unreal.load_asset(controller);comp=unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
def disk(a):return root/'Content'/(a.get_path_name().removeprefix('/Game/').split('.')[0]+'.uasset')
preserved={str(disk(a)):hashlib.sha256(disk(a).read_bytes()).hexdigest() for a in comp.get_editor_property('upgrade_pool') if a and str(a.get_editor_property('upgrade_id')) not in {r['id'] for r in rows}}
def props(a,values):
    for k,v in values.items():a.set_editor_property(k,v)
def copy(path):
    rel=Path('Content')/(path.removeprefix('/Game/')+'.uasset')
    if (root/rel).exists():(backup/rel).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/rel,backup/rel)
if not validate:
    factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.UpgradeDefinition)
    cards=[]
    stats={'AttackSpeedMultiplier':'ATTACK_SPEED_MULTIPLIER','DamageMultiplier':'DAMAGE_MULTIPLIER','ProjectileSpeedMultiplier':'PROJECTILE_SPEED_MULTIPLIER','ProjectileCountBonus':'PROJECTILE_COUNT_BONUS'}
    for r in rows:
        path=folder+'/DA_Upgrade_Ninja'+r['id'];copy(path)
        exists=unreal.EditorAssetLibrary.does_asset_exist(path)
        a=unreal.load_asset(path) if exists else unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],folder,unreal.UpgradeDefinition,factory)
        if not exists:
            art=unreal.load_asset('/Game/HeavensDivide/Blueprints/UI/CardArt2/Ninja/'+r['art']);assert art
            mods=[]
            for stat,value in r.get('mods',[]):
                m=unreal.UpgradeStatModifierDefinition();props(m,dict(target=unreal.UpgradeStatTarget.NINJA,character_stat=getattr(unreal.CharacterStatType,stats[stat]),operation=unreal.StatModifierOperation.ADD_FLAT if stat=='ProjectileCountBonus' else unreal.StatModifierOperation.MULTIPLY,value_per_level=value));mods.append(m)
            mags=[]
            for rarity,value in zip([unreal.UpgradeRarity.COMMON,unreal.UpgradeRarity.RARE,unreal.UpgradeRarity.EPIC],r.get('magnitudes',[])):
                m=unreal.UpgradeRarityMagnitude();props(m,dict(rarity=rarity,magnitude=value));mags.append(m)
            props(a,dict(upgrade_id=r['id'],display_name=r['name'],description=r['description'],category=unreal.UpgradeCategory.NINJA,investment_owner=unreal.UpgradeInvestmentOwner.NINJA,role=unreal.UpgradeRole.STARTER if r.get('stance') or not r['requires'] else unreal.UpgradeRole.SUPPORT if r.get('levels',1)>1 else unreal.UpgradeRole.MECHANIC,build_family_id='None',max_level=r.get('levels',1),rarity=unreal.UpgradeRarity.COMMON if mags else unreal.UpgradeRarity.RARE,uses_rolled_rarity=bool(mags),rarity_magnitudes=mags,rolled_description_format=r.get('format',''),prerequisite_upgrade_ids=r['requires'],prerequisite_requirements=[],exclusivity_group='NinjaWeaponStance' if r.get('stance') else 'None',requires_meta_unlock=False,unlocked_by_default=True,stat_modifiers=mods,special_effects=[],card_artwork=art,icon=art,balance_parameters={k:float(v) for k,v in r.get('balance',{}).items()}))
            if r.get('balance'):
                unreal.SystemLibrary.execute_console_command(None,'setnopec '+a.get_path_name()+' bHasRuntimeBalance True')
                assert a.get_editor_property('has_runtime_balance')
            assert unreal.EditorAssetLibrary.save_loaded_asset(a,False)
        cards.append(a)
    copy(controller);ids={r['id'] for r in rows}
    pool=[a for a in comp.get_editor_property('upgrade_pool') if a and str(a.get_editor_property('upgrade_id')) not in ids]+cards
    comp.set_editor_property('upgrade_pool',pool);unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==digest for p,digest in preserved.items())
    backup.mkdir(parents=True,exist_ok=True);(backup/'preserved_card_hashes.json').write_text(json.dumps(preserved,indent=2))
pool=list(comp.get_editor_property('upgrade_pool'));ids=[str(a.get_editor_property('upgrade_id')) for a in pool if a]
assert len(ids)==len(pool)==len(set(ids))==57
for r in rows:
    assert ids.count(r['id'])==1
    a=unreal.load_asset(folder+'/DA_Upgrade_Ninja'+r['id'])
    assert a.get_editor_property('card_artwork') and a.get_editor_property('icon')
    assert set(str(k) for k in a.get_editor_property('prerequisite_upgrade_ids')).issubset(ids)
    if r.get('stance'):assert str(a.get_editor_property('exclusivity_group'))=='NinjaWeaponStance'
unreal.log('NINJA_BUILDS_OK: 20 cards, 3 exclusive stances, 57 unique pool entries; existing card tuning preserved')
