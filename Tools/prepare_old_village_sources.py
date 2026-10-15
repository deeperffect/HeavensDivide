"""Safely stage Desktop Tripo ZIPs into the project; originals are read-only."""
from pathlib import Path, PurePosixPath
import hashlib, json, shutil, zipfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path(r'C:\Users\deepe\OneDrive\Desktop\NewLevel')
ART = ROOT / 'Art/Environment/OldVillage'
SPECS = [
 ('circular+stone+paving+3d+model.zip','Landmarks','CourtyardMedallion',500,'width'),
 ('curved+tile+roof+3d+model.zip','Architecture','TiledRoof',600,'width'),
 ('sacred+rock+3d+model.zip','Landmarks','SacredStone',280,'height'),
 ('shrine+3d+model (1).zip','Buildings','SmallShrine',600,'width'),
 ('shrine+3d+model.zip','Buildings','MainShrine',2000,'width'),
 ('stone+circular+platform+3d+model.zip','Landmarks','AltarPlatform',550,'width'),
 ('stone+guardian+statue+3d+model.zip','Landmarks','GuardianStatue',180,'height'),
 ('stone+railing+3d+model.zip','Boundaries','RetainingWallStraight',400,'width'),
 ('stone+wall+3d+model.zip','Boundaries','RetainingWallCorner',400,'width'),
 ('stylized+architectural+gate+3d+model.zip','Boundaries','RoofedBoundaryWall',500,'width'),
 ('stylized+shrine+3d+model.zip','Buildings','RaisedStorehouse',700,'width'),
 ('stylized+wooden+gate+3d+model.zip','Architecture','SlidingDoor',250,'width'),
 ('thatched+cottage+3d+model.zip','Buildings','ThatchedCottage',900,'width'),
 ('thatched+hut+3d+model.zip','Architecture','ThatchedRoof',650,'width'),
 ('thatched+shelter+3d+model.zip','Buildings','ThatchedShelter',650,'width'),
 ('torii+gate+3d+model.zip','Buildings','ShrineGatehouse',1000,'width'),
 ('traditional+house+3d+model.zip','Buildings','OpenWorkshop',850,'width'),
 ('wooden+bridge+3d+model.zip','Boundaries','WaterfrontWalkway',450,'width'),
 ('wooden+crate+3d+model.zip','Props','OfferingBox',130,'width'),
 ('wooden+frame+3d+model.zip','Architecture','WallPanel',250,'width'),
 ('wooden+gate+3d+model.zip','Architecture','ShutterWindow',150,'width'),
]
manifest = {'source': str(SOURCE), 'units': 'centimeters', 'assets': []}
for zipname, category, name, target, axis in SPECS:
    archive = SOURCE / zipname
    original = ART / '_Original' / name
    original.mkdir(parents=True, exist_ok=True)
    entries = []
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None, zipname
        for item in z.infolist():
            rel = PurePosixPath(item.filename.replace('\\', '/'))
            assert not rel.is_absolute() and '..' not in rel.parts and ':' not in item.filename
            dest = (original / str(rel)).resolve()
            assert dest.is_relative_to(original.resolve())
            if item.is_dir():
                dest.mkdir(parents=True, exist_ok=True)
                continue
            assert item.file_size < 100_000_000
            dest.parent.mkdir(parents=True, exist_ok=True)
            data = z.read(item)
            if dest.exists():
                assert dest.read_bytes() == data, f'Existing source differs: {dest}'
            else:
                dest.write_bytes(data)
            entries.append(str(dest.relative_to(ROOT)))
    fbx = list(original.rglob('*.fbx'))
    texture = list(original.rglob('*.jpg'))
    assert len(fbx) == len(texture) == 1
    folder = ART / category / name
    (folder / 'Textures').mkdir(parents=True, exist_ok=True)
    tex = folder / 'Textures' / f'T_OV_{name}_BaseColor.jpg'
    shutil.copy2(texture[0], tex)
    manifest['assets'].append(dict(name=name,category=category,zip=zipname,
        zip_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(), original_files=entries,
        original_fbx=str(fbx[0].relative_to(ROOT)), texture=str(tex.relative_to(ROOT)),
        prepared_fbx=str((folder / f'SM_OV_{name}.fbx').relative_to(ROOT)),
        target_cm=target, scale_axis=axis, yaw_degrees=0))
(ART / 'SourceManifest.json').write_text(json.dumps(manifest,indent=2))
print(f'Staged {len(manifest["assets"])} ZIPs under {ART}')
