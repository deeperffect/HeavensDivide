"""Author Crescent cards; -ValidateCrescent is read-only. Preserve existing card tuning."""
from pathlib import Path
from datetime import datetime
import hashlib
import shutil
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
folder = '/Game/HeavensDivide/Upgrades/Samurai/'
controller = '/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
validate = '-ValidateCrescent' in unreal.SystemLibrary.get_command_line()
backup = root / 'Saved/Backups/CrescentBuild' / datetime.now().strftime('%Y%m%d_%H%M%S')
rows = [
    ('ReturningBlade', 'Returning Blade', 1, 'Common', [], {'ReturnDamageMultiplier': 0.5}, 'Waves return to their starting point, striking enemies again for 50% damage. Split waves do not return.'),
    ('CrescentSlowDuration', 'Lingering Chill', 5, 'Common', [], {'PerRank': 1.0}, 'Your waves slow enemies for 1 second longer.'),
    ('CrescentArcChance', 'Arc Mastery', 5, 'Rare', ['CrescentArc'], {'PerRank': 0.1}, '+10% chance to trigger Arc Volley.'),
    ('CrescentFieldChance', 'Restless Wake', 5, 'Rare', ['CrescentField'], {'PerRank': 0.05}, '+5% chance to trigger Lingering Wake.'),
 ('CrescentDoubleCut','Double Cut',1,'Common',[],{},'Every fourth attack sends 4 full-damage waves in a cross, each carrying your wave upgrades.'),
 ('CrescentSplit','Splitting Waves',1,'Common',[],{'Chance':.15,'DamageMultiplier':.5,'SizeMultiplier':.6,'RangeMultiplier':.6,'Angle':90.0},'The first enemy hit has a 15% chance to launch 2 smaller waves sideways while the original continues. Side waves deal 50% damage, have 60% width and range, and cannot split again.'),
 ('CrescentField','Lingering Wake',1,'Common',[],{'Chance':.15,'Duration':3.0,'DamagePerSecond':.3},'Waves have a 15% chance to leave a trail along their full path after travelling outward. Trails deal 30% wave damage per second for 3 seconds. Their split waves share the effect.'),
 ('CrescentAssist','Ninja Assist',1,'Common',[],{'Chance':.05},'Kills in Crescent Stance have a 5% chance to call Ninja for an assist attack.'),
 ('CrescentArc','Arc Volley',1,'Common',[],{'Chance':.15,'SideAngle':20.0},'Attacks have a 15% chance to fire a fan of 3 waves. With Double Cut, the fan fires in all four directions.'),
 ('CrescentDamage','Wave Damage',5,'Common',[],{'PerRank':.2},'Waves and their damaging trails deal 20% more damage.'),
 ('CrescentSpeed','Wave Speed',5,'Common',[],{'PerRank':.2},'Waves travel 20% faster.'),
 ('CrescentRange','Wave Range',5,'Common',[],{'PerRank':.2},'Waves travel 20% farther.'),
 ('CrescentSlow','Chilling Waves',5,'Common',[],{'PerRank':.05},'Waves slow enemies by an additional 5%.'),
 ('CrescentDoubleCutFrequency','Relentless Cuts',3,'Rare',['CrescentDoubleCut'],{},'Double Cut requires 1 fewer attack, down to every attack.'),
 ('CrescentFieldPower','Enduring Wake',5,'Rare',['CrescentField'],{'PerRank':.15},'+15% Lingering Wake duration and total damage. Also increases Sudden Eruption damage.'),
 ('CrescentSplitChance','Fracture Mastery',5,'Rare',['CrescentSplit'],{'PerRank':.15},'+15% chance to trigger Splitting Waves.'),
 ('CrescentAssistChance','Reinforcements',5,'Rare',['CrescentAssist'],{'PerRank':.05},'+5% chance to trigger Ninja Assist.'),
 ('CrescentFieldPact','Scorched Wake',1,'Rare',['CrescentField'],{},'Lingering Wake deals 50% more damage, but wave hits deal 30% less damage.'),
 ('CrescentPowerPact','Crushing Tide',1,'Rare',[],{},'Waves deal 50% more damage, but travel 50% slower.'),
 ('CrescentEruptionPact','Sudden Eruption',1,'Rare',['CrescentField'],{},'Your waves no longer slow enemies. Lingering Wake erupts after 0.3 seconds for all its damage at once. Your slow upgrades become Wave Damage.'),
]
shrine = {'CrescentFieldPact','CrescentPowerPact','CrescentEruptionPact'}

