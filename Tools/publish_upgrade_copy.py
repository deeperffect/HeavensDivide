"""Publish upgrade names and copy after gameplay authoring.

-ValidateCardDescriptions checks every saved card and rarity variant read-only.
Full serialized exports verify every property except the four copy fields.
"""
import hashlib
import json
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
catalog = json.loads((root / 'Tools/card_descriptions.json').read_text(encoding='utf-8'))
validate = '-ValidateCardDescriptions' in unreal.SystemLibrary.get_command_line() or globals().get('FORCE_VALIDATE', False)
work = root / 'Saved/CardCopyValidation'
work.mkdir(parents=True, exist_ok=True)
baseline_path = work / 'baseline.json'
backup = root / 'Saved/Backups/CardDescriptions' / datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S_%f')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def disk(asset):
    return root / 'Content' / (asset.get_path_name().split('.')[0].removeprefix('/Game/') + '.uasset')


def export(asset, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = str(path)
    task.exporter = unreal.ObjectExporterT3D()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    assert unreal.Exporter.run_asset_export_task(task), path
    raw = path.read_bytes()
    return raw.decode('utf-16') if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else raw.decode('utf-8-sig')


def non_copy_digest(text):
    lines = []
    for line in text.splitlines():
        if re.match(r'^\s*(?:DisplayName|Description|RolledDescriptionFormat)=', line):
            continue
        if re.match(r'^\s*RarityMagnitudes\(', line):
            # DescriptionOverride is last; retain rarity and magnitude exactly.
            line = re.sub(r',?DescriptionOverride=.*(?=\)$)', '', line)
        lines.append(line.rstrip())
    return hashlib.sha256('\n'.join(lines).encode('utf-8')).hexdigest()


def non_art_digest(text):
    # Same normalization as import_all_upgrade_icons.py.
    lines = [line.rstrip() for line in text.splitlines()
             if not re.match(r'^\s*(?:Icon|CardArtwork)=', line)]
    return hashlib.sha256('\n'.join(lines).encode('utf-8')).hexdigest()


def rarity_name(variant):
    return str(variant.get_editor_property('rarity')).split('.')[1].split(':')[0].title()


def expected(asset, row):
    variants = list(asset.get_editor_property('rarity_magnitudes'))
    fmt = row.get('format', '')
    description = row['description']
    if asset.get_editor_property('uses_rolled_rarity'):
        assert fmt and variants, asset.get_name()
        base = next((v for v in variants if v.get_editor_property('rarity') == asset.get_editor_property('rarity')), variants[0])
        magnitude = base.get_editor_property('magnitude')
        description = row.get('overrides', {}).get(rarity_name(base),
            fmt.replace('{Magnitude}', f'{magnitude:g}').replace('{Percent}', str(round(magnitude * 100))))
    else:
        assert not fmt, asset.get_name()
    overrides = [row.get('overrides', {}).get(rarity_name(v), '') for v in variants]
    return description, fmt, variants, overrides


registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
assets = {}
for data in registry.get_assets_by_path('/Game/HeavensDivide/Upgrades', recursive=True):
    if str(data.asset_class_path.asset_name) != 'UpgradeDefinition':
        continue
    asset = data.get_asset()
    uid = str(asset.get_editor_property('upgrade_id'))
    assert uid not in assets, uid
    assets[uid] = asset
assert set(assets) == set(catalog), 'Catalog must cover every saved upgrade'

for uid, row in catalog.items():
    assert 0 < len(row['name']) <= 30, uid
    for field in ('description', 'format'):
        text = row.get(field, '')
        assert len(text) <= 220, (uid, field)
        assert set(re.findall(r'\{([^{}]+)\}', text)) <= {'Magnitude', 'Percent'}, uid
        assert not re.search(r'\b(?:hitbox|runtime|Blueprint|RNG|proc|deprecated|retired)\b|Blood Shrine reward|Maximum \d+ ranks', text, re.I), uid

protected = [root / 'Content/HeavensDivide/Blueprints/BP_SurvivorPlayerController.uasset',
             root / 'Content/HeavensDivide/Blueprints/UI/UpgradeUI/WBP_LevelUp.uasset']
for folder in ('UpgradeIconArt', 'CategoryArt'):
    protected += list((root / 'Content/HeavensDivide/Blueprints/UI' / folder).glob('*.uasset'))
protected_hashes = {str(p.relative_to(root)): digest(p) for p in protected}
disk_before = {uid: digest(disk(asset)) for uid, asset in assets.items()}
before = {uid: non_copy_digest(export(asset, work / 'Before' / (uid + '.copy'))) for uid, asset in assets.items()}
if validate:
    assert baseline_path.exists(), 'Publish copy once before validating'
    assert before == json.loads(baseline_path.read_text(encoding='utf-8')), 'Non-copy properties changed since publication'
else:
    backup.mkdir(parents=True)
    for asset in assets.values():
        source = disk(asset)
        destination = backup / source.relative_to(root)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    (backup / 'non_copy_properties.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
    (backup / 'protected_files.json').write_text(json.dumps(protected_hashes, indent=2), encoding='utf-8')
    for relative in ('Art/UpgradeIcons/manifest.json', 'Art/UpgradeIcons/tuning_baseline.json'):
        source = root / relative
        if source.exists():
            destination = backup / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)

report = []
art_baseline = {}
for uid, asset in sorted(assets.items()):
    row = catalog[uid]
    description, fmt, variants, overrides = expected(asset, row)
    old = dict(name=str(asset.get_editor_property('display_name')),
               description=str(asset.get_editor_property('description')),
               format=str(asset.get_editor_property('rolled_description_format')))
    new = dict(name=row['name'], description=description, format=fmt)
    changed = old != new or any(str(v.get_editor_property('description_override')) != t for v, t in zip(variants, overrides))
    if validate:
        assert not changed, 'Saved copy mismatch: ' + uid
    elif changed:
        asset.set_editor_property('display_name', row['name'])
        asset.set_editor_property('description', description)
        asset.set_editor_property('rolled_description_format', fmt)
        for variant, text in zip(variants, overrides):
            variant.set_editor_property('description_override', text)
        asset.set_editor_property('rarity_magnitudes', variants)
        assert non_copy_digest(export(asset, work / 'After' / (uid + '.copy'))) == before[uid], uid + ' non-copy properties changed'
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False), uid
    text = export(asset, work / 'Published' / (uid + '.copy'))
    assert non_copy_digest(text) == before[uid], uid
    art_baseline[uid] = non_art_digest(text)
    for variant, override in zip(variants, overrides):
        magnitude = variant.get_editor_property('magnitude')
        rendered = override or fmt.replace('{Magnitude}', f'{magnitude:g}').replace('{Percent}', str(round(magnitude * 100)))
        assert '{' not in rendered and '}' not in rendered and len(rendered) <= 220, uid
    report.append(dict(id=uid, changed=changed, before=old, **new, variants=overrides, non_copy_properties_preserved=True))

