"""Read-only inspection of the fire system and the trial's authored components."""
import json
from pathlib import Path
import unreal as u

out = Path(u.Paths.project_saved_dir(), 'SamuraiFire').resolve()
out.mkdir(parents=True, exist_ok=True)
path = '/Game/Assets/VFX/StylizedSmokeV1/Particles/NiagaraSystems/NS_StylizedSmoke_Loop_v08_Fire'
system = u.load_asset(path)
assert system
task = u.AssetExportTask()
task.object = system
task.filename = str(out / 'fire.copy')
task.automated = True
task.prompt = False
task.replace_identical = True
u.Exporter.run_asset_export_task(task)
bp = u.load_asset('/Game/HeavensDivide/Blueprints/Objectives/BP_SamuraiTechniqueTrial')
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
actor = actors.spawn_actor_from_class(bp.generated_class(), u.Vector())
rows = []
for comp in actor.get_components_by_class(u.NiagaraComponent):
    rows.append(dict(name=comp.get_name(), asset=str(comp.get_asset()),
                     rotation=str(comp.get_editor_property('relative_rotation')),
                     world_rotation=str(comp.get_world_rotation()),
                     scale=str(comp.get_editor_property('relative_scale3d')),
                     absolute_rotation=comp.get_editor_property('absolute_rotation'),
                     parent=comp.get_attach_parent().get_name() if comp.get_attach_parent() else None))
actors.destroy_actor(actor)
(out / 'components.json').write_text(json.dumps(rows, indent=2))
u.log('SAMURAI_FIRE_INSPECT ' + json.dumps(rows))