def disk(path):
    return root / 'Content' / (path.removeprefix('/Game/').split('.')[0] + '.uasset')
def back_up(path):
    source = disk(path)
    if source.exists():
        dest = backup / source.relative_to(root)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, dest)
def props(card, **values):
    for key, value in values.items(): card.set_editor_property(key, value)

bp = unreal.load_asset(controller)
upgrades = unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
pool = list(upgrades.get_editor_property('upgrade_pool'))
if not validate:
    changed = {row[0] for row in rows} | {'BladeWave'}
    preserved = {disk(a.get_path_name()):hashlib.sha256(disk(a.get_path_name()).read_bytes()).hexdigest()
                 for a in pool if a and str(a.get_editor_property('upgrade_id')) not in changed}
    back_up(controller)
    stance = unreal.load_asset(folder + 'DA_Upgrade_SamuraiCrescentStance')
    art = stance.get_editor_property('card_artwork')
    for uid, name, levels, rarity, requires, defaults, description in rows:
        path = folder + 'DA_Upgrade_Samurai' + uid
        back_up(path)
        card = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if not card:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property('data_asset_class', unreal.UpgradeDefinition)
            card = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_Upgrade_Samurai' + uid,
                folder.rstrip('/'), unreal.UpgradeDefinition, factory)
            props(card, icon=art, card_artwork=art)
        balance = {str(k):v for k,v in card.get_editor_property('balance_parameters').items()}
        for key,value in defaults.items(): balance.setdefault(key,value)
        if uid == 'CrescentField': balance.pop('Radius', None)  # Geometry comes from the spawning wave.
        props(card, upgrade_id=uid, display_name=name, description=description, max_level=levels,
            category=unreal.UpgradeCategory.CURSED if uid in shrine else unreal.UpgradeCategory.SAMURAI,
            investment_owner=unreal.UpgradeInvestmentOwner.SAMURAI,
            role=unreal.UpgradeRole.SUPPORT if levels>1 else unreal.UpgradeRole.MECHANIC,
            rarity=getattr(unreal.UpgradeRarity,rarity.upper()), uses_rolled_rarity=False,
            prerequisite_upgrade_ids=['BladeWave']+requires, balance_parameters=balance,
            requires_meta_unlock=False, unlocked_by_default=True)
        unreal.SystemLibrary.execute_console_command(None,'setnopec '+card.get_path_name()+' bHasRuntimeBalance True')
        if uid == 'CrescentField':
            unreal.SystemLibrary.execute_console_command(None,'setnopec '+card.get_path_name()+' bHasRuntimePresentation True')
        assert unreal.EditorAssetLibrary.save_loaded_asset(card,False)
        if card not in pool: pool.append(card)
    back_up(stance.get_path_name())
    balance = {str(k):v for k,v in stance.get_editor_property('balance_parameters').items()}
    balance.setdefault('SlowFraction',.3)
    balance.setdefault('SlowDuration',5.0)
    props(stance,balance_parameters=balance,description='Replace melee attacks with piercing waves that deal 40% attack damage and slow enemies by 30% for 5 seconds. Choose one Samurai stance per run.')
    assert unreal.EditorAssetLibrary.save_loaded_asset(stance,False)
    upgrades.set_editor_property('upgrade_pool',pool)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
    for path,digest in preserved.items():
        assert hashlib.sha256(path.read_bytes()).hexdigest()==digest,'Unrelated card changed: '+str(path)
    unreal.log('CRESCENT_BACKUP: '+str(backup))
ids=[str(a.get_editor_property('upgrade_id')) for a in pool if a]
assert len(ids)==len(set(ids))==156,(len(ids),len(set(ids)))
for uid,name,levels,rarity,requires,defaults,description in rows:
    card=next(a for a in pool if str(a.get_editor_property('upgrade_id'))==uid)
    assert card.get_editor_property('max_level')==levels
    assert [str(x) for x in card.get_editor_property('prerequisite_upgrade_ids')]==['BladeWave']+requires
    assert card.get_editor_property('card_artwork')
    assert (card.get_editor_property('category')==unreal.UpgradeCategory.CURSED)==(uid in shrine)
unreal.log('CRESCENT_OK: 156 unique cards, 20 Crescent upgrades; prerequisites, Shrine routing and unrelated tuning verified')
