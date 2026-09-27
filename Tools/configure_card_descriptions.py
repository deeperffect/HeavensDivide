"""Apply compact card copy only; run last after upgrade-authoring scripts.

Pass -ValidateCardDescriptions to check saved assets without changing them.
The catalog covers every saved upgrade, including all rolled rarity variants.
"""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
copy = json.loads((root / 'Tools/card_descriptions.json').read_text(encoding='utf-8'))
validate = '-ValidateCardDescriptions' in unreal.SystemLibrary.get_command_line()
backup = root / 'Saved/Backups/CardDescriptions' / datetime.now().strftime('%Y%m%d_%H%M%S')
rows = []
seen = set()

def tuning(asset):
    # Include numeric/structured gameplay fields affected by common authoring scripts.
    fields = ['balance_parameters', 'stat_modifiers', 'special_effects', 'max_level',
              'rarity', 'uses_rolled_rarity', 'prerequisite_upgrade_ids',
              'prerequisite_requirements', 'exclusivity_group', 'presentation']
    return {key: str(asset.get_editor_property(key)) for key in fields} | {
        'magnitudes': [(str(v.get_editor_property('rarity')), v.get_editor_property('magnitude'))
                       for v in asset.get_editor_property('rarity_magnitudes')]}

assets = []
for path in unreal.EditorAssetLibrary.list_assets('/Game/HeavensDivide/Upgrades', recursive=True):
    asset = unreal.load_asset(path)
    if isinstance(asset, unreal.UpgradeDefinition):
        uid = str(asset.get_editor_property('upgrade_id'))
        assert uid in copy, 'Missing copy: ' + uid
        assert uid not in seen, 'Duplicate card: ' + uid
        seen.add(uid)
        assets.append((asset, uid))
assert seen == set(copy), 'Catalog and saved cards differ: ' + str(seen ^ set(copy))

for asset, uid in assets:
    description = copy[uid][0]
    fmt = copy[uid][1] if len(copy[uid]) > 1 else ''
    variants = list(asset.get_editor_property('rarity_magnitudes'))
    expected_variants = []
    for variant in variants:
        magnitude = variant.get_editor_property('magnitude')
        # Existing overrides take precedence over the rolled format at runtime.
        expected_variants.append(fmt.replace('{Magnitude}', f'{magnitude:g}')
            .replace('{Percent}', str(round(magnitude * 100))) if str(variant.get_editor_property('description_override')) else '')
    assert max(map(len, [description, fmt] + expected_variants)) <= 125, uid
    before = tuning(asset)
    old = [str(asset.get_editor_property('description')), str(asset.get_editor_property('rolled_description_format'))]
    changed = old != [description, fmt] or any(str(v.get_editor_property('description_override')) != text for v, text in zip(variants, expected_variants))
    if validate:
        assert not changed, 'Saved copy mismatch: ' + uid
    elif changed:
        relative = Path(asset.get_path_name().split('.')[0].removeprefix('/Game/') + '.uasset')
        destination = backup / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root / 'Content' / relative, destination)
        asset.set_editor_property('description', description)
        asset.set_editor_property('rolled_description_format', fmt)
        for variant, text in zip(variants, expected_variants):
            variant.set_editor_property('description_override', text)
        asset.set_editor_property('rarity_magnitudes', variants)
        assert tuning(asset) == before, 'Gameplay tuning changed: ' + uid
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False), uid
    rows.append(dict(id=uid, changed=changed, before=old, description=description, format=fmt, variants=expected_variants))

report = 'CardDescriptionsValidation.json' if validate else 'CardDescriptionsChanges.json'
(root / 'Saved' / report).write_text(json.dumps(rows, indent=2), encoding='utf-8')
unreal.log(f'CARD_COPY_OK: {len(rows)} cards; {sum(r["changed"] for r in rows)} changed; validate={validate}')
