"""Assign the verified mirrored throw and its left-hand release bone."""
import unreal as u
from pathlib import Path
import shutil
import json

root=Path(u.Paths.project_dir()).resolve()
path='/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja'
backup=root/'Saved/Backups/NinjaAlternatingThrow'
backup.mkdir(parents=True,exist_ok=True)
source=root/'Content/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja.uasset'
if not (backup/source.name).exists():
    shutil.copy2(source,backup/source.name)
bp=u.load_asset(path)
cdo=u.get_default_object(bp.generated_class())
attack=cdo.get_component_by_class(u.AutoAttackComponent)
original=attack.get_editor_property('attack_montage')
right_socket=attack.get_editor_property('projectile_spawn_socket')
alternate=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Ninja/AM_AutoAttackNinja_Left')
assert alternate
assert cdo.get_editor_property('mesh').does_socket_exist('LeftHand')
attack.set_editor_property('alternate_attack_montage',alternate)
attack.set_editor_property('alternate_projectile_spawn_socket','LeftHand')
u.BlueprintEditorLibrary.compile_blueprint(bp)
attack=u.get_default_object(bp.generated_class()).get_component_by_class(u.AutoAttackComponent)
assert attack.get_editor_property('attack_montage')==original
assert attack.get_editor_property('projectile_spawn_socket')==right_socket
assert attack.get_editor_property('alternate_attack_montage')==alternate
assert str(attack.get_editor_property('alternate_projectile_spawn_socket'))=='LeftHand'
assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
(backup/'assignment.json').write_text(json.dumps({'right':original.get_path_name(),'left':alternate.get_path_name(),
    'right_socket':str(right_socket),'left_socket':'LeftHand'},indent=2))
u.log('NINJA_ALTERNATING_THROW_ASSIGNED')
