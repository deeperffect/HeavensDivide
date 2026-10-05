"""Author Iaijutsu upgrades. -ValidateIaijutsuBuild is read-only; -UpdateIaijutsuCascade updates only its description. Existing tuning is preserved."""
from pathlib import Path
from datetime import datetime
import hashlib
import shutil
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
folder = '/Game/HeavensDivide/Upgrades/Samurai/'
controller = '/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
validate = '-ValidateIaijutsuBuild' in unreal.SystemLibrary.get_command_line()
backup = root / 'Saved/Backups/IaijutsuBuild' / datetime.now().strftime('%Y%m%d_%H%M%S')
rows = [
    ('IaijutsuVacuumReach', 'Gathering Wind', 5, 'Common', [], {'PerRank': 0.15}, 'Draw enemies into your Iaijutsu slash from 15% farther away.'),
    ('IaijutsuCascadePower', 'Deadly Cascade', 5, 'Rare', ['IaijutsuChain'], {'PerRank': 0.2}, 'Death Cascade slashes and their Final Flourish deal 20% more damage.'),
    ('IaijutsuDashPower', 'Driving Draw', 5, 'Rare', ['IaijutsuDash'], {'PerRank': 0.2}, 'Dash Draw slashes and their Final Flourish deal 20% more damage.'),
    ('IaijutsuChain', 'Death Cascade', 1, 'Common', [], {}, 'The first kill from an Iaijutsu attack launches a slash from the fallen enemy toward the nearest foe. This slash cannot trigger Death Cascade.'),
    ('IaijutsuDoubleCut', 'Double Cut', 1, 'Common', [], {}, 'Every fourth Iaijutsu attack releases 2 full-damage slashes in an X.'),
    ('IaijutsuInstant', 'Flash Draw', 1, 'Common', [], {'Chance': .15}, 'Iaijutsu attacks have a 15% chance to strike instantly, without charging.'),
    ('IaijutsuAOE', 'Final Flourish', 1, 'Common', [], {'Chance': .15, 'Radius': 250.0}, 'Iaijutsu attacks have a 15% chance to unleash a circular strike at the end of their path, dealing full attack damage within 2.5 meters.'),
    ('IaijutsuAssist', 'Ninja Assist', 1, 'Common', [], {'Chance': .05}, 'While fighting in Iaijutsu Stance, you have a 5% chance each second to call Ninja for an assist attack.'),
    ('IaijutsuDash', 'Dash Draw', 1, 'Common', [], {'DashDistanceBonus': 1.0}, 'Dash twice as far in Iaijutsu Stance. At the end of your dash, charge a slash along the path you travelled.'),
    ('IaijutsuDamage', 'Iaijutsu Damage', 5, 'Common', [], {'PerRank': .2}, 'Iaijutsu attacks deal 20% more damage.'),
    ('IaijutsuChargeSpeed', 'Charge Speed', 5, 'Common', [], {'PerRank': .15}, 'Iaijutsu slashes charge 15% faster.'),
    ('IaijutsuWidth', 'Slash Width', 5, 'Common', [], {'PerRank': .25}, 'Iaijutsu slashes are 25% wider.'),
    ('IaijutsuMarkDamage', 'Exposed Weakness', 5, 'Common', [], {'PerRank': .1}, 'Enemies marked by Iaijutsu take an additional 10% damage.'),
    ('IaijutsuDoubleCutFrequency', 'Relentless Cuts', 3, 'Rare', ['IaijutsuDoubleCut'], {}, 'Double Cut requires 1 fewer attack, down to every attack.'),
    ('IaijutsuAOEChance', 'Flourish Mastery', 5, 'Rare', ['IaijutsuAOE'], {'PerRank': .1}, '+10% chance to trigger Final Flourish.'),
    ('IaijutsuInstantChance', 'Flash Mastery', 5, 'Rare', ['IaijutsuInstant'], {'PerRank': .1}, '+10% chance to trigger Flash Draw.'),
    ('IaijutsuAssistChance', 'Reinforcements', 5, 'Rare', ['IaijutsuAssist'], {'PerRank': .05}, '+5% chance to trigger Ninja Assist.'),
    ('IaijutsuMarkPact', 'Focused Malice', 1, 'Rare', [], {}, 'Double the damage bonus from Iaijutsu marks, but your slashes are 20% narrower.'),
    ('IaijutsuPowerPact', 'Patient Blade', 1, 'Rare', [], {}, 'Iaijutsu slashes deal 50% more damage, but take 30% longer to charge.'),
    ('IaijutsuDashPact', 'Relentless Steps', 1, 'Rare', [], {}, 'Your Iaijutsu marks no longer amplify damage. Killing a marked enemy reduces dash recharge by 0.3 seconds instead. Exposed Weakness ranks become Iaijutsu Damage.'),
]
shrine = {'IaijutsuMarkPact', 'IaijutsuPowerPact', 'IaijutsuDashPact'}


