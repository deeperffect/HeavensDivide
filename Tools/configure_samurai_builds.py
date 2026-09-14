"""Author Samurai stances and mixable attack upgrades. -ValidateSamuraiBuilds is read-only."""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
rows=json.loads((root/'Tools/samurai_build_upgrades.json').read_text())
controller='/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
folder='/Game/HeavensDivide/Upgrades/Samurai'
validate='-ValidateSamuraiBuilds' in unreal.SystemLibrary.get_command_line()
backup=root/'Saved/Backups/SamuraiBuilds'/datetime.now().strftime('%Y%m%d_%H%M%S')
def backup_asset(path):
    relative=Path('Content')/(path.removeprefix('/Game/')+'.uasset')
    if (root/relative).exists():
        (backup/relative).parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(root/relative,backup/relative)
def props(asset,values):
    for k,v in values.items():asset.set_editor_property(k,v)
def modifier(stat,value,operation):
    m=unreal.UpgradeStatModifierDefinition()
    props(m,dict(target=unreal.UpgradeStatTarget.SAMURAI,character_stat=getattr(unreal.CharacterStatType,{'AttackAreaMultiplier':'ATTACK_AREA_MULTIPLIER','AttackSpeedMultiplier':'ATTACK_SPEED_MULTIPLIER','DamageMultiplier':'DAMAGE_MULTIPLIER'}[stat]),operation=operation,value_per_level=value))
    return m

if not validate:
    factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.UpgradeDefinition)
    cards=[]
    for row in rows:
        name='DA_Upgrade_Samurai'+row['id'];path=folder+'/'+name;backup_asset(path)
        a=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.UpgradeDefinition,factory)
        art=unreal.load_asset('/Game/HeavensDivide/Blueprints/UI/CardArt2/Samurai/'+row['art']);assert art
        modifiers=[modifier(k,v,unreal.StatModifierOperation.MULTIPLY) for k,v in row.get('stance',{}).items()]
        if 'stat' in row:modifiers.append(modifier(row['stat'],row['magnitudes'][0],unreal.StatModifierOperation.ADD_PERCENT))
        props(a,dict(upgrade_id=row['id'],display_name=row['name'],description=row['description'],category=unreal.UpgradeCategory.SAMURAI,investment_owner=unreal.UpgradeInvestmentOwner.SAMURAI,role=getattr(unreal.UpgradeRole,row['role'].upper()),build_family_id='None',max_level=row.get('levels',1),rarity=unreal.UpgradeRarity.COMMON if 'magnitudes' in row else unreal.UpgradeRarity.RARE,uses_rolled_rarity='magnitudes' in row,prerequisite_upgrade_ids=row.get('requires',[]),prerequisite_requirements=[],exclusivity_group='SamuraiStance' if 'stance' in row else 'None',requires_meta_unlock=False,unlocked_by_default=True,stat_modifiers=modifiers,special_effects=[],card_artwork=art,icon=art))
        mags=[]
        for rarity,value in zip([unreal.UpgradeRarity.COMMON,unreal.UpgradeRarity.RARE,unreal.UpgradeRarity.EPIC],row.get('magnitudes',[])):
            m=unreal.UpgradeRarityMagnitude();props(m,dict(rarity=rarity,magnitude=value));mags.append(m)
        props(a,dict(rarity_magnitudes=mags,rolled_description_format=row.get('format','')))
        current=dict(a.get_editor_property('balance_parameters'));keys={str(k) for k in current}
        for k,v in row.get('balance',{}).items():
            if k not in keys:current[k]=float(v)
        a.set_editor_property('balance_parameters',current)
        unreal.SystemLibrary.execute_console_command(None,'setnopec '+a.get_path_name()+' bHasRuntimeBalance '+('True' if current else 'False'))
        assert unreal.EditorAssetLibrary.save_loaded_asset(a,False);cards.append(a)
    path=folder+'/DA_Upgrade_SamuraiBleedingEdge';backup_asset(path);bleed=unreal.load_asset(path)
    bleed.set_editor_property('description','Normal Samurai melee and Blade Wave hits apply Bleed. Each new stack adds 10% of the applying hit damage per tick to its base Bleed damage. Bleed support upgrades scale both contributions. Assists retain their intrinsic status behavior.')
    current=dict(bleed.get_editor_property('balance_parameters'))
    if 'HitDamagePerTick' not in {str(k) for k in current}:current['HitDamagePerTick']=0.1
    bleed.set_editor_property('balance_parameters',current)
    unreal.SystemLibrary.execute_console_command(None,'setnopec '+bleed.get_path_name()+' bHasRuntimeBalance True')
    assert unreal.EditorAssetLibrary.save_loaded_asset(bleed,False)
    backup_asset(controller);bp=unreal.load_asset(controller)
    comp=unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
    ids={r['id'] for r in rows}
    pool=[a for a in comp.get_editor_property('upgrade_pool') if a and str(a.get_editor_property('upgrade_id')) not in ids]+cards
    comp.set_editor_property('upgrade_pool',pool);unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
    unreal.log('SAMURAI_BUILDS_BACKUP: '+str(backup))

bp=unreal.load_asset(controller);pool=unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component').get_editor_property('upgrade_pool')
ids=[str(a.get_editor_property('upgrade_id')) for a in pool if a]
assert len(ids)==len(pool)==len(set(ids))==57
for row in rows:
    assert ids.count(row['id'])==1
    a=unreal.load_asset(folder+'/DA_Upgrade_Samurai'+row['id'])
    assert a.get_editor_property('card_artwork') and a.get_editor_property('icon')
    assert set(str(k) for k in a.get_editor_property('prerequisite_upgrade_ids'))==set(row.get('requires',[]))
    assert a.get_editor_property('max_level')==row.get('levels',1)
    if 'stance' in row:
        assert str(a.get_editor_property('exclusivity_group'))=='SamuraiStance'
        assert len(a.get_editor_property('stat_modifiers'))==3
        assert all(m.get_editor_property('operation')==unreal.StatModifierOperation.MULTIPLY for m in a.get_editor_property('stat_modifiers'))
for a in pool:
    assert set(str(k) for k in a.get_editor_property('prerequisite_upgrade_ids')).issubset(ids)
unreal.log('SAMURAI_BUILDS_OK: 3 exclusive stances, 7 mixable upgrades, 57 unique pool entries, valid prerequisites and artwork')
if not validate:
    text_script=root/'Tools/shorten_samurai_descriptions.py'
    exec(compile(text_script.read_text(),str(text_script),'exec'),{'__name__':'__main__'})
