"""Assign Iaijutsu release slashes. -ValidateIaijutsuPathSlashes is read-only."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
validate = '-ValidateIaijutsuPathSlashes' in u.SystemLibrary.get_command_line()
bp_path = '/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai'
effect_path = '/Game/Assets/VFX/SlashesV1/Particles/NiagaraSystems/NS_Slash_Iaijutsu'
bp = u.load_asset(bp_path)
effect = u.load_asset(effect_path)
assert bp and effect, 'Missing Samurai Blueprint or authored Iaijutsu slash'
attack = u.get_default_object(bp.generated_class()).get_component_by_class(u.AutoAttackComponent)
assert attack, 'Missing AutoAttackComponent'
preserved = {name: attack.get_editor_property(name) for name in [
    'iaijutsu_hit_vfx', 'iaijutsu_montage', 'iaijutsu_endpoint_montage',
    'iaijutsu_indicator_material', 'iaijutsu_indicator_color',
    'attack_montage', 'alternate_attack_montage', 'attack_interval',
    'crescent_montage', 'crescent_alternate_montage']}

if not validate:
    backup = root / 'Saved/Backups/IaijutsuPathSlashes' / datetime.now().strftime('%Y%m%d_%H%M%S')
    backup.mkdir(parents=True, exist_ok=True)
    source = root / 'Content/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai.uasset'
    shutil.copy2(source, backup / source.name)
    hashes = {p: hashlib.sha256(p.read_bytes()).hexdigest()
              for folder in ['Content/HeavensDivide/Upgrades',
                             'Content/HeavensDivide/Blueprints/PlayerCharacters/Montages',
                             'Content/Assets/VFX/SlashesV1']
              for p in (root / folder).rglob('*.uasset')}
    before = str(attack.get_editor_property('iaijutsu_path_slash_vfx'))
    # Preserve count, scale, rotation, delay, randomness and unrelated user tuning.
    attack.set_editor_property('iaijutsu_path_slash_vfx', effect)
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    attack = u.get_default_object(bp.generated_class()).get_component_by_class(u.AutoAttackComponent)

assert attack.get_editor_property('iaijutsu_path_slash_vfx') == effect
assert 0 <= attack.get_editor_property('iaijutsu_path_slash_count') <= 12
assert attack.get_editor_property('iaijutsu_path_slash_scale') >= 0
assert 0 <= attack.get_editor_property('iaijutsu_path_slash_delay') <= 1
randomness = attack.get_editor_property('iaijutsu_path_slash_rotation_randomness')
assert all(0 <= value <= 180 for value in [randomness.pitch, randomness.yaw, randomness.roll])
for name, original in preserved.items():
    assert attack.get_editor_property(name) == original, 'Unrelated property changed: ' + name
if not validate:
    assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
    for path, digest in hashes.items():
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, 'Unrelated asset changed: ' + str(path)
    (backup / 'assignment.json').write_text(json.dumps({
        'before': before, 'after': effect_path,
        'count': attack.get_editor_property('iaijutsu_path_slash_count'),
        'scale': attack.get_editor_property('iaijutsu_path_slash_scale'),
        'delay': attack.get_editor_property('iaijutsu_path_slash_delay'),
        'rotation_randomness': str(randomness),
        'preserved_file_count': len(hashes)}, indent=2), encoding='utf-8')
    u.log('IAIJUTSU_PATH_SLASHES_BACKUP: ' + str(backup))
u.log('IAIJUTSU_PATH_SLASHES_OK: %s, count=%s, scale=%s, delay=%s, randomness=%s' % (
    effect_path, attack.get_editor_property('iaijutsu_path_slash_count'),
    attack.get_editor_property('iaijutsu_path_slash_scale'),
    attack.get_editor_property('iaijutsu_path_slash_delay'), randomness))
