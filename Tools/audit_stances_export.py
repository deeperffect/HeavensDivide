"""Read-only snapshot of saved stance/upgrade data for the spreadsheet audit."""
import json
import re
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
out = root / 'Saved' / 'StanceAudit_20261016'
out.mkdir(parents=True, exist_ok=True)
errors = []

def scalar(v):
    if v is None or isinstance(v, (str, bool, int, float)):
        return v
    if isinstance(v, u.Object):
        return v.get_path_name()
    return str(v)

def fields(obj, names, context):
    result = {}
    for name in names.split():
        try:
            result[name] = scalar(obj.get_editor_property(name))
        except Exception as e:
            errors.append({'context': context, 'field': name, 'error': str(e)})
    return result

registry = u.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
cards = []
for asset in registry.get_assets_by_path('/Game', True):
    if str(asset.asset_class_path.asset_name) != 'UpgradeDefinition':
        continue
    obj = asset.get_asset()
    path = obj.get_path_name()
    row = fields(obj, 'upgrade_id display_name description category investment_owner role build_family_id meta_unlock_id requires_meta_unlock unlocked_by_default max_level rarity uses_rolled_rarity rolled_description_format exclusivity_group has_runtime_balance has_runtime_presentation handoff_attack_speed_bonus handoff_duration afterimage_frenzy_attack_speed_bonus icon card_artwork', path)
    row['path'] = path
    row['resolved_description'] = str(obj.get_balance_description())
    row['balance_parameters'] = {str(k): float(v) for k, v in dict(obj.get_editor_property('balance_parameters')).items()}
    for key in ('prerequisite_upgrade_ids', 'special_effects'):
        row[key] = [scalar(v) for v in obj.get_editor_property(key)]
    for key, names in [('rarity_magnitudes', 'rarity magnitude description_override'), ('prerequisite_requirements', 'upgrade_id minimum_level'), ('stat_modifiers', 'target character_stat shared_player_stat operation value_per_level')]:
        row[key] = [fields(v, names, path + ':' + key) for v in obj.get_editor_property(key)]
    cards.append(row)

bp = u.load_asset('/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController')
cdo = u.get_default_object(bp.generated_class())
component = cdo.get_editor_property('player_upgrade_component')
pool = [v.get_path_name() if v else None for v in component.get_editor_property('upgrade_pool')]
rarity_brackets = [fields(v, 'minimum_run_time_seconds common_weight rare_weight epic_weight', 'RarityBracket') for v in component.get_editor_property('rarity_time_brackets')]
balance = u.load_asset('/Game/HeavensDivide/Balance/DA_GlobalBalance')
skills = [fields(s, 'id name description branch tier lane required_stance page passive_effect max_rank base_cost prerequisites effect per_rank target shared_stat character_stat', 'MetaSkill') for s in balance.get_editor_property('skills')]
for row, skill in zip(skills, balance.get_editor_property('skills')):
    row['prerequisites'] = [str(v) for v in skill.get_editor_property('prerequisites')]

defaults = {}
for hero in ('Samurai', 'Ninja'):
    bp = u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_' + hero)
    hero_cdo = u.get_default_object(bp.generated_class())
    defaults[hero] = {}
    for comp in hero_cdo.get_components_by_class(u.ActorComponent):
        class_name = comp.get_class().get_name()
        if class_name != 'AutoAttackComponent':
            continue
        header = (root / 'Source' / 'HeavensDivide' / 'AutoAttackComponent.h').read_text()
        names = re.findall(r'UPROPERTY\(EditAnywhere[^\n]*\)\s*(?:float|int32|bool)\s+(\w+)', header)
        vals = {}
        for name in names:
            try:
                clean = name[1:] if name.startswith('b') and name[1].isupper() else name
                snake = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', re.sub(r'([A-Z]+)([A-Z][a-z])', r'\1_\2', clean)).lower()
                vals[name] = scalar(comp.get_editor_property(snake))
            except Exception as e:
                errors.append({'context': hero, 'field': name, 'error': str(e)})
        defaults[hero][class_name] = vals

payload = {'audit_date': '2026-10-16', 'cards': sorted(cards, key=lambda x: x['upgrade_id']), 'pool': pool, 'skills': skills, 'cadence_rank_cap': balance.get_editor_property('cadence_upgrade_rank_cap'), 'rarity_brackets': rarity_brackets, 'character_defaults': defaults, 'errors': errors}
(out / 'saved_assets.json').write_text(json.dumps(payload, indent=2), encoding='utf-8')
u.log('STANCE_AUDIT_EXPORT ' + json.dumps({'cards': len(cards), 'pool': len(pool), 'skills': len(skills), 'errors': len(errors)}))
