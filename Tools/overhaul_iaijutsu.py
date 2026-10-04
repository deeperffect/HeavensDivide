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
    ('IaijutsuVacuumReach', 'Vacuum Reach', 5, 'Common', [], {'PerRank': 0.15}, '+15% Iaijutsu vacuum reach per rank. Maximum 5 ranks: base 60 cm reaches 105 cm. Does not increase the slash hitbox.'),
    ('IaijutsuCascadePower', 'Cascade Power', 5, 'Rare', ['IaijutsuChain'], {'PerRank': 0.2}, '+20% Death Cascade attack damage per rank, including its crossing lanes and Endpoint Burst. Maximum 5 ranks. Cascaded attacks cannot cascade again.'),
    ('IaijutsuDashPower', 'Dash Draw Power', 5, 'Rare', ['IaijutsuDash'], {'PerRank': 0.2}, '+20% Dash Draw attack damage per rank, including its crossing lanes and Endpoint Burst. Maximum 5 ranks. Does not increase normal or cascaded attack damage.'),
    ('IaijutsuChain', 'Death Cascade', 1, 'Common', [], {}, 'On an Iaijutsu kill, launch another slash from the victim toward the nearest enemy. Cascaded attacks cannot trigger another cascade.'),
    ('IaijutsuDoubleCut', 'Double Cut', 1, 'Common', [], {}, 'Every fourth Iaijutsu attack becomes two full-damage crossing slashes in an X.'),
    ('IaijutsuInstant', 'Flash Draw', 1, 'Common', [], {'Chance': .15}, 'Iaijutsu attacks have a 15% chance to resolve instantly without charging.'),
    ('IaijutsuAOE', 'Endpoint Burst', 1, 'Common', [], {'Chance': .15, 'Radius': 250.0}, '15% chance for a full-damage circular attack at the end of the Iaijutsu lane. Base radius 250 cm.'),
    ('IaijutsuAssist', 'Ninja Assist', 1, 'Common', [], {'Chance': .05}, 'Every second, 5% chance to call Ninja\'s equipped assist while Samurai autoattacks are active. Busy assists cannot overlap.'),
    ('IaijutsuDash', 'Dash Draw', 1, 'Common', [], {'DashDistanceBonus': 1.0}, '+100% Samurai dash distance in Iaijutsu Stance. Completing a dash charges an Iaijutsu slash along the path from its starting position to its actual endpoint.'),
    ('IaijutsuDamage', 'Iaijutsu Damage', 5, 'Common', [], {'PerRank': .2}, '+20% Iaijutsu damage per rank. Maximum 5 ranks.'),
    ('IaijutsuChargeSpeed', 'Iaijutsu Charge Speed', 5, 'Common', [], {'PerRank': .15}, '+15% Iaijutsu charge speed per rank. Maximum 5 ranks. Charge duration is divided by total charge speed.'),
    ('IaijutsuWidth', 'Iaijutsu Width', 5, 'Common', [], {'PerRank': .25}, '+25% Iaijutsu lane width per rank. Maximum 5 ranks.'),
    ('IaijutsuMarkDamage', 'Mark Damage', 5, 'Common', [], {'PerRank': .1}, '+10 percentage points damage taken by Iaijutsu-marked enemies per rank. Maximum 5 ranks. Unavailable with Relentless Steps.'),
    ('IaijutsuDoubleCutFrequency', 'Double Cut Frequency', 3, 'Rare', ['IaijutsuDoubleCut'], {}, 'One fewer attack between Double Cuts per rank: 3/2/1. Maximum 3 ranks.'),
    ('IaijutsuAOEChance', 'Endpoint Burst Chance', 5, 'Rare', ['IaijutsuAOE'], {'PerRank': .1}, '+10 percentage points Endpoint Burst chance per rank. Maximum 5 ranks.'),
    ('IaijutsuInstantChance', 'Instant Cast Chance', 5, 'Rare', ['IaijutsuInstant'], {'PerRank': .1}, '+10 percentage points instant cast chance per rank. Maximum 5 ranks.'),
    ('IaijutsuAssistChance', 'Assist Chance', 5, 'Rare', ['IaijutsuAssist'], {'PerRank': .05}, '+5 percentage points assist chance per second per rank. Maximum 5 ranks.'),
    ('IaijutsuMarkPact', 'Focused Malice', 1, 'Rare', [], {}, '+100% mark damage bonus (doubles its vulnerability), -20% Iaijutsu width. Cannot combine with Relentless Steps. Iaijutsu Blood Shrine reward.'),
    ('IaijutsuPowerPact', 'Patient Blade', 1, 'Rare', [], {}, '+50% Iaijutsu attack damage, +30% charge duration (slower charging). Iaijutsu Blood Shrine reward.'),
    ('IaijutsuDashPact', 'Relentless Steps', 1, 'Rare', [], {}, 'Marks lose vulnerability; marked kills refund 0.3s dash recharge. Converts owned Mark Damage ranks into Iaijutsu Damage. Cannot combine with Focused Malice. Iaijutsu Blood Shrine reward.'),
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
    props(card, balance_parameters=balance, description='Charge a 200 cm-wide spectral lane for 1 second, gently pulling nearby enemies into it. The lane hits instantly after charging and marks enemies for 3 seconds, making them take 50% more damage from subsequent hits. Choose one Samurai stance per run.')
    assert unreal.EditorAssetLibrary.save_loaded_asset(card, False)
    upgrades.set_editor_property('upgrade_pool', pool)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    for path, digest in preserved.items():
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, 'Unrelated tuning changed: ' + str(path)
    unreal.log('IAIJUTSU_BUILD_BACKUP: ' + str(backup))

ids = [str(a.get_editor_property('upgrade_id')) for a in pool if a]
assert len(ids) == len(set(ids)) == 106, (len(ids), len(set(ids)))
for uid, name, levels, rarity, requires, defaults, description in rows:
    card = next(a for a in pool if str(a.get_editor_property('upgrade_id')) == uid)
    if uid == 'IaijutsuChain':
        assert str(card.get_editor_property('description')) == description
    assert card.get_editor_property('max_level') == levels
    assert [str(x) for x in card.get_editor_property('prerequisite_upgrade_ids')] == ['Iaijutsu'] + requires
    assert card.get_editor_property('card_artwork')
    assert (card.get_editor_property('category') == unreal.UpgradeCategory.CURSED) == (uid in shrine)
unreal.log('IAIJUTSU_BUILD_OK: 106 unique cards, 17 Iaijutsu upgrades, prerequisites and Shrine routing validated')
