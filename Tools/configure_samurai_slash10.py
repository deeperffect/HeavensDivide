"""Swap only the area-aware autoattack notify's system, preserving all tuning."""
from pathlib import Path
import shutil
import unreal as u

path = '/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_AutoAttackSamurai'
montage = u.load_asset(path)
system = u.load_asset('/Game/Assets/VFX/SlashesV1/Particles/NiagaraSystems/NS_Slash_10')
assert montage and system
events = list(u.AnimationLibrary.get_animation_notify_events(montage))
matches = [e.get_editor_property('notify') for e in events
           if isinstance(e.get_editor_property('notify'), u.AnimNotify_SpawnSamuraiSlashNiagara)
           and e.get_editor_property('notify').get_editor_property('attach_to_character_mesh')]
assert len(matches) == 1, 'Expected one area-aware, mesh-attached autoattack slash'
notify = matches[0]
fields = ['location_offset', 'rotation_offset', 'scale', 'area_bonus_scale_multiplier',
          'skeleton_socket_name', 'attach_to_character_mesh',
          'area_scaled_offset_parameter', 'area_scaled_float_parameters']
before = {field: str(notify.get_editor_property(field)) for field in fields}
timings = [u.AnimationLibrary.get_anim_notify_event_trigger_time(e) for e in events]
source = Path(u.Paths.project_content_dir(), path.removeprefix('/Game/') + '.uasset')
backup = Path(u.Paths.project_saved_dir(), 'Backups/SamuraiSlash10', source.name)
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(source, backup)
notify.set_editor_property('niagara_system', system)
assert before == {field: str(notify.get_editor_property(field)) for field in fields}
assert timings == [u.AnimationLibrary.get_anim_notify_event_trigger_time(e)
                   for e in u.AnimationLibrary.get_animation_notify_events(montage)]
assert u.EditorAssetLibrary.save_loaded_asset(montage, False)
out = Path(u.Paths.project_saved_dir(), 'GroundSlashInspection/slash10.copy').resolve()
out.parent.mkdir(parents=True, exist_ok=True)
task = u.AssetExportTask()
task.object = system
task.exporter = u.ObjectExporterT3D()
task.filename = str(out)
task.automated = True
task.prompt = False
task.replace_identical = True
assert u.Exporter.run_asset_export_task(task)
u.log('SAMURAI_SLASH10_PASS: replaced Niagara system; preserved area scaling, offsets and notify timing')
