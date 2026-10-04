"""Author Blood route cards and stance names. Run in Unreal; -ValidateBloodStance is read-only."""
from pathlib import Path
from datetime import datetime
import json
import shutil
import hashlib
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
folder = '/Game/HeavensDivide/Upgrades/Samurai/'
controller = '/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
validate = any(flag in unreal.SystemLibrary.get_command_line() for flag in ['-ValidateBloodStance','-ValidateSamuraiBuilds'])
backup = root / 'Saved/Backups/BloodStance' / datetime.now().strftime('%Y%m%d_%H%M%S')
rows = [
    ('BloodRush', 'Blood Rush', 1, 'Common', [], 'Killing a bleeding enemy while Samurai is active grants +20% movement speed for 3 seconds. Refreshes without stacking. Also triggers on lethal Blood Detonation; ends on swap.'),
 ('BloodTransfer','Blood Transfer',1,'Common',[], 'Bleeding enemies spread their stacks to nearby enemies on death.'),
 ('DoubleCut','Double Cut',1,'Common',[], 'Every fourth attack becomes a full-damage 360-degree slash.'),
 ('BloodEcho','Echoing Slash',1,'Common',[], '15% chance to echo an attack after a short delay for 50% damage. Echoes apply Bleed and can critically strike.'),
 ('BloodCritical','Critical Strike',1,'Common',[], 'Attacks have a 15% chance to critically strike for double damage.'),
 ('BloodAssist','Ninja Assist',1,'Common',[], 'Attacks have a 5% chance to call a Ninja assist.'),
 ('LingeringWounds','Bleed Duration',3,'Common',[], '+1 second Bleed duration per rank. Maximum 3 ranks.'),
 ('Bloodletting','Bleed Stacks per Hit',5,'Common',[], '+1 Bleed stack per hit per rank. Maximum 5 ranks.'),
 ('BloodCapacity','Max Bleed Stacks',5,'Rare',[], '+1 maximum Bleed stack per rank, up to 10 stacks.'),
 ('BloodTransferArea','Blood Transfer Area',5,'Rare',[], '+10% Blood Transfer radius per rank. Requires Blood Transfer or Blood Detonation.'),
 ('DoubleCutFrequency','Double Cut Frequency',3,'Rare',['DoubleCut'], 'One fewer attack between Double Cuts per rank: 3/2/1. Maximum 3 ranks.'),
 ('BloodCriticalChance','Critical Chance',5,'Rare',['BloodCritical'], '+10 percentage points critical strike chance per rank.'),
 ('BloodEchoChance','Echo Chance',5,'Rare',['BloodEcho'], '+10 percentage points echo chance per rank.'),
 ('BloodAssistChance','Assist Chance',5,'Rare',['BloodAssist'], '+5 percentage points Ninja assist chance per rank.'),
 ('BloodPactPower','Crimson Power',1,'Rare',[], '+70% Bleed damage, -30% attack speed. Blood Shrine reward.'),
 ('BloodPactSpeed','Frenzied Blood',1,'Rare',[], '+40% attack speed, -25% attack damage. Blood Shrine reward.'),
 ('BloodDetonation','Blood Detonation',1,'Rare',[], 'Reaching maximum Bleed stacks consumes them and explodes for 200% of their remaining damage. Disables Blood Transfer. Transfer area upgrades affect the explosion.'),
]
shrine = {'BloodPactPower','BloodPactSpeed','BloodDetonation'}
retired = {'BleedingEdge','DeepCuts'}

def save_backup(path):
    disk = root/'Content'/(path.removeprefix('/Game/')+'.uasset')
    if disk.exists():
        dest=backup/disk.relative_to(root);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(disk,dest)

def props(card, **values):
    for key,value in values.items():
        if key == 'has_runtime_balance':
            unreal.SystemLibrary.execute_console_command(None,'setnopec '+card.get_path_name()+' bHasRuntimeBalance '+str(value))
        else: card.set_editor_property(key,value)

def modifier(stat,value):
    m=unreal.UpgradeStatModifierDefinition()
    props(m,target=unreal.UpgradeStatTarget.SAMURAI,character_stat=getattr(unreal.CharacterStatType,stat),operation=unreal.StatModifierOperation.MULTIPLY,value_per_level=value)
    return m

