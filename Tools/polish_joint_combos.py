"""Update only the joint combo pacing/description fields, preserving authored effects."""
import json
import shutil
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir())
backup = root / 'Saved/JointComboPolish/AssetBackups'
backup.mkdir(parents=True, exist_ok=True)
descriptions = {
    'BloodMoon': 'A Blood slash and arcing kunai volley. Blood upgrades apply to the slash; Barrage upgrades apply to the volley.',
    'HeavensDivide': 'Ninja repeatedly throws lightning-fast Fangs into the charging Iaijutsu lanes. Samurai then dashes through the slash. Matching Iaijutsu and Fang upgrades empower their own half.',
    'Moonwell': 'Ninja throws a Great Shuriken. Samurai infuses it with Crescent energy; four Crescent slashes fire in a cross that turns with the shuriken. Matching Shuriken and Crescent upgrades empower their own half.',
}
results = []
for identity, description in descriptions.items():
    disk = root / 'Content/HeavensDivide/Abilities/Combos' / ('DA_Combo_' + identity + '.uasset')
    if not (backup / disk.name).exists():
        shutil.copy2(disk, backup / disk.name)
    asset = u.load_asset('/Game/HeavensDivide/Abilities/Combos/DA_Combo_' + identity)
    assert isinstance(asset, u.ComboAbilityDefinition)
    properties = dict(release_recovery=.42, fang_throw_interval=.14, cinematic_world_speed=.1, description=description)
    if identity == 'Moonwell':
        properties['count'] = 4
    for key, value in properties.items():
        asset.set_editor_property(key, value)
    assert u.EditorAssetLibrary.save_loaded_asset(asset, False)
    results.append(dict(id=identity, **properties))
(root / 'Saved/JointComboPolish/Assets.json').write_text(json.dumps(results, indent=2))
print('Joint combo polish saved: ' + str(len(results)))
