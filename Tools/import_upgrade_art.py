"""Import generated card PNGs and change only artwork references; safe to resume."""
import json
import shutil
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
jobs = json.loads((root / 'Art/UpgradeCards/manifest.json').read_text(encoding='utf-8'))
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
completed, pending = [], []
for j in jobs:
    source = root / j['file']
    if not source.exists():
        pending.append(j['id'])
        continue
    card = unreal.load_asset(j['asset'])
    assert card, j['asset']
    assert str(card.get_editor_property('upgrade_id')) == j['id'], j['id']
    texture = unreal.load_asset(j['texture']) if unreal.EditorAssetLibrary.does_asset_exist(j['texture']) else None
    if not texture:
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path, task.destination_name = j['texture'].rsplit('/', 1)
        task.automated = True
        task.replace_existing = False
        task.save = True
        asset_tools.import_asset_tasks([task])
        texture = unreal.load_asset(j['texture'])
        assert isinstance(texture, unreal.Texture2D), j['id']
        texture.set_editor_property('srgb', True)
        texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        assert unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    if texture.get_editor_property('max_texture_size') != 512:
        texture.set_editor_property('max_texture_size', 512)
        assert unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    rel = Path(j['asset'].removeprefix('/Game/') + '.uasset')
    backup = root / 'Saved/Backups/UpgradeArtwork' / rel
    if not backup.exists():
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root / 'Content' / rel, backup)
    card.set_editor_property('card_artwork', texture)
    card.set_editor_property('icon', texture)
    assert unreal.EditorAssetLibrary.save_loaded_asset(card, False)
    assert card.get_editor_property('card_artwork') == texture
    assert card.get_editor_property('icon') == texture
    completed.append(j['id'])
report = {'assigned': completed, 'pending': pending}
(root / 'Art/UpgradeCards/import_report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log(f'UPGRADE_ART_IMPORT: {len(completed)} assigned, {len(pending)} pending')