bp=unreal.load_asset(controller)
upgrades=unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
pool=list(upgrades.get_editor_property('upgrade_pool'))
if not validate:
    changed_ids={row[0] for row in rows}|{'BattleStance','Iaijutsu','BladeWave','SplinterWave'}
    preserved={root/'Content'/(a.get_path_name().removeprefix('/Game/').split('.')[0]+'.uasset'):None
               for a in pool if a and str(a.get_editor_property('upgrade_id')) not in changed_ids}
    preserved={path:hashlib.sha256(path.read_bytes()).hexdigest() for path in preserved}
    save_backup(controller)
    for uid,name in [('BattleStance','Blood Stance'),('Iaijutsu','Iaijutsu Stance'),('BladeWave','Crescent Stance')]:
        path=folder+'DA_Upgrade_Samurai'+{'Iaijutsu':'IaijutsuStance','BladeWave':'CrescentStance'}.get(uid,uid);save_backup(path);card=unreal.load_asset(path)
        props(card,display_name=name)
        if uid=='BattleStance':
            old_bleed=unreal.load_asset(folder+'DA_Upgrade_SamuraiBleedingEdge')
            props(card,presentation=old_bleed.get_editor_property('presentation'))
            unreal.SystemLibrary.execute_console_command(None,'setnopec '+card.get_path_name()+' bHasRuntimePresentation True')
            balance=dict(card.get_editor_property('balance_parameters'));balance['BleedHitFraction']=.125
            props(card,description='Normal melee attacks gain 35% area and 30% attack speed and apply Bleed. Each stack deals 12.5% of hit damage over 3 seconds, ticking every 0.5 seconds. Maximum 5 stacks; all stacks refresh on every application.',has_runtime_balance=True,balance_parameters=balance)
        elif uid=='BladeWave':
            props(card,description='Replaces normal melee attacks with traveling Blade Waves, without a melee swing. Wave hits slow enemies by 30% for 5 seconds. Choose one Samurai stance per run. Cannot apply Bleed.')
        unreal.EditorAssetLibrary.save_loaded_asset(card,False)
    art=unreal.load_asset(folder+'DA_Upgrade_SamuraiDoubleCut').get_editor_property('card_artwork')
    for uid,name,levels,rarity,requires,description in rows:
        path=folder+'DA_Upgrade_Samurai'+uid;save_backup(path)
        card=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if not card:
            factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.UpgradeDefinition)
            card=unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_Upgrade_Samurai'+uid,folder.rstrip('/'),unreal.UpgradeDefinition,factory)
            props(card,icon=art,card_artwork=art)
        props(card,upgrade_id=uid,display_name=name,description=description,max_level=levels,
              category=unreal.UpgradeCategory.CURSED if uid in shrine else unreal.UpgradeCategory.SAMURAI,
              investment_owner=unreal.UpgradeInvestmentOwner.SAMURAI,
              role=unreal.UpgradeRole.SUPPORT if levels>1 else unreal.UpgradeRole.MECHANIC,
              rarity=getattr(unreal.UpgradeRarity,rarity.upper()),uses_rolled_rarity=False,
              prerequisite_upgrade_ids=['BattleStance']+requires,prerequisite_requirements=[],
              rarity_magnitudes=[],rolled_description_format='',stat_modifiers=[],
              requires_meta_unlock=False,unlocked_by_default=True)
        if uid=='BloodTransfer': props(card,has_runtime_balance=True,balance_parameters={'Radius':300.0})
        if uid=='BloodRush':
            balance={str(k):v for k,v in card.get_editor_property('balance_parameters').items()}
            balance.setdefault('MoveSpeedBonus',.2);balance.setdefault('Duration',3.0)
            props(card,has_runtime_balance=True,balance_parameters=balance)
        if uid=='BloodPactPower': props(card,stat_modifiers=[modifier('ATTACK_SPEED_MULTIPLIER',.7)])
        if uid=='BloodPactSpeed': props(card,stat_modifiers=[modifier('ATTACK_SPEED_MULTIPLIER',1.4),modifier('DAMAGE_MULTIPLIER',.75)])
        unreal.EditorAssetLibrary.save_loaded_asset(card,False)
        if card not in pool: pool.append(card)
    # Splinter remains a direct-damage wave burst; Bleed is exclusive to Blood.
    path=folder+'DA_Upgrade_SamuraiSplinterWave';save_backup(path);card=unreal.load_asset(path)
    props(card,description='The first enemy hit on each wave pass releases a small 30%-damage burst. Does not apply Bleed.')
    unreal.EditorAssetLibrary.save_loaded_asset(card,False)
    pool=[a for a in pool if a and str(a.get_editor_property('upgrade_id')) not in retired]
    upgrades.set_editor_property('upgrade_pool',pool)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
    # Reuse the existing authored Samurai slash effect for echoes.
    samurai='/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai'
    save_backup(samurai);sbp=unreal.load_asset(samurai);cdo=unreal.get_default_object(sbp.generated_class())
    attack=cdo.get_component_by_class(unreal.AutoAttackComponent)
    montage=unreal.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_AutoAttackSamurai')
    for event in unreal.AnimationLibrary.get_animation_notify_events(montage):
        notify=event.get_editor_property('notify')
        if isinstance(notify,unreal.AnimNotify_SpawnSamuraiSlashNiagara):
            effect=notify.get_editor_property('niagara_system')
            if effect:
                attack.set_editor_property('blood_echo_vfx',effect);break
    assert attack.get_editor_property('blood_echo_vfx'), 'Samurai slash effect must be assigned to echoes'
    unreal.BlueprintEditorLibrary.compile_blueprint(sbp)
    unreal.EditorAssetLibrary.save_loaded_asset(sbp,False)
    for path,digest in preserved.items():
        assert hashlib.sha256(path.read_bytes()).hexdigest()==digest, 'Unrelated card changed: '+str(path)
    (backup/'preserved_card_hashes.json').write_text(json.dumps({str(p.relative_to(root)):h for p,h in preserved.items()},indent=2))
    unreal.log('BLOOD_STANCE_BACKUP: '+str(backup))

ids=[str(a.get_editor_property('upgrade_id')) for a in pool if a]
assert len(ids)==len(set(ids))==106,(len(ids),len(set(ids)))
assert not retired.intersection(ids)
for uid,name,levels,rarity,requires,description in rows:
    card=next(a for a in pool if str(a.get_editor_property('upgrade_id'))==uid)
    assert card.get_editor_property('max_level')==levels
    assert 'BattleStance' in [str(p) for p in card.get_editor_property('prerequisite_upgrade_ids')]
for a in pool:
    assert set(str(p) for p in a.get_editor_property('prerequisite_upgrade_ids')).issubset(ids)
unreal.log('BLOOD_STANCE_OK: 106 cards; Blood-only upgrades and Shrine tradeoffs validated')
