"""Read-only exports of the vendor ground slash and the current blade wave."""
from pathlib import Path
import json
import unreal as u

out = Path(u.Paths.project_saved_dir(), 'GroundSlashInspection').resolve()
out.mkdir(parents=True, exist_ok=True)
paths = {
    'ground_blueprint': '/Game/Assets/VFX/GroundSlash/Blueprints/BP_GroundSlash',
    'spawner_blueprint': '/Game/Assets/VFX/GroundSlash/Blueprints/BP_Spawner',
    'blade_wave': '/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_SamuraiWaveProjectile',
    'niagara_v1': '/Game/Assets/VFX/GroundSlash/Particles/NiagaraSystems/NS_GroundSlashV1',
    'niagara_v2': '/Game/Assets/VFX/GroundSlash/Particles/NiagaraSystems/NS_GroundSlashV2',
    'niagara_tut': '/Game/Assets/VFX/GroundSlash/Particles/NiagaraSystems/NS_GroundSlashTut',
    'reduce_speed': '/Game/Assets/VFX/GroundSlash/Blueprints/ReduceSpeed',
    'follow_ground': '/Game/Assets/VFX/GroundSlash/Blueprints/FollowGround',
}
def export(obj, name):
    task = u.AssetExportTask()
    task.object = obj
    task.exporter = u.ObjectExporterT3D()
    task.filename = str(out / (name + '.copy'))
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    assert u.Exporter.run_asset_export_task(task), name
for name, path in paths.items():
    asset = u.load_asset(path)
    assert asset, path
    export(asset, name)
    if isinstance(asset, u.Blueprint):
        export(u.get_default_object(asset.generated_class()), name + '_defaults')
        if name in ['ground_blueprint', 'blade_wave']:
            actors = u.get_editor_subsystem(u.EditorActorSubsystem)
            actor = actors.spawn_actor_from_class(asset.generated_class(), u.Vector())
            assert actor
            export(actor, name + '_instance')
            rows = []
            for comp in actor.get_components_by_class(u.ActorComponent):
                row = {'name': comp.get_name(), 'class': comp.get_class().get_name()}
                for field in ['asset', 'relative_location', 'relative_rotation', 'relative_scale3d',
                              'initial_speed', 'max_speed', 'velocity', 'projectile_gravity_scale',
                              'Delay', 'ReduceSpeedRate', 'LineTraceDistance', 'OffsetFromGround',
                              'auto_activate']:
                    try:
                        row[field] = str(comp.get_editor_property(field))
                    except Exception:
                        pass
                rows.append(row)
            (out / (name + '_components.json')).write_text(json.dumps(rows, indent=2))
            actors.destroy_actor(actor)
u.log('GROUND_SLASH_INSPECT_PASS')

