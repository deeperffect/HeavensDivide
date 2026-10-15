"""Idempotent survival-run setup shared by map builds and existing-map repair."""
import unreal as u

SPAWNER = '/Game/HeavensDivide/Blueprints/EnemyCharacters/BP_EnemySpawner'


def repair_spawn_collision():
    """Named profiles preserve the custom channels through save/reload and PIE."""
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    count = 0
    for actor in actors.get_all_level_actors():
        if not isinstance(actor, u.StaticMeshActor):
            continue
        component = actor.static_mesh_component
        profile = str(component.get_collision_profile_name())
        if component.get_collision_enabled() == u.CollisionEnabled.NO_COLLISION or profile == 'OldVillageCanopy':
            continue
        mesh = component.static_mesh
        is_floor = mesh and mesh.get_name() == 'SM_OV_Island'
        component.set_collision_profile_name('OldVillageGround' if is_floor else ('OldVillageOccluder' if profile == 'OldVillageOccluder' else 'OldVillageSolid'))
        assert component.get_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY) == u.CollisionResponseType.ECR_BLOCK
        if is_floor:
            assert component.get_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY_GROUND) == u.CollisionResponseType.ECR_BLOCK
        count += 1
    return count


def ensure_run_spawner():
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    existing = [a for a in actors.get_all_level_actors() if isinstance(a, u.EnemySpawner)]
    assert len(existing) <= 1, 'Multiple run clocks/spawners; resolve manually before saving.'
    if existing:
        return existing[0]
    blueprint = u.load_asset(SPAWNER)
    assert blueprint, SPAWNER
    actor = actors.spawn_actor_from_class(blueprint.generated_class(), u.Vector(0, 0, 100))
    assert actor
    actor.set_actor_label('Old Village - Enemy Spawner and Run Timer')
    actor.set_folder_path('80_Gameplay')
    assert actor.get_editor_property('spawning_enabled')
    assert actor.get_editor_property('run_time_update_interval') > 0
    assert len(actor.get_editor_property('enemy_spawn_entries')) > 0
    assert len(actor.get_editor_property('pressure_phases')) > 0
    # Authored roster/phase defaults are shared with the original gameplay map.
    # EnemyGround traces and collision validation constrain spawns to this island.
    return actor
