"""Read-only verification of persisted build-family artwork assignments."""
import hashlib
import json
import struct
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
jobs = json.loads((root / 'Art/UpgradeCards/manifest.json').read_text(encoding='utf-8'))
assert len(jobs) == 160
assert len({j['id'] for j in jobs}) == len(jobs)
digests = set()
for j in jobs:
    data = (root / j['file']).read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', j['id']
    width, height = struct.unpack('>II', data[16:24])
    assert width == height and width >= 512, (j['id'], width, height)
    digest = hashlib.sha256(data).hexdigest()
    assert digest not in digests, f"Duplicate image: {j['id']}"
    digests.add(digest)
    card = unreal.load_asset(j['asset'])
    texture = unreal.load_asset(j['texture'])
    assert card and isinstance(texture, unreal.Texture2D), j['id']
    assert card.get_editor_property('card_artwork') == texture, j['id']
    assert card.get_editor_property('icon') == texture, j['id']
    assert texture.get_editor_property('max_texture_size') == 512, j['id']
    assert texture.get_editor_property('lod_group') == unreal.TextureGroup.TEXTUREGROUP_UI, j['id']
unreal.log(f'UPGRADE_ART_VALIDATION: {len(jobs)} unique images and persisted card/icon assignments passed')
