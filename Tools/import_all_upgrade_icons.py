"""Import the complete cartoon icon catalog, changing only artwork references.

Run in Unreal Python. -ValidateAllUpgradeIcons checks saved assets read-only.
Backups include original assets and full text exports of their non-art settings.
"""
import hashlib
import json
import re
import shutil
import struct
from datetime import datetime, timezone
from pathlib import Path

import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
CATALOG = ROOT / 'Art/UpgradeIcons'
MANIFEST = json.loads((CATALOG / 'manifest.json').read_text(encoding='utf-8'))
VALIDATE_ONLY = globals().get('FORCE_VALIDATE', False) or '-ValidateAllUpgradeIcons' in u.SystemLibrary.get_command_line()
CARDS = MANIFEST['cards']


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def asset_file(path):
    package = path.split('.', 1)[0]
    assert package.startswith('/Game/'), path
    return ROOT / 'Content' / (package.removeprefix('/Game/') + '.uasset')


def non_art_export(asset, filename):
    filename.parent.mkdir(parents=True, exist_ok=True)
    task = u.AssetExportTask()
    task.object = asset
    task.filename = str(filename)
    task.exporter = u.ObjectExporterT3D()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    assert u.Exporter.run_asset_export_task(task), filename
    raw = filename.read_bytes()
    text = raw.decode('utf-16') if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else raw.decode('utf-8-sig')
    # Compare every serialized property, allowing only the two texture references.
    lines = [line.rstrip() for line in text.splitlines()
             if not re.match(r'^\s*(?:Icon|CardArtwork)=', line)]
    return hashlib.sha256('\n'.join(lines).encode('utf-8')).hexdigest()


registry = u.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
actual = {}
for data in registry.get_assets_by_path('/Game/HeavensDivide/Upgrades', recursive=True):
    if str(data.asset_class_path.asset_name) == 'UpgradeDefinition':
        asset = data.get_asset()
        uid = str(asset.get_editor_property('upgrade_id'))
        assert uid not in actual, 'Duplicate ID: ' + uid
        actual[uid] = asset
assert len(CARDS) == MANIFEST['total'] == len(actual)
assert {row['id'] for row in CARDS} == set(actual), 'Manifest does not cover every saved upgrade'
assert len({row['texture'] for row in CARDS}) == len(CARDS), 'Textures must be unique'

source_hashes = {}
for row in CARDS:
    source = ROOT / row['file']
    with source.open('rb') as stream:
        header = stream.read(24)
    assert header[:8] == b'\x89PNG\r\n\x1a\n', source
    width, height = struct.unpack('>II', header[16:24])
    assert width >= 512 and height >= 512 and abs(width - height) <= 2, (row['id'], width, height)
    source_hashes[row['id']] = digest(source)
assert len(set(source_hashes.values())) == len(CARDS), 'Each card needs a distinct image'

baseline_path = CATALOG / 'tuning_baseline.json'
baseline = json.loads(baseline_path.read_text(encoding='utf-8')) if baseline_path.exists() else None
report = {'count': len(CARDS), 'mode': 'validate' if VALIDATE_ONLY else 'import', 'cards': []}
check_dir = ROOT / 'Saved/UpgradeIconValidation'
check_dir.mkdir(parents=True, exist_ok=True)

