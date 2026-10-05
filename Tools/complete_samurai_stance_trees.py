"""Complete the three 21-card Samurai trees, preserving existing tuning. -ValidateSamuraiTrees is read-only."""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import shutil
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
catalog = json.loads((root / 'Tools/samurai_stance_expansion.json').read_text(encoding='utf-8'))
folder = '/Game/HeavensDivide/Upgrades/Samurai/'
controller = '/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
validate = '-ValidateSamuraiTrees' in unreal.SystemLibrary.get_command_line()
backup = root / 'Saved/Backups/SamuraiStanceTrees' / datetime.now().strftime('%Y%m%d_%H%M%S')
rows = catalog['cards']
paths = {row['id']: folder + 'DA_Upgrade_Samurai' + row['id'] for row in rows}
pact_path = folder + 'DA_Upgrade_SamuraiCrescentEruptionPact'
pact_text = 'Your waves no longer slow enemies. Lingering Wake erupts after 0.3 seconds for all its damage at once. Your slow upgrades become Wave Damage.'

def disk(path):
    return root / 'Content' / (path.removeprefix('/Game/').split('.')[0] + '.uasset')

def back_up(path):
    source = disk(path)
    if source.exists():
        dest = backup / source.relative_to(root)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, dest)

changed = {disk(path) for path in paths.values()} | {disk(pact_path)}
preserved = {p: hashlib.sha256(p.read_bytes()).hexdigest()
             for p in (root / 'Content/HeavensDivide/Upgrades').rglob('*.uasset') if p not in changed}
bp = unreal.load_asset(controller)
component = unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
pool = list(component.get_editor_property('upgrade_pool'))
if not validate:
    back_up(controller)
    art_paths = {'BattleStance':'DoubleCut', 'Iaijutsu':'IaijutsuStance', 'BladeWave':'CrescentStance'}
    for row in rows:
        path = paths[row['id']]
        back_up(path)
        card = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if not card:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property('data_asset_class', unreal.UpgradeDefinition)
            card = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                'DA_Upgrade_Samurai' + row['id'], folder.rstrip('/'), unreal.UpgradeDefinition, factory)
            art = unreal.load_asset(folder + 'DA_Upgrade_Samurai' + art_paths[row['stance']]).get_editor_property('card_artwork')
            card.set_editor_property('icon', art)
            card.set_editor_property('card_artwork', art)
        balance = {str(k):v for k,v in card.get_editor_property('balance_parameters').items()}
        for key, value in row['balance'].items():
            if row['id'] == 'ReturningBlade': balance[key] = value  # Rebalance the formerly disabled return.
            else: balance.setdefault(key, value)
        properties = dict(upgrade_id=row['id'], display_name=row['name'], description=row['description'],
            max_level=row['ranks'], rarity=getattr(unreal.UpgradeRarity, row['rarity'].upper()),
            category=unreal.UpgradeCategory.SAMURAI, investment_owner=unreal.UpgradeInvestmentOwner.SAMURAI,
            role=unreal.UpgradeRole.MECHANIC if row['ranks'] == 1 else unreal.UpgradeRole.SUPPORT,
            prerequisite_upgrade_ids=[row['stance']] + row['requires'], prerequisite_requirements=[],
            stat_modifiers=[], special_effects=[], uses_rolled_rarity=False, rarity_magnitudes=[],
            rolled_description_format='', requires_meta_unlock=False, unlocked_by_default=True,
            balance_parameters=balance)
        for key, value in properties.items(): card.set_editor_property(key, value)
        unreal.SystemLibrary.execute_console_command(None, 'setnopec ' + card.get_path_name() + ' bHasRuntimeBalance True')
        assert unreal.EditorAssetLibrary.save_loaded_asset(card, False)
        if card not in pool: pool.append(card)
    back_up(pact_path)
    pact = unreal.load_asset(pact_path)
    pact.set_editor_property('description', pact_text)
    assert unreal.EditorAssetLibrary.save_loaded_asset(pact, False)
    component.set_editor_property('upgrade_pool', pool)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    (backup / 'preserved_hashes.json').write_text(json.dumps({str(p.relative_to(root)):v for p,v in preserved.items()}, indent=2))
    unreal.log('SAMURAI_TREES_BACKUP: ' + str(backup))

cards = {str(card.get_editor_property('upgrade_id')):card for card in pool if card}
assert len(cards) == len(pool) == catalog['catalog_count'], (len(cards), len(pool))
disabled = set(catalog['disabled_ids'])
assert len(cards.keys() - disabled) == catalog['enabled_count']
for row in rows:
    card = cards[row['id']]
    assert card.get_editor_property('max_level') == row['ranks']
    assert str(card.get_editor_property('description')) == row['description']
    assert card.get_editor_property('rarity') == getattr(unreal.UpgradeRarity, row['rarity'].upper())
    assert [str(v) for v in card.get_editor_property('prerequisite_upgrade_ids')] == [row['stance']] + row['requires']
    assert card.get_editor_property('card_artwork')
    assert not card.get_editor_property('requires_meta_unlock')
    assert card.get_editor_property('category') == unreal.UpgradeCategory.SAMURAI
assert str(cards['CrescentEruptionPact'].get_editor_property('description')) == pact_text
for stance in ('BattleStance', 'Iaijutsu', 'BladeWave'):
    group = []
    for uid, card in cards.items():
        if uid in disabled: continue
        prereqs = [str(v) for v in card.get_editor_property('prerequisite_upgrade_ids')]
        shared_blood = stance == 'BattleStance' and uid in ('SamuraiHeavyBlade', 'SamuraiTempo', 'SamuraiArea')
        if uid == stance or stance in prereqs or shared_blood: group.append(card)
    categories = {'mechanics':0, 'normal':0, 'rare':0, 'shrine':0}
    for card in group:
        if str(card.get_editor_property('upgrade_id')) == stance: continue
        if card.get_editor_property('category') == unreal.UpgradeCategory.CURSED: key = 'shrine'
        elif card.get_editor_property('max_level') == 1: key = 'mechanics'
        elif card.get_editor_property('rarity') == unreal.UpgradeRarity.RARE: key = 'rare'
        else: key = 'normal'
        categories[key] += 1
    assert len(group) == 21 and categories == {'mechanics':6,'normal':5,'rare':6,'shrine':3}, (stance,len(group),categories)
    unreal.log('SAMURAI_TREE: ' + stance + ' ' + str(categories))
for p, digest in preserved.items(): assert hashlib.sha256(p.read_bytes()).hexdigest() == digest, 'Unrelated card changed: ' + str(p)
unreal.log('SAMURAI_TREES_OK: 156 unique cards, 148 enabled, three equal 21-card stance trees; other assets preserved')
