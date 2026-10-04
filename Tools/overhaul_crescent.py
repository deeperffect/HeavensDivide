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
    ('ReturningBlade', 'Returning Blade', 1, 'Common', [], {'ReturnDamageMultiplier': 0.5}, 'Primary waves return to their launch point, hitting each enemy once more for 50% damage. Split waves do not return. Each wave keeps one split roll and one field opportunity across both passes.'),
    ('CrescentSlowDuration', 'Slow Duration', 5, 'Common', [], {'PerRank': 1.0}, '+1 second Crescent slow duration per rank. Maximum 5 ranks: base 5 seconds reaches 10 seconds. Unavailable with Sudden Eruption; owned ranks convert into Wave Damage.'),
    ('CrescentArcChance', 'Arc Volley Chance', 5, 'Rare', ['CrescentArc'], {'PerRank': 0.1}, '+10 percentage points Arc Volley chance per rank. Maximum 5 ranks: base 15% reaches 65%.'),
    ('CrescentFieldChance', 'Wake Chance', 5, 'Rare', ['CrescentField'], {'PerRank': 0.05}, '+5 percentage points damaging field chance per primary wave per rank. Maximum 5 ranks: base 15% reaches 40%. Split waves inherit their parent result; returning waves do not reroll.'),
 ('CrescentDoubleCut','Double Cut',1,'Common',[],{},'Every fourth committed attack fires four full-damage waves in a plus pattern. Waves retain split, field and arc upgrades.'),
 ('CrescentSplit','Splitting Waves',1,'Common',[],{'Chance':.15,'DamageMultiplier':.5,'SizeMultiplier':.6,'RangeMultiplier':.6,'Angle':35.0},'15% chance on a wave\'s first hit to split into two smaller waves. Each deals 50% damage, with 60% width and range. Split waves cannot split again.'),
 ('CrescentField','Lingering Wake',1,'Common',[],{'Chance':.15,'Duration':3.0,'DamagePerSecond':.3},'15% chance per primary wave to leave a damaging strip covering its whole travelled path, with the wave\'s width. Appears when outbound travel ends. Lasts 3 seconds at 30% wave damage per second. Split waves inherit the parent result and leave their own full-path strips; returns do not duplicate it.'),
 ('CrescentAssist','Ninja Assist',1,'Common',[],{'Chance':.05},'Kills while Samurai is active in Crescent Stance have a 5% chance to call a Ninja assist. Busy assists cannot overlap.'),
 ('CrescentArc','Arc Volley',1,'Common',[],{'Chance':.15,'SideAngle':20.0},'Attacks have a 15% chance to fire three waves in a 40-degree arc. Double Cut fires a fan in each of its four directions.'),
 ('CrescentDamage','Wave Damage',5,'Common',[],{'PerRank':.2},'+20% wave damage per rank. Also increases the damage basis of ground fields. Maximum 5 ranks.'),
 ('CrescentSpeed','Wave Speed',5,'Common',[],{'PerRank':.2},'+20% wave travel speed per rank. Does not change attack frequency. Maximum 5 ranks.'),
 ('CrescentRange','Wave Range',5,'Common',[],{'PerRank':.2},'+20% wave travel range per rank. Maximum 5 ranks.'),
 ('CrescentSlow','Increased Slow',5,'Common',[],{'PerRank':.05},'+5 percentage points slow per rank. Maximum 5 ranks; base 30% reaches 55%. Unavailable with Sudden Eruption.'),
 ('CrescentDoubleCutFrequency','Double Cut Frequency',3,'Rare',['CrescentDoubleCut'],{},'One fewer attack between Double Cuts per rank: 3/2/1. Maximum 3 ranks.'),
 ('CrescentFieldPower','Field Duration & Damage',5,'Rare',['CrescentField'],{'PerRank':.15},'+15% ground field duration and total damage per rank. Damage per second is unchanged. Maximum 5 ranks: +75% duration and total damage. Also scales eruptions.'),
 ('CrescentSplitChance','Split Chance',5,'Rare',['CrescentSplit'],{'PerRank':.15},'+15 percentage points wave split chance per rank. Maximum 5 ranks.'),
 ('CrescentAssistChance','Assist Chance',5,'Rare',['CrescentAssist'],{'PerRank':.05},'+5 percentage points assist chance per kill per rank. Maximum 5 ranks.'),
 ('CrescentFieldPact','Scorched Wake',1,'Rare',['CrescentField'],{},'+50% ground field damage, -30% direct wave damage. Crescent Blood Shrine reward.'),
 ('CrescentPowerPact','Crushing Tide',1,'Rare',[],{},'+50% direct wave damage, -50% wave travel speed. Crescent Blood Shrine reward.'),
 ('CrescentEruptionPact','Sudden Eruption',1,'Rare',['CrescentField'],{},'Waves stop slowing. Fields erupt after 0.3s for their full total damage. Converts owned Increased Slow and Slow Duration ranks into Wave Damage. Crescent Blood Shrine reward.'),
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
    props(stance,balance_parameters=balance,description='Replaces normal melee attacks with traveling Blade Waves, without a melee swing. Wave hits slow enemies by 30% for 5 seconds. Choose one Samurai stance per run. Cannot apply Bleed.')
    assert unreal.EditorAssetLibrary.save_loaded_asset(stance,False)
    upgrades.set_editor_property('upgrade_pool',pool)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
    for path,digest in preserved.items():
        assert hashlib.sha256(path.read_bytes()).hexdigest()==digest,'Unrelated card changed: '+str(path)
    unreal.log('CRESCENT_BACKUP: '+str(backup))
ids=[str(a.get_editor_property('upgrade_id')) for a in pool if a]
assert len(ids)==len(set(ids))==106,(len(ids),len(set(ids)))
for uid,name,levels,rarity,requires,defaults,description in rows:
    card=next(a for a in pool if str(a.get_editor_property('upgrade_id'))==uid)
    assert card.get_editor_property('max_level')==levels
    assert [str(x) for x in card.get_editor_property('prerequisite_upgrade_ids')]==['BladeWave']+requires
    assert card.get_editor_property('card_artwork')
    assert (card.get_editor_property('category')==unreal.UpgradeCategory.CURSED)==(uid in shrine)
unreal.log('CRESCENT_OK: 106 unique cards, 20 Crescent upgrades; prerequisites, Shrine routing and unrelated tuning verified')
