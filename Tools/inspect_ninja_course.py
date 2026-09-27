import json
from pathlib import Path
import unreal
bp = unreal.load_asset('/Game/HeavensDivide/Blueprints/Objectives/BP_NinjaTechniqueTrial')
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actor = sub.spawn_actor_from_class(bp.generated_class(), unreal.Vector(0,0,0))
rows = []
for comp in actor.get_components_by_class(unreal.SceneComponent):
    row = dict(name=comp.get_name(), cls=comp.get_class().get_name(), location=str(comp.get_editor_property('relative_location')), rotation=str(comp.get_editor_property('relative_rotation')), scale=str(comp.get_editor_property('relative_scale3d')), parent=comp.get_attach_parent().get_name() if comp.get_attach_parent() else '')
    if isinstance(comp, unreal.StaticMeshComponent):
        row['mesh'] = str(comp.get_editor_property('static_mesh'))
    if isinstance(comp, unreal.ChildActorComponent):
        child = comp.get_child_actor()
        row['child'] = str(child)
        if child:
            row['properties'] = {}
            for key in ['damage','initial_delay','telegraph_duration','active_duration','cooldown_duration','movement_direction','movement_distance','sweep_duration','delay_between_sweeps','open_duration','closed_duration','transition_duration']:
                try: row['properties'][key] = str(child.get_editor_property(key))
                except Exception: pass
    rows.append(row)
Path(unreal.Paths.project_dir(), 'Saved/NinjaCourseBefore.json').write_text(json.dumps(rows, indent=2))
sub.destroy_actor(actor)
unreal.log('NINJA_COURSE_INSPECT_OK')
