"""Run after SamuraiVFXScaleSetup -Basic. Preserve the montage's authored timing and transforms."""
import os
import shutil
import unreal

path = '/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_AutoAttackSamurai'
montage = unreal.load_asset(path)
basic = unreal.load_asset('/Game/Assets/VFX/SlashTrail_SoftTofu/Niagara/Basic/NS_SlashTrail_Basic')
assert montage and basic
states = [e.get_editor_property('notify_state_class') for e in unreal.AnimationLibrary.get_animation_notify_events(montage)]
states = [s for s in states if isinstance(s, unreal.AnimNotifyState_SamuraiSlashNiagara)
          and s.get_editor_property('niagara_system') == basic]
assert states, 'The saved auto-attack montage is not using the Basic trail'

task = unreal.AssetExportTask()
task.object = basic
task.exporter = unreal.ObjectExporterT3D()
task.filename = os.path.join(unreal.Paths.project_saved_dir(), 'samurai_basic_scale_verified.t3d')
task.automated = True
task.prompt = False
task.replace_identical = True
assert unreal.Exporter.run_asset_export_task(task)
with open(task.filename, 'rb') as f:
    data = f.read()
text = data.decode('utf-16' if data.startswith(bytes([255, 254])) else 'utf-8-sig')
assert 'ApplyOwnerScaleToAttributes.OwnerScale = Context.MapUpdate.Engine.Owner.Scale;' in text
assert 'Context.MapUpdate.Particles.RibbonWidth =' in text
assert 'ApplyOwnerScaleToAttributes' in text

source = os.path.join(unreal.Paths.project_content_dir(), path.removeprefix('/Game/') + '.uasset')
backup = os.path.join(unreal.Paths.project_saved_dir(), 'Backups', 'SamuraiBasicTrail', 'AM_AutoAttackSamurai.uasset')
os.makedirs(os.path.dirname(backup), exist_ok=True)
if not os.path.exists(backup):
    shutil.copy2(source, backup)
for state in states:
    state.set_editor_property('area_scaled_offset_parameter', 'User.Position Offset')
    unreal.log(f'Basic trail configured: socket={state.get_editor_property("skeleton_socket_name")}, area strength={state.get_editor_property("area_bonus_scale_multiplier")}')
assert unreal.EditorAssetLibrary.save_loaded_asset(montage, only_if_is_dirty=False)
unreal.log('Basic ribbon scale verified; montage offset binding saved.')