if not VALIDATE_ONLY:
    backup = ROOT / 'Saved/Backups/UpgradeIcons' / datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S_%f')
    backup.mkdir(parents=True)
    report['backup'] = str(backup.relative_to(ROOT))
    before = {}
    for row in CARDS:
        asset = actual[row['id']]
        assert asset.get_path_name().split('.')[0] == row['asset']
        before[row['id']] = non_art_export(asset, backup / 'Before' / (row['id'] + '.copy'))
        for source in (asset_file(row['asset']), asset_file(row['texture'])):
            if source.exists():
                destination = backup / source.relative_to(ROOT)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, destination)
    if baseline is None:
        baseline = before
        baseline_path.write_text(json.dumps(baseline, indent=2), encoding='utf-8')
    else:
        # Existing tuning may legitimately change between later art imports.
        # This run always checks against a fresh snapshot, and publishes it on success.
        baseline = before
    protected = [ROOT / 'Content/HeavensDivide/Blueprints/BP_SurvivorPlayerController.uasset',
                 ROOT / 'Content/HeavensDivide/Blueprints/UI/UpgradeUI/WBP_LevelUp.uasset']
    protected += list((ROOT / 'Content/HeavensDivide/Blueprints/UI/CategoryArt').glob('*.uasset'))
    protected_hashes = {str(path.relative_to(ROOT)): digest(path) for path in protected}
    (backup / 'non_art_settings.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
    (backup / 'protected_files.json').write_text(json.dumps(protected_hashes, indent=2), encoding='utf-8')

    for index, row in enumerate(CARDS, 1):
        task = u.AssetImportTask()
        task.filename = str(ROOT / row['file'])
        task.destination_path, task.destination_name = row['texture'].rsplit('/', 1)
        task.automated = True
        task.replace_existing = True
        task.save = True
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        assert task.imported_object_paths, row['id']
        texture = u.load_asset(row['texture'])
        assert isinstance(texture, u.Texture2D), row['id']
        for name, value in {
            'srgb': True, 'lod_group': u.TextureGroup.TEXTUREGROUP_UI,
            'compression_settings': u.TextureCompressionSettings.TC_EDITOR_ICON,
            'mip_gen_settings': u.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            'address_x': u.TextureAddress.TA_CLAMP, 'address_y': u.TextureAddress.TA_CLAMP,
            'never_stream': True, 'max_texture_size': 512,
        }.items():
            texture.set_editor_property(name, value)
        assert u.EditorAssetLibrary.save_loaded_asset(texture, False)
        asset = actual[row['id']]
        asset.set_editor_property('card_artwork', texture)
        asset.set_editor_property('icon', texture)
        assert non_art_export(asset, backup / 'After' / (row['id'] + '.copy')) == before[row['id']], row['id'] + ' tuning changed'
        assert u.EditorAssetLibrary.save_loaded_asset(asset, False)
        u.log('UPGRADE_ICON_IMPORTED ' + str(index) + '/' + str(len(CARDS)) + ' ' + row['id'])
    assert all(digest(ROOT / rel) == expected for rel, expected in protected_hashes.items()), 'Unrelated UI or upgrade pool changed'
    baseline_path.write_text(json.dumps(baseline, indent=2), encoding='utf-8')

assert baseline is not None, 'Import must establish a tuning baseline before validation'
for row in CARDS:
    asset = actual[row['id']]
    texture = u.load_asset(row['texture'])
    assert isinstance(texture, u.Texture2D), row['id']
    assert asset.get_editor_property('card_artwork') == texture, row['id'] + ' artwork'
    assert asset.get_editor_property('icon') == texture, row['id'] + ' icon'
    assert texture.get_editor_property('max_texture_size') == 512, row['id']
    assert texture.get_editor_property('lod_group') == u.TextureGroup.TEXTUREGROUP_UI, row['id']
    assert texture.get_editor_property('compression_settings') == u.TextureCompressionSettings.TC_EDITOR_ICON, row['id']
    assert texture.get_editor_property('mip_gen_settings') == u.TextureMipGenSettings.TMGS_NO_MIPMAPS, row['id']
    assert texture.get_editor_property('srgb') and texture.get_editor_property('never_stream'), row['id']
    assert texture.get_editor_property('address_x') == u.TextureAddress.TA_CLAMP, row['id']
    assert texture.get_editor_property('address_y') == u.TextureAddress.TA_CLAMP, row['id']
    assert non_art_export(asset, check_dir / (row['id'] + '.copy')) == baseline[row['id']], row['id'] + ' saved tuning changed'
    report['cards'].append({'id': row['id'], 'asset': row['asset'], 'texture': texture.get_path_name(),
                            'file': row['file'], 'source_sha256': source_hashes[row['id']],
                            'non_art_settings_preserved': True})

destination = check_dir / 'report.json' if VALIDATE_ONLY else CATALOG / 'import_report.json'
destination.write_text(json.dumps(report, indent=2), encoding='utf-8')
u.log('ALL_UPGRADE_ICONS_' + ('VALIDATION' if VALIDATE_ONLY else 'IMPORT') + '_SUCCESS: ' + str(len(CARDS)))
