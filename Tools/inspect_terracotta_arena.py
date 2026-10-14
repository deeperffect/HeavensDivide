"""Read-only checks of the loaded terracotta arena, included in capture reports."""
import unreal as u

actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
REPORT = dict(passed=False, slabs=0, rim_rocks=0, hidden_fences=0, max_slab_z_error=0.)
for label, count in [('GroundRockStamps', 65), ('TerracottaArenaRim', 155)]:
    matches = [a for a in actors if a.get_actor_label() == label]
    assert len(matches) == 1, label
    total = 0
    for component in matches[0].get_components_by_class(u.HierarchicalInstancedStaticMeshComponent):
        assert component.get_collision_enabled() == u.CollisionEnabled.NO_COLLISION
        assert not component.get_editor_property('can_ever_affect_navigation')
        assert component.get_material(0).get_path_name().startswith('/Game/HeavensDivide/Materials/TerracottaArena/')
        total += component.get_instance_count()
        for index in range(component.get_instance_count()):
            transform = component.get_instance_transform(index, world_space=True)
            if isinstance(transform, (tuple, list)):
                transform = transform[-1]
            if label == 'GroundRockStamps':
                for x in [-50., 50.]:
                    for y in [-50., 50.]:
                        p = u.MathLibrary.transform_location(transform, u.Vector(x, y, 0.))
                        error = abs(p.z - 1.3)
                        REPORT['max_slab_z_error'] = max(REPORT['max_slab_z_error'], error)
                        assert error < .05, 'Stone slab tilted below the floor'
    assert total == count, label
    REPORT['slabs' if label == 'GroundRockStamps' else 'rim_rocks'] = total
for actor in actors:
    for component in actor.get_components_by_class(u.StaticMeshComponent):
        if component.static_mesh and component.static_mesh.get_name() == 'SM_ShrineFence03a':
            assert not component.is_visible(), 'Old fence is visible'
            REPORT['hidden_fences'] += 1
assert REPORT['hidden_fences'] == 262
floor = next(a for a in actors if a.get_actor_label() == 'Plane')
REPORT['floor_collision'] = str(floor.static_mesh_component.get_collision_profile_name())
assert REPORT['floor_collision'] == 'BlockAll'
REPORT['passed'] = True
