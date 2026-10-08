"""Assign the user's Fang return slash without editing its montage or other Ninja tuning."""
from pathlib import Path
from datetime import datetime
import hashlib
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
path = '/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja'
montage_path = '/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Ninja/AM_FangSlash'
def disk(p): return root / 'Content' / (p.removeprefix('/Game/') + '.uasset')
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
montage_hash = digest(disk(montage_path))
backup = root / 'Saved/Backups/FangSlash' / datetime.now().strftime('%Y%m%d_%H%M%S_%f')
backup.mkdir(parents=True)
shutil.copy2(disk(path), backup / 'BP_Ninja.uasset')
bp = u.load_asset(path)
montage = u.load_asset(montage_path)
assert bp and montage
attack = u.get_default_object(bp.generated_class()).get_component_by_class(u.AutoAttackComponent)
before = {k: attack.get_editor_property(k) for k in ['attack_montage', 'alternate_attack_montage', 'fang_montage', 'fang_alternate_montage', 'projectile_class']}
attack.set_editor_property('fang_slash_montage', montage)
u.BlueprintEditorLibrary.compile_blueprint(bp)
attack = u.get_default_object(bp.generated_class()).get_component_by_class(u.AutoAttackComponent)
assert attack.get_editor_property('fang_slash_montage') == montage
assert all(attack.get_editor_property(k) == v for k, v in before.items())
assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
assert digest(disk(montage_path)) == montage_hash
u.log('FANG_SLASH_ASSIGNED: montage unchanged; other attack assignments preserved; backup=' + str(backup))
