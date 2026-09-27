"""Check editor lane helpers on the saved Blueprint without modifying assets."""
from pathlib import Path
import json
import unreal as u

actors = u.get_editor_subsystem(u.EditorActorSubsystem)
cls = u.EditorAssetLibrary.load_blueprint_class('/Game/HeavensDivide/Blueprints/Objectives/BP_SamuraiTechniqueTrial')
actor = actors.spawn_actor_from_class(cls, u.Vector())
assert actor
report = []
try:
    components = {c.get_name(): c for c in actor.get_components_by_class(u.SceneComponent)}
    for name in ['LeftLane', 'CenterLane', 'RightLane']:
        lane = components[name]
        box = components[name + 'Bounds']
        assert isinstance(box, u.BoxComponent)
        assert box.get_attach_parent() == lane
        assert box.get_editor_property('visible')
        assert box.get_editor_property('hidden_in_game')
        assert box.get_editor_property('is_editor_only')
        assert box.get_collision_enabled() == u.CollisionEnabled.NO_COLLISION
        assert box.get_unscaled_box_extent() == u.Vector(50, 50, 50)
        assert box.get_editor_property('relative_location') == u.Vector()
        assert box.get_editor_property('relative_scale3d') == u.Vector(1, 1, 1)
        # Exercise the saved lane's transform and confirm its child follows.
        lane.set_relative_location(u.Vector(125, 250, 5), False, False)
        lane.set_relative_rotation(u.Rotator(0, 27, 0), False, False)
        lane.set_relative_scale3d(u.Vector(4, 9, .03))
        for axis in ['x', 'y', 'z']:
            assert abs(getattr(box.get_world_location(), axis) - getattr(lane.get_world_location(), axis)) < .01
        for axis in ['pitch', 'yaw', 'roll']:
            assert abs(getattr(box.get_world_rotation(), axis) - getattr(lane.get_world_rotation(), axis)) < .01
        for axis in ['x', 'y', 'z']:
            assert abs(getattr(box.get_scaled_box_extent(), axis) - 50 * abs(getattr(lane.get_world_scale(), axis))) < .01
        report.append({'lane': name, 'editor_only': True, 'follows_transform': True})
finally:
    actors.destroy_actor(actor)
out = Path(u.Paths.project_saved_dir(), 'SamuraiLaneIndicator', 'bounds_verification.json')
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2))
u.log('SAMURAI_LANE_BOUNDS_PASS ' + json.dumps(report))
