"""Bake static routes and prepare isolated camera-fade material copies."""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import shutil
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
DEST = '/Game/HeavensDivide/Environment/OldVillage/CameraFade'
E = u.EditorAssetLibrary
M = u.MaterialEditingLibrary
ACT = u.get_editor_subsystem(u.EditorActorSubsystem)
LEVEL = u.get_editor_subsystem(u.LevelEditorSubsystem)


def fade_material(source, cache):
    path = source.get_path_name()
    if path.startswith(DEST + '/'):
        return source
    if path in cache:
        return cache[path]
    name = source.get_name() + '_Fade_' + hashlib.sha1(path.encode()).hexdigest()[:8]
    target = DEST + '/' + name
    copy = E.load_asset(target) if E.does_asset_exist(target) else E.duplicate_asset(path, target)
    assert copy, path
    if isinstance(copy, u.MaterialInstanceConstant):
        parent = fade_material(source.get_editor_property('parent'), cache)
        M.set_material_instance_parent(copy, parent)
        overrides = copy.get_editor_property('base_property_overrides')
        overrides.set_editor_property('override_blend_mode', False)
        copy.set_editor_property('base_property_overrides', overrides)
        M.update_material_instance(copy)
    elif isinstance(copy, u.Material):
        assert u.CameraOcclusionMaterialLibrary.prepare_material(copy), path
    else:
        raise RuntimeError('Unsupported scenery material: ' + path)
    assert E.save_loaded_asset(copy, False)
    cache[path] = copy
    return copy


def prepare_current_level():
    cache = {}
    changed = []
    for actor in ACT.get_all_level_actors():
        if actor.actor_has_tag('OldVillageBackdrop'):
            continue
        if not isinstance(actor, u.StaticMeshActor):
            continue
        component = actor.static_mesh_component
        mesh = component.static_mesh
        if not mesh or not component.is_visible() or actor.get_editor_property('hidden'):
            continue
        if mesh.get_name() in ['SM_OV_Island', 'SM_OV_Water']:
            continue
        # Closed buildings should not create isolated walkable pockets under
        # their roofs. Open shelters/workshops/gates retain their passages.
        if mesh.get_name() in ['SM_OV_ThatchedCottage', 'SM_OV_RaisedStorehouse']:
            component.set_editor_property('fill_collision_underneath_for_navmesh', True)
        center, extent = actor.get_actor_bounds(False)
        # Ground decorations and low props cannot cover the character's body.
        if extent.z < 75 or center.z + extent.z < 140:
            continue
        for slot in range(component.get_num_materials()):
            source = component.get_material(slot)
            if source:
                component.set_material(slot, fade_material(source, cache))
        tags = list(component.get_editor_property('component_tags'))
        if 'CameraFade' not in [str(t) for t in tags]:
            tags.append('CameraFade')
        component.set_editor_property('component_tags', tags)
        profile = str(component.get_collision_profile_name())
        visual_only = component.get_collision_enabled() == u.CollisionEnabled.NO_COLLISION or profile == 'OldVillageCanopy'
        component.set_collision_profile_name('OldVillageCanopy' if visual_only else 'OldVillageOccluder')
        changed.append(actor.get_actor_label())
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    assert u.EnemyNavigationLibrary.bake_old_village_navigation(world), 'Navigation bake failed'
    for actor in ACT.get_all_level_actors():
        if isinstance(actor, u.EnemySpawner):
            actor.set_editor_property('require_nav_mesh_projection', True)
            actor.set_editor_property('nav_mesh_projection_extent', u.Vector(250,250,100))
    navigation = [a for a in ACT.get_all_level_actors() if isinstance(a, u.RecastNavMesh)]
    assert len(navigation) == 1, 'Expected one baked navigation mesh'
    nav = navigation[0]
    assert nav.get_editor_property('runtime_generation') == u.RuntimeGenerationType.STATIC
    assert LEVEL.save_current_level()
    return {'occluders': len(changed), 'material_copies': len(cache), 'labels': changed,
            'material_sources': {source: copy.get_path_name() for source, copy in cache.items()},
            'navigation_radius': nav.get_editor_property('agent_radius'),
            'navigation_generation': str(nav.get_editor_property('runtime_generation')),
            'actors': len(ACT.get_all_level_actors())}


if __name__ == '__main__':
    backup = ROOT / 'Saved/OldVillage/Backups' / (datetime.now().strftime('%Y%m%d_%H%M%S') + '_NavigationCamera')
    backup.mkdir(parents=True, exist_ok=True)
    report = {}
    for name in ['Lvl_OldVillage', 'Lvl_OldVillage_Blockout']:
        disk = ROOT / 'Content/Maps' / (name + '.umap')
        shutil.copy2(disk, backup / disk.name)
        assert LEVEL.load_level('/Game/Maps/' + name)
        report[name] = prepare_current_level()
    (ROOT / 'Saved/OldVillage/NavigationCameraSetup.json').write_text(json.dumps(report, indent=2))
    u.log('OLD_VILLAGE_NAVIGATION_CAMERA_SETUP_COMPLETE')
