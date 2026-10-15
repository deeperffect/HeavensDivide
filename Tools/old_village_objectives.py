"""Restore the existing survival event systems in the authored village."""
import unreal as u


def ensure_objectives(scale=1):
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    def one(cls, blueprint, label, position):
        existing=[a for a in actors.get_all_level_actors() if isinstance(a,cls)]
        assert len(existing)<=1, label
        asset=u.load_asset(blueprint)
        assert asset,blueprint
        actor=existing[0] if existing else actors.spawn_actor_from_class(asset.generated_class(),position)
        actor.set_actor_label(label);actor.set_folder_path('80_Gameplay/Objectives')
        return actor
    gate=one(u.BossToriiGate,'/Game/HeavensDivide/Blueprints/BP_BossToriiGate',
             'Boss gate - shrine court',u.Vector(3900*scale,3600*scale,0))
    gate.set_editor_property('boss_arena_level',u.load_asset('/Game/Maps/Lvl_B1_Arena'))
    gate.set_editor_property('unlock_run_time_seconds',600.)
    # This Blueprint's visual has a centered mesh pivot. Seat the visual while
    # keeping the interaction root at ground level; don't alter its shared BP.
    visual=gate.get_editor_property('gate_visual')
    center,extent,radius=u.SystemLibrary.get_component_bounds(visual)
    relative=visual.get_editor_property('relative_location')
    relative.z-=center.z-extent.z
    visual.set_relative_location(relative,False,False)
    gate.align_portal_to_gate()
    one(u.RunObjectiveDirector,'/Game/HeavensDivide/Blueprints/Objectives/BP_RunObjectiveDirector',
        'Village shrine objective director',u.Vector(0,0,100))
    encounter=one(u.EncounterDirector,'/Game/HeavensDivide/Blueprints/Encounters/BP_EncounterDirector',
                  'Village encounters - urns, marches and field events',u.Vector(0,0,100))
    for key in ['marching_enemy_class','healing_urn_class','magnet_urn_class']:
        assert encounter.get_editor_property(key), key
    existing=[a for a in actors.get_all_level_actors() if isinstance(a,u.ObjectiveSpawnPoint)]
    if not existing:
        for i,(east,north) in enumerate([(-3700,1500),(-3900,-900),(600,3100),(2300,2100),
                                        (2400,-2500),(0,-3500),(4300,3200),(-1800,4400)]):
            location=u.NavigationSystemV1.project_point_to_navigation(world,u.Vector(north*scale,east*scale,0),None,None,query_extent=u.Vector(500,500,120))
            assert location is not None, 'Objective point off navigation'
            location.z=0
            point=actors.spawn_actor_from_class(u.ObjectiveSpawnPoint,location)
            point.set_actor_label('Village objective clearing %02d'%(i+1))
            point.set_folder_path('80_Gameplay/Objectives/Spawn points')
