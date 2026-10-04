"""Import category illustrations and assign WBP_LevelUp's four artwork slots.

Run with Unreal's -ExecutePythonScript. Add -ValidateCategoryArtwork to check
saved assignments and texture settings without changing any Unreal assets.
"""
import hashlib
import json
import shutil
import struct
from datetime import datetime, timezone
from pathlib import Path

import unreal as u


ROOT = Path(u.Paths.project_dir()).resolve()
MANIFEST = json.loads((ROOT / 'Art/CategoryCards/manifest.json').read_text(encoding='utf-8'))
VALIDATE_ONLY = '-ValidateCategoryArtwork' in u.SystemLibrary.get_command_line()


def asset_file(asset_path):
    package = asset_path.split('.', 1)[0]
    assert package.startswith('/Game/'), package
    return ROOT / 'Content' / (package.removeprefix('/Game/') + '.uasset')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def png_size(path):
    with path.open('rb') as stream:
        header = stream.read(24)
    assert header[:8] == b'\x89PNG\r\n\x1a\n', path
    return struct.unpack('>II', header[16:24])


def object_path(obj):
    return obj.get_path_name() if obj else None


def presentation_defaults(defaults):
    properties = [owner + suffix for owner in ('samurai', 'ninja', 'synergy', 'global')
                  for suffix in ('_card_border', '_category_border')]
    properties += [rarity + '_rarity_material' for rarity in ('rare', 'epic', 'legendary')]
    return {name: object_path(defaults.get_editor_property(name)) for name in properties}


bp = u.load_asset(MANIFEST['widget'])
assert bp, MANIFEST['widget']
cdo = u.get_default_object(bp.generated_class())
assert cdo
cards = MANIFEST['cards']
assert {card['category'] for card in cards} == {'Samurai', 'Ninja', 'Synergy', 'Global'}
assert len(cards) == 4
for card in cards:
    assert png_size(ROOT / card['file']) == (1024, 1536), card['file']

backup = None
if not VALIDATE_ONLY:
    before_presentation = presentation_defaults(cdo)
    previous_artwork = {card['property']: object_path(cdo.get_editor_property(card['property']))
                        for card in cards}
    destination_files = {asset_file(card['texture']) for card in cards}
    # Check existing category art, borders and upgrade tuning remain untouched.
    protected_files = set((ROOT / 'Content/HeavensDivide/Upgrades').rglob('*.uasset'))
    protected_files.add(ROOT / 'Content/HeavensDivide/Blueprints/BP_SurvivorPlayerController.uasset')
    for path in list(previous_artwork.values()) + list(before_presentation.values()):
        if path and path.startswith('/Game/'):
            protected_files.add(asset_file(path))
    protected_files -= destination_files
    protected_hashes = {str(path.relative_to(ROOT)): digest(path)
                        for path in sorted(protected_files) if path.is_file()}
    stamp = datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S_%f')
    backup = ROOT / 'Saved/Backups/CategoryArtwork' / stamp
    for path in [asset_file(MANIFEST['widget'])] + sorted(destination_files):
        if path.is_file():
            target = backup / path.relative_to(ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
    (backup / 'before.json').write_text(json.dumps({
        'artwork': previous_artwork,
        'presentation': before_presentation,
        'protected_file_hashes': protected_hashes,
    }, indent=2), encoding='utf-8')

    for card in cards:
        task = u.AssetImportTask()
        task.filename = str(ROOT / card['file'])
        task.destination_path, task.destination_name = card['texture'].rsplit('/', 1)
        task.automated = True
        task.replace_existing = True
        task.save = True
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        assert task.imported_object_paths, card['category']
        texture = u.load_asset(card['texture'])
        assert isinstance(texture, u.Texture2D), card['texture']
        settings = {
            'srgb': True,
            'lod_group': u.TextureGroup.TEXTUREGROUP_UI,
            'compression_settings': u.TextureCompressionSettings.TC_EDITOR_ICON,
            'mip_gen_settings': u.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            'address_x': u.TextureAddress.TA_CLAMP,
            'address_y': u.TextureAddress.TA_CLAMP,
            'never_stream': True,
            'max_texture_size': 0,
        }
        for name, value in settings.items():
            texture.set_editor_property(name, value)
        assert u.EditorAssetLibrary.save_loaded_asset(texture, False), card['texture']
        cdo.set_editor_property(card['property'], texture)

    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
    cdo = u.get_default_object(bp.generated_class())
    assert presentation_defaults(cdo) == before_presentation, 'Unrelated presentation defaults changed'
    changed = [rel for rel, expected in protected_hashes.items() if digest(ROOT / rel) != expected]
    assert not changed, 'Protected assets changed: ' + str(changed)
    (backup / 'verified.json').write_text(json.dumps({
        'protected_files_unchanged': len(protected_hashes),
        'borders_and_rarity_materials_unchanged': True,
    }, indent=2), encoding='utf-8')

verified = []
for card in cards:
    texture = u.load_asset(card['texture'])
    assert isinstance(texture, u.Texture2D), card['texture']
    assert cdo.get_editor_property(card['property']) == texture, card['property']
    assert (texture.blueprint_get_size_x(), texture.blueprint_get_size_y()) == (1024, 1536)
    assert texture.get_editor_property('srgb')
    assert texture.get_editor_property('lod_group') == u.TextureGroup.TEXTUREGROUP_UI
    assert texture.get_editor_property('compression_settings') == u.TextureCompressionSettings.TC_EDITOR_ICON
    assert texture.get_editor_property('mip_gen_settings') == u.TextureMipGenSettings.TMGS_NO_MIPMAPS
    assert texture.get_editor_property('address_x') == u.TextureAddress.TA_CLAMP
    assert texture.get_editor_property('address_y') == u.TextureAddress.TA_CLAMP
    assert texture.get_editor_property('never_stream')
    assert texture.get_editor_property('max_texture_size') == 0
    verified.append({
        'category': card['category'], 'property': card['property'],
        'texture': texture.get_path_name(), 'source': card['file'],
        'size': [1024, 1536], 'source_sha256': digest(ROOT / card['file']),
    })

report = {'validated_saved_assets': VALIDATE_ONLY, 'widget': MANIFEST['widget'],
          'backup': str(backup.relative_to(ROOT)) if backup else None, 'cards': verified}
output = ROOT / ('Saved/CategoryArtworkInspection/validation.json' if VALIDATE_ONLY
                 else 'Art/CategoryCards/import_report.json')
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2), encoding='utf-8')
u.log('CATEGORY_ARTWORK_' + ('VALIDATION' if VALIDATE_ONLY else 'IMPORT') + '_SUCCESS: 4 categories')
