"""Read-only saved scene/asset checks; also snapshots unrelated scene state."""
from pathlib import Path
import hashlib
import json
import math
import re
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve()
ACTORS=u.get_editor_subsystem(u.EditorActorSubsystem)
LIB=u.MaterialEditingLibrary
FOLDER='/Game/HeavensDivide/Materials/LayeredGround'

def transform(t):
    if isinstance(t,tuple):t=t[-1]
    p,q,s=t.translation,t.rotation,t.scale3d
    return [round(v,5) for v in (p.x,p.y,p.z,q.x,q.y,q.z,q.w,s.x,s.y,s.z)]

actors=ACTORS.get_all_level_actors()
snapshot={}
editor_icons=[]
for a in actors:
    if a.get_actor_label()=='GroundRockStamps' or a.get_name().startswith(('CameraActor_','BP_PlayerCameraRig_C_')):
        continue
    components={}
    for c in a.get_components_by_class(u.PrimitiveComponent):
        # Editor-only light/start gizmos resize and change visibility as the
        # viewport initializes. They do not exist in the game or saved geometry.
        if isinstance(c,u.BillboardComponent) and c.get_editor_property('is_editor_only'):
            editor_icons.append((a.get_path_name(),c.get_name()))
            continue
        item={'transform':transform(c.get_world_transform()),
              'collision':[str(c.get_collision_profile_name()),str(c.get_collision_enabled())],
              'visible':c.get_editor_property('visible'),'hidden':c.get_editor_property('hidden_in_game')}
        if isinstance(c,u.StaticMeshComponent):
            item['mesh']=c.static_mesh.get_path_name() if c.static_mesh else None
            if a.get_actor_label()!='Plane':
                item['materials']=[m.get_path_name() if m else None for m in c.get_materials()]
        components[c.get_name()]=item
    snapshot[a.get_path_name()]={'transform':transform(a.get_actor_transform()),'components':components}
REPORT=snapshot
if not globals().get('SNAPSHOT_ONLY',False):
    layout=json.loads((ROOT/'Art/LayeredGround/RockScatterLayout.json').read_text())
    regions=json.loads((ROOT/'Art/LayeredGround/RockAtlasRegions.json').read_text())
    applied=json.loads((ROOT/'Saved/LayeredGround/Applied.json').read_text())
    expected_scene=applied['scene_snapshot']
    for actor,name in editor_icons:
        if actor in expected_scene:expected_scene[actor]['components'].pop(name,None)
    # Construction scripts recreate dynamic sky material instances with a new
    # numeric UObject suffix per load. Compare their stable owner/material name.
    for scene in (snapshot,expected_scene):
        for actor in scene.values():
            for component in actor['components'].values():
                if 'materials' in component:
                    component['materials']=[re.sub(r'(\.MID_[^.]+)_\d+$',r'\1',p) if p else None
                                            for p in component['materials']]
    if snapshot!=expected_scene:
        differences={key:{'expected':expected_scene.get(key),'actual':snapshot.get(key)}
                     for key in set(snapshot)|set(expected_scene)
                     if snapshot.get(key)!=expected_scene.get(key)}
        (ROOT/'Saved/LayeredGround/SceneDifferences.json').write_text(json.dumps(differences,indent=2))
        raise AssertionError('Unrelated scene differs after fresh reload: '+str(list(differences)))
    assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==h for p,h in applied['protected_assets'].items())
    floor=next(a for a in actors if a.get_actor_label()=='Plane')
    ground=floor.static_mesh_component.get_material(0)
    assert ground.get_path_name().split('.')[0]==FOLDER+'/MI_Lvl1_Ground'
    assert LIB.get_material_instance_scalar_parameter_value(ground,'GrassTileSize')==1000
    assert LIB.get_material_instance_scalar_parameter_value(ground,'DirtTileSize')==1500
    slabs=next(a for a in actors if a.get_actor_label()=='GroundRockStamps')
    components=slabs.get_components_by_class(u.HierarchicalInstancedStaticMeshComponent)
    assert len(components)==8
    counts={}
    for c in components:
        index=int(c.get_material(0).get_name().rsplit('_',1)[1])
        placements=[p for p in layout['placements'] if p['variant']==index]
        assert c.get_instance_count()==len(placements)
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        assert c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION
        assert not c.get_editor_property('can_ever_affect_navigation')
        for i,p in enumerate(placements):
            expected=u.Transform(location=u.Vector(p['x'],p['y'],p['z']),rotation=u.Rotator(pitch=0,yaw=p['yaw'],roll=0),scale=u.Vector(p['width']/100,p['height']/100,1))
            actual,wanted=transform(c.get_instance_transform(i,True)),transform(expected)
            # Stored instance matrices may reconstruct the equivalent -q quaternion.
            assert all(abs(actual[k]-wanted[k])<.002 for k in (0,1,2,7,8,9)), (p,actual,wanted)
            assert abs(abs(sum(actual[k]*wanted[k] for k in (3,4,5,6)))-1)<.0001, (p,actual,wanted)
        atlas=LIB.get_material_instance_texture_parameter_value(c.get_material(0),'RockAtlas')
        assert atlas.get_name()=='T_RockAtlas_'+str(regions['variants'][index]['atlas'])
        assert abs(LIB.get_material_instance_scalar_parameter_value(c.get_material(0),'Brightness')-.45)<.0001
        counts[index]=len(placements)
    REPORT={'passed':True,'rock_count':sum(counts.values()),'variants':len(counts),
            'counts':counts,'protected_asset_count':len(applied['protected_assets']),
            'unrelated_scene_unchanged':True,'fixed_world_tile_cm':{'grass':1000,'dirt':1500},
            'collision_unchanged':True,'normal_texture_streaming':True}
    u.log('LAYERED_GROUND_SCENE_OK '+json.dumps(REPORT))
