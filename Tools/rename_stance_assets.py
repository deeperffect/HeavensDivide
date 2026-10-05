"""Rename stance DAs without changing IDs or tuning; -ValidateStanceNames is read-only."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import re
import shutil
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
controller = '/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
renames = [
    ('Samurai', 'BladeWave', 'CrescentStance'),
    ('Samurai', 'Iaijutsu', 'IaijutsuStance'),
    ('Ninja', 'ReturningFang', 'ReturningFangStance'),
    ('Ninja', 'GreatShuriken', 'GreatShurikenStance'),
]
validate = '-ValidateStanceNames' in unreal.SystemLibrary.get_command_line()
backup = root / 'Saved/Backups/StanceAssetNames' / datetime.now().strftime('%Y%m%d_%H%M%S')
properties = '''has_runtime_balance has_runtime_presentation balance_parameters presentation
upgrade_id display_name description icon card_artwork category investment_owner role build_family_id
meta_unlock_id requires_meta_unlock unlocked_by_default max_level rarity uses_rolled_rarity
rarity_magnitudes rolled_description_format prerequisite_upgrade_ids prerequisite_requirements
stat_modifiers special_effects exclusivity_group handoff_attack_speed_bonus handoff_duration
afterimage_frenzy_attack_speed_bonus'''.split()


def asset_path(owner, name):
    return '/Game/HeavensDivide/Upgrades/' + owner + '/DA_Upgrade_' + owner + name


def disk(path):
    return root / 'Content' / (path.removeprefix('/Game/').split('.')[0] + '.uasset')


def snapshot(card):
    # Struct repr includes a transient address; only compare serialized property values.
    return {key: re.sub(r'0x[0-9a-fA-F]+', '<address>', str(card.get_editor_property(key)))
            for key in properties}


bp = unreal.load_asset(controller)
component = unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
pool = list(component.get_editor_property('upgrade_pool'))
before = {str(card.get_editor_property('upgrade_id')): snapshot(card) for card in pool}
old_paths = {asset_path(owner, old) for owner, old, new in renames}

if not validate:
    referencers = {controller}
    for owner, old, new in renames:
        source = asset_path(owner, old)
        if unreal.EditorAssetLibrary.does_asset_exist(source):
            referencers.update(str(p) for p in unreal.EditorAssetLibrary.find_package_referencers_for_asset(source, True))
    paths = referencers | {card.get_path_name().split('.')[0] for card in pool}
    for path in paths:
        source = disk(path)
        if source.exists():
            dest = backup / source.relative_to(root)
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, dest)
    backup.mkdir(parents=True, exist_ok=True)
    (backup / 'card_properties.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
    preserved = {disk(path): hashlib.sha256(disk(path).read_bytes()).hexdigest()
                 for path in paths - old_paths - referencers if disk(path).exists()}
    for owner, old, new in renames:
        source, destination = asset_path(owner, old), asset_path(owner, new)
        if not unreal.EditorAssetLibrary.does_asset_exist(destination):
            card = unreal.load_asset(source)
            assert card and str(card.get_editor_property('upgrade_id')) == old, source
            assert unreal.EditorAssetLibrary.rename_asset(source, destination), (source, destination)
        card = unreal.load_asset(destination)
        assert unreal.EditorAssetLibrary.save_loaded_asset(card, False), destination
        assert snapshot(card) == before[old], 'Stance properties changed: ' + old
    # Resave known referencers with the canonical asset paths, retaining compatibility redirects.
    component.set_editor_property('upgrade_pool', pool)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    for path in sorted(referencers):
        asset = unreal.load_asset(path)
        assert asset and unreal.EditorAssetLibrary.save_loaded_asset(asset, False), path
    for path, digest in preserved.items():
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, 'Unrelated card changed: ' + str(path)
    unreal.log('STANCE_NAMES_BACKUP: ' + str(backup))

ids = [str(card.get_editor_property('upgrade_id')) for card in pool]
assert len(ids) == len(set(ids))
assert before == {str(card.get_editor_property('upgrade_id')): snapshot(card) for card in pool}
for owner, old, new in renames:
    canonical = unreal.load_asset(asset_path(owner, new))
    assert canonical and canonical.get_name() == 'DA_Upgrade_' + owner + new
    assert str(canonical.get_editor_property('upgrade_id')) == old
    assert next(card for card in pool if str(card.get_editor_property('upgrade_id')) == old) == canonical
    # The BuildFamilies automation test exercises these redirects through FSoftObjectPath.
    source = asset_path(owner, old) + '.DA_Upgrade_' + owner + old
    redirects = (root / 'Config/DefaultEngine.ini').read_text(encoding='utf-8-sig')
    assert 'OldName="' + source + '"' in redirects, 'Missing legacy reference redirect: ' + old
stances = [card for card in pool if str(card.get_editor_property('exclusivity_group')) in ('SamuraiStance', 'NinjaWeaponStance')]
assert len(stances) == 6
assert all('Stance' in card.get_name() for card in stances)
# Check every saved DA, including retired definitions outside the offer pool.
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
named_stances = []
for data in registry.get_assets_by_path('/Game/HeavensDivide/Upgrades', recursive=True):
    if str(data.asset_class_path.asset_name) != 'UpgradeDefinition':
        continue
    if 'stance' in str(data.asset_name).lower():
        card = data.get_asset()
        assert card in stances, 'Non-stance DA contains Stance: ' + card.get_path_name()
        named_stances.append(card)
assert len(named_stances) == 6
unreal.log(f'STANCE_NAMES_OK: only the 6 stance DAs include Stance; {len(ids)} unique pool cards, tuning preserved and legacy redirects configured')