def disk(path):
    return root / 'Content' / (path.removeprefix('/Game/').split('.')[0] + '.uasset')


def back_up(path):
    source = disk(path)
    if source.exists():
        destination = backup / source.relative_to(root)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


def props(card, **values):
    for key, value in values.items():
        card.set_editor_property(key, value)


bp = unreal.load_asset(controller)
upgrades = unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
pool = list(upgrades.get_editor_property('upgrade_pool'))
if not validate and '-UpdateIaijutsuCascade' in unreal.SystemLibrary.get_command_line():
    # A narrow migration for the one-follow-up rule; do not rewrite other cards or tuning.
    card = next(a for a in pool if a and str(a.get_editor_property('upgrade_id')) == 'IaijutsuChain')
    preserved = {disk(a.get_path_name()): hashlib.sha256(disk(a.get_path_name()).read_bytes()).hexdigest()
                 for a in pool if a and a != card}
    back_up(card.get_path_name())
    props(card, description=next(row[-1] for row in rows if row[0] == 'IaijutsuChain'))
    assert unreal.EditorAssetLibrary.save_loaded_asset(card, False)
    for path, digest in preserved.items():
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, 'Unrelated card changed: ' + str(path)
    unreal.log('IAIJUTSU_CASCADE_BACKUP: ' + str(backup))
elif not validate:
    changed = {row[0] for row in rows} | {'Iaijutsu'}
    preserved = {disk(a.get_path_name()): hashlib.sha256(disk(a.get_path_name()).read_bytes()).hexdigest()
                 for a in pool if a and str(a.get_editor_property('upgrade_id')) not in changed}
    back_up(controller)
    art = unreal.load_asset(folder + 'DA_Upgrade_SamuraiIaijutsuStance').get_editor_property('card_artwork')
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
        balance = {str(k): v for k, v in card.get_editor_property('balance_parameters').items()}
        for key, value in defaults.items():
            balance.setdefault(key, value)
        props(card, upgrade_id=uid, display_name=name, description=description, max_level=levels,
              category=unreal.UpgradeCategory.CURSED if uid in shrine else unreal.UpgradeCategory.SAMURAI,
              investment_owner=unreal.UpgradeInvestmentOwner.SAMURAI,
              role=unreal.UpgradeRole.SUPPORT if levels > 1 else unreal.UpgradeRole.MECHANIC,
              rarity=getattr(unreal.UpgradeRarity, rarity.upper()), uses_rolled_rarity=False,
              prerequisite_upgrade_ids=['Iaijutsu'] + requires, balance_parameters=balance,
              requires_meta_unlock=False, unlocked_by_default=True)
        unreal.SystemLibrary.execute_console_command(None, 'setnopec ' + card.get_path_name() + ' bHasRuntimeBalance True')
        assert unreal.EditorAssetLibrary.save_loaded_asset(card, False)
        if card not in pool:
            pool.append(card)
    path = folder + 'DA_Upgrade_SamuraiIaijutsuStance'
    back_up(path)
    card = unreal.load_asset(path)
    balance = {str(k): v for k, v in card.get_editor_property('balance_parameters').items()}
    for key, value in {'VacuumReach': 60.0, 'VacuumSpeed': 240.0, 'MarkBonus': .5, 'MarkDuration': 3.0}.items():
        balance.setdefault(key, value)
    props(card, balance_parameters=balance, description='Charge a slash for 1 second, drawing nearby enemies into its path. Strike the entire path and mark foes for 3 seconds. Marked foes take 50% more damage from later hits. Choose one Samurai stance per run.')
    assert unreal.EditorAssetLibrary.save_loaded_asset(card, False)
    upgrades.set_editor_property('upgrade_pool', pool)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    for path, digest in preserved.items():
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, 'Unrelated tuning changed: ' + str(path)
    unreal.log('IAIJUTSU_BUILD_BACKUP: ' + str(backup))

ids = [str(a.get_editor_property('upgrade_id')) for a in pool if a]
assert len(ids) == len(set(ids)) == 156, (len(ids), len(set(ids)))
for uid, name, levels, rarity, requires, defaults, description in rows:
    card = next(a for a in pool if str(a.get_editor_property('upgrade_id')) == uid)
    if uid == 'IaijutsuChain':
        assert str(card.get_editor_property('description')) == description
    assert card.get_editor_property('max_level') == levels
    assert [str(x) for x in card.get_editor_property('prerequisite_upgrade_ids')] == ['Iaijutsu'] + requires
    assert card.get_editor_property('card_artwork')
    assert (card.get_editor_property('category') == unreal.UpgradeCategory.CURSED) == (uid in shrine)
unreal.log('IAIJUTSU_BUILD_OK: 156 unique cards, 17 Iaijutsu upgrades, prerequisites and Shrine routing validated')
