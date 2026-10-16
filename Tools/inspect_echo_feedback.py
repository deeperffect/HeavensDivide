from pathlib import Path
import json
import unreal as u

out=Path(u.Paths.project_saved_dir())/'FeedbackPolish'
out.mkdir(parents=True,exist_ok=True)
bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai')
attack=u.get_default_object(bp.generated_class()).get_component_by_class(u.AutoAttackComponent)
effect=attack.get_editor_property('blood_echo_vfx')
report={'effect':effect.get_path_name(),'delay':attack.get_editor_property('blood_echo_delay')}
task=u.AssetExportTask();task.object=effect;task.exporter=u.ObjectExporterT3D()
task.filename=str(out/'Echo.txt');task.automated=True;task.prompt=False
assert u.Exporter.run_asset_export_task(task)
(out/'Before.json').write_text(json.dumps(report,indent=2))
u.log('ECHO_FEEDBACK_INSPECTED '+json.dumps(report))
