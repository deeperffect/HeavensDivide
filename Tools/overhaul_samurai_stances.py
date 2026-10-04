"""Replace retired Samurai stances; preserve all unrelated saved card tuning.

Run in Unreal Python. -ValidateSamuraiStances performs read-only validation.
"""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
folder = '/Game/HeavensDivide/Upgrades/Samurai/'
controller = '/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
retired = {'BloodStance', 'ExecutionStance', 'WaveStance'}
paths = [folder + 'DA_Upgrade_Samurai' + uid for uid in sorted(retired)]
wave_path = folder + 'DA_Upgrade_SamuraiCrescentStance'
battle_path = folder + 'DA_Upgrade_SamuraiBattleStance'
validate = '-ValidateSamuraiStances' in unreal.SystemLibrary.get_command_line()
backup = root / 'Saved/Backups/SamuraiStanceOverhaul' / datetime.now().strftime('%Y%m%d_%H%M%S')

def disk(path):
    return root / 'Content' / (path.removeprefix('/Game/') + '.uasset')

if not validate:
    changed = {disk(p) for p in paths + [wave_path, battle_path]}
    hashes = {p: hashlib.sha256(p.read_bytes()).hexdigest()
              for p in (root / 'Content/HeavensDivide/Upgrades').rglob('*.uasset') if p not in changed}
    for path in paths + [wave_path, battle_path, controller]:
        source = disk(path)
        if source.exists():
            dest = backup / source.relative_to(root)
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, dest)
    if not unreal.EditorAssetLibrary.does_asset_exist(battle_path):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.UpgradeDefinition)
        card = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_Upgrade_SamuraiBattleStance', folder.rstrip('/'), unreal.UpgradeDefinition, factory)
        row = next(r for r in json.loads((root / 'Tools/samurai_build_upgrades.json').read_text()) if r['id'] == 'BattleStance')
        art = unreal.load_asset('/Game/HeavensDivide/Blueprints/UI/CardArt2/Samurai/' + row['art'])
        assert art
        modifiers = []
        for stat, value in [(unreal.CharacterStatType.ATTACK_AREA_MULTIPLIER, 1.35), (unreal.CharacterStatType.ATTACK_SPEED_MULTIPLIER, 1.3)]:
            modifier = unreal.UpgradeStatModifierDefinition()
            for key, val in dict(target=unreal.UpgradeStatTarget.SAMURAI, character_stat=stat,
                operation=unreal.StatModifierOperation.MULTIPLY, value_per_level=value).items():
                modifier.set_editor_property(key, val)
            modifiers.append(modifier)
        for key, value in dict(upgrade_id='BattleStance', display_name=row['name'], description=row['description'],
            category=unreal.UpgradeCategory.SAMURAI, investment_owner=unreal.UpgradeInvestmentOwner.SAMURAI,
            role=unreal.UpgradeRole.STARTER, rarity=unreal.UpgradeRarity.RARE, max_level=1,
            exclusivity_group='SamuraiStance', requires_meta_unlock=False, unlocked_by_default=True,
            stat_modifiers=modifiers, card_artwork=art, icon=art).items():
            card.set_editor_property(key, value)
        assert unreal.EditorAssetLibrary.save_loaded_asset(card, False)
    battle = unreal.load_asset(battle_path)
    wave = unreal.load_asset(wave_path)
    for key, value in dict(exclusivity_group='SamuraiStance', rarity=unreal.UpgradeRarity.RARE,
        max_level=1, role=unreal.UpgradeRole.STARTER, uses_rolled_rarity=False,
        requires_meta_unlock=False, unlocked_by_default=True,
        description='Stance: replaces normal melee attacks with automatically launched Blade Waves, without a melee swing. Wave hits slow enemies by 30% for 5 seconds. Choose one Samurai stance per run.').items():
        wave.set_editor_property(key, value)
    assert unreal.EditorAssetLibrary.save_loaded_asset(wave, False)
    bp = unreal.load_asset(controller)
    upgrades = unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
    pool = [a for a in upgrades.get_editor_property('upgrade_pool') if a and str(a.get_editor_property('upgrade_id')) not in retired | {'BattleStance'}]
    upgrades.set_editor_property('upgrade_pool', pool + [battle])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    for path in paths:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unexpected = set(str(r) for r in unreal.EditorAssetLibrary.find_package_referencers_for_asset(path, True)) - {controller}
            assert not unexpected, (path, unexpected)
            assert unreal.EditorAssetLibrary.delete_asset(path), path
    for path, digest in hashes.items():
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, path
    (backup / 'preserved_card_hashes.json').write_text(json.dumps({str(p.relative_to(root)): h for p, h in hashes.items()}, indent=2))
    unreal.log('SAMURAI_STANCE_BACKUP: ' + str(backup))

bp = unreal.load_asset(controller)
pool = unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component').get_editor_property('upgrade_pool')
ids = [str(a.get_editor_property('upgrade_id')) for a in pool if a]
assert len(ids) == len(pool) == len(set(ids)) == 106
assert not retired.intersection(ids)
assert all(not unreal.EditorAssetLibrary.does_asset_exist(path) for path in paths)
stances = {str(a.get_editor_property('upgrade_id')) for a in pool if str(a.get_editor_property('exclusivity_group')) == 'SamuraiStance'}
assert stances == {'BladeWave', 'Iaijutsu', 'BattleStance'}
for a in pool:
    assert set(str(p) for p in a.get_editor_property('prerequisite_upgrade_ids')).issubset(ids)
unreal.log('SAMURAI_STANCES_OK: Blade Wave, Iaijutsu, Battle Stance; 106 cards; unrelated tuning preserved')
