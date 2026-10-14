"""Restore Level 1's grass/dirt appearance from before the terracotta pass.

Run in Unreal Python. Saves only Level 1, with a dated backup. The newer art
assets and hidden rim stay available. APPLY=False only verifies the loaded map.
"""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/RestoreArenaGround'
MAP = '/Game/Maps/Lvl_B1_Lvl1'
GROUND = '/Game/HeavensDivide/Materials/EnvironmentPolish/MI_Lvl1_Ground'
STAMPS = '/Game/HeavensDivide/Materials/ReferenceGround/MI_RockStamp_'
APPLY = globals().get('APPLY', True)
LEVEL = u.get_editor_subsystem(u.LevelEditorSubsystem)
ACTORS = u.get_editor_subsystem(u.EditorActorSubsystem)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def transform_values(transform):
    if isinstance(transform, tuple):
        transform = transform[-1]
    p, q, s = transform.translation, transform.rotation, transform.scale3d
    return [p.x, p.y, p.z, q.x, q.y, q.z, q.w, s.x, s.y, s.z]


def actor_transforms():
    return {a.get_path_name(): transform_values(a.get_actor_transform())
            for a in ACTORS.get_all_level_actors()}


def collision_snapshot():
    return {c.get_path_name(): [str(c.get_collision_profile_name()), str(c.get_collision_enabled())]
            for a in ACTORS.get_all_level_actors()
            for c in a.get_components_by_class(u.PrimitiveComponent)}


def show(component, visible):
    component.modify(True)
    component.set_visibility(visible, True)
    component.set_hidden_in_game(not visible, True)


def expected_transform(p):
    return u.Transform(location=u.Vector(p['x'], p['y'], p['z']),
                       rotation=u.Rotator(pitch=0, yaw=p['yaw'], roll=0),
                       scale=u.Vector(p['width'] / 100, p['height'] / 100, 1))


if APPLY:
    OUT.mkdir(parents=True, exist_ok=True)
    protected = [p for p in (ROOT / 'Content/Maps').glob('*.umap')
                 if p.name != 'Lvl_B1_Lvl1.umap']
    for folder in ['EnvironmentPolish', 'ReferenceGround', 'StylizedGround',
                   'TerracottaArena', 'ArenaVariations']:
        protected += list((ROOT / 'Content/HeavensDivide/Materials' / folder).glob('*.uasset'))
    hashes = {str(p.relative_to(ROOT)): digest(p) for p in protected}
    # Restore scene assignments using the currently selected grass material
    # revision; the user may have restored its earlier color/blending version.
    assert LEVEL.load_level(MAP)
    assert LEVEL.set_current_level_by_name('Lvl_B1_Lvl1')

actors = ACTORS.get_all_level_actors()
floor = next(a for a in actors if a.get_actor_label() == 'Plane' and isinstance(a, u.StaticMeshActor))
assert MAP + '.' in floor.get_path_name(), 'Open Level 1 before restoring'
slabs = next(a for a in actors if a.get_actor_label() == 'GroundRockStamps')
rim = next(a for a in actors if a.get_actor_label() == 'TerracottaArenaRim')
fences = [c for a in actors for c in a.get_components_by_class(u.StaticMeshComponent)
          if c.static_mesh and c.static_mesh.get_name() == 'SM_ShrineFence03a']
ground = u.load_asset(GROUND)
materials = [u.load_asset(STAMPS + str(i)) for i in range(4)]
assert ground and all(materials)
layout = json.loads((ROOT / 'Art/ReferenceGround/RockScatterLayout.json').read_text())
groups = []
for component in slabs.get_components_by_class(u.HierarchicalInstancedStaticMeshComponent):
    index = int(component.get_material(0).get_name().rsplit('_', 1)[1])
    placements = [p for p in layout['placements'] if int(p['variant']) == index]
    assert len(placements) == component.get_instance_count()
    groups.append((component, index, placements))
assert len(groups) == 4 and sum(len(p) for _, _, p in groups) == 65
assert len(fences) == 262

if APPLY:
    before, collisions = actor_transforms(), collision_snapshot()
    file = ROOT / 'Content/Maps/Lvl_B1_Lvl1.umap'
    backup = OUT / 'Backups' / datetime.now().strftime('%Y%m%d_%H%M%S_%f') / file.relative_to(ROOT)
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(file, backup)
    with u.ScopedEditorTransaction('Restore pre-terracotta grass arena'):
        floor.modify(True)
        floor.static_mesh_component.modify(True)
        floor.static_mesh_component.set_material(0, ground)
        slabs.modify(True)
        for component, index, placements in groups:
            show(component, True)
            component.set_material(0, materials[index])
            for i, p in enumerate(placements):
                assert component.update_instance_transform(i, expected_transform(p), True, True, True)
        rim.modify(True)
        for component in rim.get_components_by_class(u.MeshComponent):
            show(component, False)
        for component in fences:
            component.get_owner().modify(True)
            show(component, True)
    assert before == actor_transforms(), 'An actor transform changed'
    assert collisions == collision_snapshot(), 'Collision changed'
    assert LEVEL.save_current_level()
    assert all(digest(ROOT / p) == h for p, h in hashes.items()), 'A protected asset changed'

assert floor.static_mesh_component.get_material(0) == ground
for component, index, placements in groups:
    assert component.get_material(0) == materials[index]
    assert component.get_editor_property('visible') and not component.get_editor_property('hidden_in_game')
    for i, p in enumerate(placements):
        actual = component.get_instance_transform(i, True)
        if isinstance(actual, tuple):
            actual = actual[-1]
        expected = expected_transform(p)
        # Compare geometry: equivalent rotations may return opposite quaternion signs.
        for corner in [u.Vector(), u.Vector(50, 50, 0), u.Vector(-50, 50, 0)]:
            a = u.MathLibrary.transform_location(actual, corner)
            b = u.MathLibrary.transform_location(expected, corner)
            error = max(abs(a.x - b.x), abs(a.y - b.y), abs(a.z - b.z))
            assert error < .01, (index, i, error, str(a), str(b))
assert all(c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') for c in fences)
assert all(not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
           for c in rim.get_components_by_class(u.MeshComponent))
REPORT = dict(passed=True, map=MAP, material=ground.get_path_name(),
              restored_stamps=65, visible_fences=len(fences), arena_rim_hidden=True,
              floor_collision=str(floor.static_mesh_component.get_collision_profile_name()))
if APPLY:
    REPORT.update(backup=str(backup.relative_to(ROOT)), protected_assets=hashes,
                  actor_transforms_unchanged=True, collision_unchanged=True)
    (OUT / 'Applied.json').write_text(json.dumps(REPORT, indent=2))
u.log('GRASS_ARENA_RESTORED ' + json.dumps({k: v for k, v in REPORT.items() if k != 'protected_assets'}))
