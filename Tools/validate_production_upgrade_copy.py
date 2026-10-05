"""Strictly asset-read-only validation of published upgrade copy and preservation.

Loads assets, exports text snapshots to Saved, and writes a validation report.
Never sets asset properties, imports assets, saves packages, or runs authoring.
"""
import hashlib
import json
import re
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
catalog = json.loads((root / 'Tools/card_descriptions.json').read_text(encoding='utf-8'))
art = json.loads((root / 'Art/UpgradeIcons/manifest.json').read_text(encoding='utf-8'))
art_by_id = {row['id']: row for row in art['cards']}
art_baseline = json.loads((root / 'Art/UpgradeIcons/tuning_baseline.json').read_text(encoding='utf-8'))
work = root / 'Saved/CardCopyValidation'
baseline = json.loads((work / 'baseline.json').read_text(encoding='utf-8'))
output = work / 'ReadOnly'
output.mkdir(parents=True, exist_ok=True)


def hash_lines(lines):
    return hashlib.sha256('\n'.join(lines).encode('utf-8')).hexdigest()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def export(asset, path):
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = str(path)
    task.exporter = unreal.ObjectExporterT3D()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    assert unreal.Exporter.run_asset_export_task(task), path
    raw = path.read_bytes()
    text = raw.decode('utf-16') if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else raw.decode('utf-8-sig')
    return [line.rstrip() for line in text.splitlines()]


registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
assets = {}
for data in registry.get_assets_by_path('/Game/HeavensDivide/Upgrades', recursive=True):
    if str(data.asset_class_path.asset_name) == 'UpgradeDefinition':
        card = data.get_asset()
        uid = str(card.get_editor_property('upgrade_id'))
        assert uid not in assets, uid
        assets[uid] = card
assert set(assets) == set(catalog) == set(baseline) == set(art_by_id)

paths = list((root / 'Content/HeavensDivide/Upgrades').rglob('*.uasset'))
paths += list((root / 'Content/HeavensDivide/Blueprints/UI/UpgradeIconArt').glob('*.uasset'))
before_files = {p: digest(p) for p in paths}
rows = []
variant_count = 0
for uid, card in sorted(assets.items()):
    row = catalog[uid]
    description = row['description']
    fmt = row.get('format', '')
    variants = list(card.get_editor_property('rarity_magnitudes'))
    def rarity(v):
        return str(v.get_editor_property('rarity')).split('.')[1].split(':')[0].title()
    def resolved(v):
        magnitude = v.get_editor_property('magnitude')
        return row.get('overrides', {}).get(rarity(v), fmt.replace('{Magnitude}', f'{magnitude:g}').replace('{Percent}', str(round(magnitude * 100))))
    if card.get_editor_property('uses_rolled_rarity'):
        assert fmt and variants, uid
        base = next((v for v in variants if v.get_editor_property('rarity') == card.get_editor_property('rarity')), variants[0])
        description = resolved(base)
    else:
        assert not fmt, uid
    assert str(card.get_editor_property('display_name')) == row['name'], uid
    assert str(card.get_editor_property('description')) == description, uid
    assert str(card.get_editor_property('rolled_description_format')) == fmt, uid
    assert 0 < len(row['name']) <= 30 and 0 < len(description) <= 220, uid
    for variant in variants:
        expected_override = row.get('overrides', {}).get(rarity(variant), '')
        assert str(variant.get_editor_property('description_override')) == expected_override, uid
        text = resolved(variant)
        assert 0 < len(text) <= 220 and '{' not in text and '}' not in text, uid
        variant_count += 1
    for key in ('icon', 'card_artwork'):
        texture = card.get_editor_property(key)
        assert texture and texture.get_path_name().split('.')[0] == art_by_id[uid]['texture'], uid
    assert art_by_id[uid]['name'] == row['name'] and art_by_id[uid]['description'] == description, uid
    lines = export(card, output / (uid + '.copy'))
    non_copy = []
    for line in lines:
        if re.match(r'^\s*(?:DisplayName|Description|RolledDescriptionFormat)=', line):
            continue
        if re.match(r'^\s*RarityMagnitudes\(', line):
            line = re.sub(r',?DescriptionOverride=.*(?=\)$)', '', line)
        non_copy.append(line)
    assert hash_lines(non_copy) == baseline[uid], uid + ' gameplay or artwork changed'
    assert hash_lines([line for line in lines if not re.match(r'^\s*(?:Icon|CardArtwork)=', line)]) == art_baseline[uid], uid + ' artwork validation baseline mismatch'
    rows.append(dict(id=uid, name=row['name'], description=description, variants=len(variants), non_copy_properties_preserved=True))

assert all(digest(p) == h for p, h in before_files.items()), 'Read-only validation modified an asset'
report = dict(count=len(rows), rarity_variants=variant_count, assets_read_only=True, cards=rows)
(root / 'Saved/CardDescriptionsValidation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log(f'PRODUCTION_UPGRADE_COPY_VALIDATION_SUCCESS: {len(rows)} cards, {variant_count} rarity variants, all gameplay and artwork preserved')
