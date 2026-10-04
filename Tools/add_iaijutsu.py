"""Add Iaijutsu and its pool reference without changing any existing card tuning."""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
row = next(r for r in json.loads((root / 'Tools/samurai_build_upgrades.json').read_text()) if r['id'] == 'Iaijutsu')
folder = '/Game/HeavensDivide/Upgrades/Samurai'
name = 'DA_Upgrade_SamuraiIaijutsuStance'
path = folder + '/' + name
controller = '/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
backup = root / 'Saved/Backups/Iaijutsu' / datetime.now().strftime('%Y%m%d_%H%M%S')
validate = '-ValidateIaijutsu' in unreal.SystemLibrary.get_command_line()
if not validate:
    for asset_path in (controller, path):
        relative = Path('Content') / (asset_path.removeprefix('/Game/') + '.uasset')
        if (root / relative).exists():
            (backup / relative).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(root / relative, backup / relative)
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.UpgradeDefinition)
        card = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.UpgradeDefinition, factory)
        art = unreal.load_asset('/Game/HeavensDivide/Blueprints/UI/CardArt2/Samurai/' + row['art'])
        assert art
        for key, value in dict(upgrade_id='Iaijutsu', display_name=row['name'], description=row['description'],
            category=unreal.UpgradeCategory.SAMURAI, investment_owner=unreal.UpgradeInvestmentOwner.SAMURAI,
            role=unreal.UpgradeRole.STARTER, max_level=1, rarity=unreal.UpgradeRarity.RARE,
            uses_rolled_rarity=False, exclusivity_group='SamuraiStance', requires_meta_unlock=False,
            unlocked_by_default=True, card_artwork=art, icon=art, balance_parameters=row['balance']).items():
            card.set_editor_property(key, value)
        unreal.SystemLibrary.execute_console_command(None, 'setnopec ' + card.get_path_name() + ' bHasRuntimeBalance True')
        assert unreal.EditorAssetLibrary.save_loaded_asset(card, False)
    card = unreal.load_asset(path)
    bp = unreal.load_asset(controller)
    component = unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
    pool = list(component.get_editor_property('upgrade_pool'))
    if not any(a and str(a.get_editor_property('upgrade_id')) == 'Iaijutsu' for a in pool):
        component.set_editor_property('upgrade_pool', pool + [card])
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    unreal.log('IAIJUTSU_BACKUP: ' + str(backup))

bp = unreal.load_asset(controller)
pool = unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component').get_editor_property('upgrade_pool')
ids = [str(a.get_editor_property('upgrade_id')) for a in pool if a]
assert len(ids) == len(pool) == len(set(ids)) == 106
assert ids.count('Iaijutsu') == 1
card = unreal.load_asset(path)
assert str(card.get_editor_property('exclusivity_group')) == 'SamuraiStance'
assert card.get_editor_property('max_level') == 1
assert card.get_editor_property('card_artwork')
assert {'DashDistance', 'ChargeDuration', 'Cooldown', 'SlashRadius'}.issubset(str(k) for k in card.get_editor_property('balance_parameters'))
unreal.log('IAIJUTSU_OK: 106 unique cards; exclusive Samurai stance with runtime tuning')
