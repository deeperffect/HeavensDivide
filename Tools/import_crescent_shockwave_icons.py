"""Refresh existing Crescent icon textures without saving any upgrade data assets.

Run with Unreal Python. -ValidateCrescentShockwaveIcons checks saved results only.
The art manifest records the exact built-in imagegen prompts and source hashes.
"""
import hashlib
import json
import shutil
import struct
from datetime import datetime, timezone
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
ART = ROOT / 'Art/UpgradeIcons'
REFRESH = json.loads((ART / 'crescent_shockwave_refresh.json').read_text(encoding='utf-8'))
CATALOG = json.loads((ART / 'manifest.json').read_text(encoding='utf-8'))
CARDS = {row['id']: row for row in CATALOG['cards']}
VALIDATE = '-ValidateCrescentShockwaveIcons' in u.SystemLibrary.get_command_line()
CHECK = ROOT / 'Saved/CrescentShockwaveValidation'
CHECK.mkdir(parents=True, exist_ok=True)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def package_file(package):
    assert package.startswith('/Game/')
    return ROOT / 'Content' / (package.removeprefix('/Game/').split('.')[0] + '.uasset')


rows = REFRESH['cards']
assert len(rows) == len({r['id'] for r in rows}) == 21
disabled = set(json.loads((ROOT / 'Tools/samurai_stance_expansion.json').read_text())['disabled_ids'])
expected = {r['id'] for r in CARDS.values() if r['in_pool'] and r['id'] not in disabled
            and (r['id'] == 'BladeWave' or 'BladeWave' in r['requirements'])}
assert expected == {r['id'] for r in rows}
targets = {package_file(row['texture']) for row in rows}
for row in rows:
    assert row['texture'] == CARDS[row['id']]['texture']
    assert row['file'] == CARDS[row['id']]['file']
    source = ROOT / row['file']
    assert digest(source) == row['source_sha256'], row['id']
    data = source.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    width, height = struct.unpack('>II', data[16:24])
    assert width == height and width >= 512, row['id']
assert len({r['source_sha256'] for r in rows}) == 21

# Protect all upgrade data, the pool, UI, category art and every other icon.
protected = set((ROOT / 'Content/HeavensDivide/Upgrades').rglob('*.uasset'))
protected.update((ROOT / 'Content/HeavensDivide/Blueprints/UI').rglob('*.uasset'))
protected.add(ROOT / 'Content/HeavensDivide/Blueprints/BP_SurvivorPlayerController.uasset')
if not VALIDATE:
    protected -= targets
before = {str(path.relative_to(ROOT)): digest(path) for path in protected}
backup = None
if not VALIDATE:
    backup = ROOT / 'Saved/Backups/CrescentShockwaveArt' / datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S_%f')
    backup.mkdir(parents=True)
    (backup / 'protected_files.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
    for path in targets:
        assert path.is_file(), path
        destination = backup / path.relative_to(ROOT)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
    for row in rows:
        task = u.AssetImportTask()
        task.filename = str(ROOT / row['file'])
        task.destination_path, task.destination_name = row['texture'].rsplit('/', 1)
        task.automated = True
        task.replace_existing = True
        task.save = False
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        assert task.imported_object_paths, row['id']
        texture = u.load_asset(row['texture'])
        assert isinstance(texture, u.Texture2D), row['id']
        settings = dict(srgb=True, lod_group=u.TextureGroup.TEXTUREGROUP_UI,
                        compression_settings=u.TextureCompressionSettings.TC_EDITOR_ICON,
                        mip_gen_settings=u.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                        address_x=u.TextureAddress.TA_CLAMP, address_y=u.TextureAddress.TA_CLAMP,
                        never_stream=True, max_texture_size=512)
        for key, value in settings.items():
            texture.set_editor_property(key, value)
        assert u.EditorAssetLibrary.save_loaded_asset(texture, False), row['id']
        u.log('CRESCENT_SHOCKWAVE_IMPORTED ' + row['id'])

for row in rows:
    card = u.load_asset(CARDS[row['id']]['asset'])
    texture = u.load_asset(row['texture'])
    assert isinstance(texture, u.Texture2D), row['id']
    assert card.get_editor_property('icon') == texture, row['id']
    assert card.get_editor_property('card_artwork') == texture, row['id']
    assert texture.get_editor_property('max_texture_size') == 512, row['id']
    assert texture.get_editor_property('lod_group') == u.TextureGroup.TEXTUREGROUP_UI, row['id']
    assert texture.get_editor_property('compression_settings') == u.TextureCompressionSettings.TC_EDITOR_ICON, row['id']
    assert texture.get_editor_property('mip_gen_settings') == u.TextureMipGenSettings.TMGS_NO_MIPMAPS, row['id']
    assert texture.get_editor_property('srgb') and texture.get_editor_property('never_stream'), row['id']
    assert texture.get_editor_property('address_x') == texture.get_editor_property('address_y') == u.TextureAddress.TA_CLAMP
    sources = texture.get_editor_property('asset_import_data').extract_filenames()
    assert len(sources) == 1 and Path(sources[0]).resolve() == (ROOT / row['file']).resolve(), row['id']
    # Export saved texture pixels for visual inspection independently of source PNGs.
    if row['id'] in ('BladeWave', 'CrescentField', 'CrescentSlow'):
        task = u.AssetExportTask()
        task.object = texture
        task.exporter = u.TextureExporterPNG()
        task.filename = str(CHECK / (row['id'] + '.png'))
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        assert u.Exporter.run_asset_export_task(task), row['id']
assert all(digest(ROOT / rel) == value for rel, value in before.items()), 'An unrelated asset changed'
report = dict(count=len(rows), mode='validate' if VALIDATE else 'import',
              protected_files_unchanged=len(before), upgrade_data_unchanged=True,
              backup=str(backup.relative_to(ROOT)) if backup else None, cards=rows)
destination = CHECK / 'report.json' if VALIDATE else ART / 'crescent_shockwave_import_report.json'
destination.write_text(json.dumps(report, indent=2), encoding='utf-8')
if not VALIDATE:
    full_report_path = ART / 'import_report.json'
    full_report = json.loads(full_report_path.read_text(encoding='utf-8'))
    hashes = {r['id']: r['source_sha256'] for r in rows}
    for row in full_report['cards']:
        if row['id'] in hashes:
            row['source_sha256'] = hashes[row['id']]
    full_report['latest_texture_refresh'] = 'Art/UpgradeIcons/crescent_shockwave_import_report.json'
    full_report_path.write_text(json.dumps(full_report, indent=2), encoding='utf-8')
u.log('CRESCENT_SHOCKWAVE_' + ('VALIDATION' if VALIDATE else 'IMPORT') + '_SUCCESS: 21 textures; all upgrade data preserved')
