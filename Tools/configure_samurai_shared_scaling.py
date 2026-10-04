"""Make the pre-stance Damage/Speed/Area investments available. -ValidateSamuraiSharedScaling is read-only."""
from datetime import datetime
from pathlib import Path
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
folder = '/Game/HeavensDivide/Upgrades/Samurai/'
validate = '-ValidateSamuraiSharedScaling' in u.SystemLibrary.get_command_line()
damage = u.load_asset(folder + 'DA_Upgrade_SamuraiHeavyBlade')
speed = u.load_asset(folder + 'DA_Upgrade_SamuraiSamuraiTempo')
area = u.load_asset(folder + 'DA_Upgrade_SamuraiArea')
assert damage and speed and area
if not validate:
    backup = root / 'Saved/Backups/SamuraiSharedScaling' / datetime.now().strftime('%Y%m%d_%H%M%S')
    backup.mkdir(parents=True, exist_ok=True)
    for card in [damage, speed]:
        name = card.get_name()
        shutil.copy2(root / ('Content/HeavensDivide/Upgrades/Samurai/' + name + '.uasset'), backup / (name + '.uasset'))
    modifier = u.UpgradeStatModifierDefinition()
    modifier.set_editor_property('target', u.UpgradeStatTarget.SAMURAI)
    modifier.set_editor_property('character_stat', u.CharacterStatType.DAMAGE_MULTIPLIER)
    modifier.set_editor_property('operation', u.StatModifierOperation.ADD_PERCENT)
    modifier.set_editor_property('value_per_level', .2)
    magnitudes = []
    for rarity, amount in [(u.UpgradeRarity.COMMON, .2), (u.UpgradeRarity.RARE, .3), (u.UpgradeRarity.EPIC, .45)]:
        entry = u.UpgradeRarityMagnitude()
        entry.set_editor_property('rarity', rarity)
        entry.set_editor_property('magnitude', amount)
        magnitudes.append(entry)
    for key, value in {
        'display_name': 'Attack Damage', 'description': '+20% Samurai attack damage. Converts to the chosen stance\'s damage upgrade.',
        'category': u.UpgradeCategory.SAMURAI, 'role': u.UpgradeRole.SUPPORT,
        'investment_owner': u.UpgradeInvestmentOwner.SAMURAI,
        'max_level': 5, 'uses_rolled_rarity': True, 'rarity_magnitudes': magnitudes,
        'stat_modifiers': [modifier], 'prerequisite_upgrade_ids': [], 'prerequisite_requirements': [],
        'rolled_description_format': '+{Percent}% Samurai attack damage. Converts to the chosen stance\'s damage upgrade.',
    }.items():
        damage.set_editor_property(key, value)
    speed.set_editor_property('display_name', 'Attack Speed')
    for card in [damage, speed]:
        assert u.EditorAssetLibrary.save_loaded_asset(card, False)
    u.log('SAMURAI_SHARED_SCALING_BACKUP: ' + str(backup))
for card in [damage, speed, area]:
    assert card.get_editor_property('category') == u.UpgradeCategory.SAMURAI
    assert card.get_editor_property('max_level') == 5
    assert card.get_editor_property('investment_owner') == u.UpgradeInvestmentOwner.SAMURAI
    assert not card.get_editor_property('prerequisite_upgrade_ids')
assert len(damage.get_editor_property('stat_modifiers')) == 1
assert damage.get_editor_property('uses_rolled_rarity')
u.log('SAMURAI_SHARED_SCALING_OK: Damage, Attack Speed and Area are ordinary five-rank investments')
