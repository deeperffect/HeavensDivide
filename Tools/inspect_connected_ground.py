"""Read-only fresh-load checks for the connected ground pass."""
from pathlib import Path
import hashlib
import json
import re
import runpy
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/ConnectedGround'
FOLDER='/Game/HeavensDivide/Materials/ConnectedGround'
LIB=u.MaterialEditingLibrary
ACTORS=u.get_editor_subsystem(u.EditorActorSubsystem)
applied=json.loads((OUT/'Applied.json').read_text())
config=json.loads((ROOT/'Tools/connected_ground.json').read_text())
actual=runpy.run_path(str(ROOT/'Tools/inspect_layered_ground.py'),init_globals={'SNAPSHOT_ONLY':True})['REPORT']
expected=applied['scene_snapshot']
for scene in (actual,expected):
    for actor in scene.values():
        for component in actor['components'].values():
            if 'materials' in component:
                component['materials']=[re.sub(r'(\.MID_[^.]+)_\d+$',r'\1',p) if p else None for p in component['materials']]
if actual!=expected:
    changes={k:dict(expected=expected.get(k),actual=actual.get(k)) for k in set(expected)|set(actual) if expected.get(k)!=actual.get(k)}
    (OUT/'SceneDifferences.json').write_text(json.dumps(changes,indent=2))
    raise AssertionError('Unrelated saved scene differs: '+str(list(changes)))
assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==h for p,h in applied['protected_assets'].items())
assert hashlib.sha256((ROOT/'Art/ConnectedGround/ConnectedSlabs_BaseColor.png').read_bytes()).hexdigest()==applied['source_sha256']
actors=ACTORS.get_all_level_actors()
floor=next(a for a in actors if a.get_actor_label()=='Plane')
material=floor.static_mesh_component.get_material(0)
assert material.get_path_name().split('.')[0]==FOLDER+'/MI_ConnectedGround'
assert material.get_base_material().get_name()=='M_ConnectedGround'
assert str(floor.static_mesh_component.get_collision_profile_name())==applied['floor_collision']
for name,value in config.items():
    if isinstance(value,list):
        color=LIB.get_material_instance_vector_parameter_value(material,name)
        assert all(abs(a-b)<.0001 for a,b in zip([color.r,color.g,color.b,color.a],value)),name
    else:assert abs(LIB.get_material_instance_scalar_parameter_value(material,name)-value)<.0001,name
texture=LIB.get_material_instance_texture_parameter_value(material,'GroundTexture')
assert texture.get_name()=='T_ConnectedSlabs'
assert texture.get_editor_property('srgb') and not texture.get_editor_property('never_stream')
assert texture.get_editor_property('mip_gen_settings')==u.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP
stamps=next(a for a in actors if a.get_actor_label()=='GroundRockStamps')
components=stamps.get_components_by_class(u.HierarchicalInstancedStaticMeshComponent)
assert [c.get_instance_count() for c in components]==applied['retained_hidden_stamp_counts']
assert all(not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game') for c in components)
assert all(c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION and not c.get_editor_property('can_ever_affect_navigation') for c in components)
REPORT=dict(passed=True,unrelated_scene_unchanged=True,protected_asset_count=len(applied['protected_assets']),
            retained_hidden_stamps=sum(c.get_instance_count() for c in components),fixed_world_tile_cm=config['TileSize'],
            collision_unchanged=True,normal_texture_streaming=True)
u.log('CONNECTED_GROUND_SCENE_OK '+json.dumps(REPORT))
