import json, unreal as u
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();out=root/'Saved/ArenaGroundFix'
level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert level.load_level('/Game/Maps/Lvl_B1_Arena')
all_actors=actors.get_all_level_actors();boss=next(a for a in all_actors if isinstance(a,u.CinderOracleBoss))
report={'boss':{'location':str(boss.get_actor_location()),'capsule_half':boss.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_half_height()},'components':[],'traces':{}}
for a in all_actors:
    for c in a.get_components_by_class(u.PrimitiveComponent):
        if c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION:continue
        center,extent,radius=u.SystemLibrary.get_component_bounds(c)
        if extent.x<150 and extent.y<150:continue
        data={'actor':a.get_actor_label(),'class':a.get_class().get_name(),'component':c.get_name(),
              'center':str(center),'extent':str(extent),'location':str(a.get_actor_location()),
              'object_type':str(c.get_collision_object_type()),
              'collision_profile':str(c.get_collision_profile_name()),
              'enemy_ground_response':str(c.get_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY_GROUND))}
        if isinstance(c,u.StaticMeshComponent):
            mesh=c.get_editor_property('static_mesh');data['mesh']=mesh.get_path_name() if mesh else None
            data['materials']=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())]
        report['components'].append(data)
report['trace_enum_names']=dir(u.TraceTypeQuery)
(out/'ArenaInspection.json').write_text(json.dumps(report,indent=2))
for name,offset in [('boss',(0,0)),('player',(800,0)),('guardian',(280,210)),('side',(0,800))]:
    p=boss.get_actor_location()+u.Vector(offset[0],offset[1],0)
    hits=u.SystemLibrary.line_trace_multi(boss,p+u.Vector(0,0,1500),p-u.Vector(0,0,3000),u.TraceTypeQuery.ECC_ENEMY_GROUND,False,[boss],u.DrawDebugTrace.NONE,True)
    report['traces'][name]=str(hits)
(out/'ArenaInspection.json').write_text(json.dumps(report,indent=2))
u.log('ARENA_GROUND_INSPECTION_OK')