assert all(digest(root / p) == h for p, h in protected_hashes.items()), 'Pool, UI or artwork changed'
if validate:
    assert all(digest(disk(assets[uid])) == h for uid, h in disk_before.items()), 'Validation modified assets'
else:
    baseline_path.write_text(json.dumps(before, indent=2), encoding='utf-8')
    art_path = root / 'Art/UpgradeIcons/manifest.json'
    if art_path.exists():
        art = json.loads(art_path.read_text(encoding='utf-8'))
        by_id = {row['id']: row for row in report}
        for card in art['cards']:
            for key in ('name', 'description', 'format'):
                card[key] = by_id[card['id']][key]
        # Retain exact original generation prompts and artwork provenance.
        art_path.write_text(json.dumps(art, indent=2)+'\n', encoding='utf-8')
        (art_path.parent / 'tuning_baseline.json').write_text(json.dumps(art_baseline, indent=2), encoding='utf-8')
    unreal.log('CARD_COPY_BACKUP: ' + str(backup))

filename = 'CardDescriptionsValidation.json' if validate else 'CardDescriptionsChanges.json'
(root / 'Saved' / filename).write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log(f'CARD_COPY_OK: {len(report)} cards; {sum(r["changed"] for r in report)} changed; validate={validate}; non-copy properties preserved')
