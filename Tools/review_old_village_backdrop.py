"""Render the saved horizon and bamboo's lowest detail, then check real gameplay."""
from pathlib import Path
import json,time,traceback,runpy
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/OldVillage/BackdropReview';OUT.mkdir(parents=True,exist_ok=True)
L=u.get_editor_subsystem(u.LevelEditorSubsystem);A=u.get_editor_subsystem(u.EditorActorSubsystem)
u.EditorPythonScripting.set_keep_python_script_alive(True)
S={'stage':'load','index':0,'start':time.monotonic()};R={'passed':False}
views=[('NorthEdge',(6600,1600,3600),(11000,1800,0),65),
       ('WestEdge',(1500,-7200,3400),(3500,-11000,0),65),
       ('Waterfront',(-5200,-5000,3000),(-10500,-11000,-150),65),
       ('Overview',(-8500,-6000,12000),(2000,2000,0),62)]
def finish():
    (OUT/'Validation.json').write_text(json.dumps(R,indent=2));L.editor_request_end_play()
    u.unregister_slate_post_tick_callback(S['handle']);u.SystemLibrary.quit_editor()
def tick(dt):
    if S.get('busy'):return
    S['busy']=True
    try:
        now=time.monotonic();assert now-S['start']<600,'Review timeout'
        if S['stage']=='load':
            assert L.load_level('/Game/Maps/Lvl_OldVillage')
            actors=A.get_all_level_actors();bamboo=[a for a in actors if isinstance(a,u.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name().startswith('SM_TreeBamboo')]
            R['bamboo_count']=len(bamboo)
            SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
            R['bamboo_lods']={}
            for a in bamboo:
                mesh=a.static_mesh_component.static_mesh
                R['bamboo_lods'][mesh.get_name()]=[[SM.get_lod_material_slot(mesh,lod,i) for i in range(mesh.get_num_sections(lod))] for lod in range(SM.get_lod_count(mesh))]
            assert len(R['bamboo_lods'])==2 and all(0 in slots and 1 in slots for lods in R['bamboo_lods'].values() for slots in lods)
            # Verify the worst retained detail level explicitly, without changing the saved map.
            S['bamboo']=bamboo
            for a in bamboo:a.static_mesh_component.set_forced_lod_model(2)
            p=bamboo[0].get_actor_location();S['bamboo_position']=p
            views.append(('BambooLowestDetail',(p.x+1450,p.y-600,p.z+1900),(p.x,p.y,p.z+150),55))
            R['backdrop_components']=[]
            for a in actors:
                if not a.actor_has_tag('OldVillageBackdrop'):continue
                for c in a.get_components_by_class(u.PrimitiveComponent):
                    assert c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION
                    assert not c.get_editor_property('can_ever_affect_navigation')
                    R['backdrop_components'].append(c.get_path_name())
            S['stage']='view'
        if S['stage']=='view':
            name,pos,target,fov=views[S['index']]
            world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
            u.SystemLibrary.execute_console_command(world,'r.ViewDistanceScale '+('10' if name=='Overview' else '1'))
            camera=A.spawn_actor_from_class(u.CameraActor,u.Vector(*pos),u.MathLibrary.find_look_at_rotation(u.Vector(*pos),u.Vector(*target)))
            cc=camera.get_component_by_class(u.CameraComponent);cc.set_field_of_view(fov);cc.set_editor_property('post_process_blend_weight',0.)
            L.editor_set_viewport_realtime(True);L.editor_set_game_view(True);L.pilot_level_actor(camera)
            camera.set_actor_location(u.Vector(*pos),False,True);camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(u.Vector(*pos),u.Vector(*target)),False)
            u.AutomationLibrary.finish_loading_before_screenshot()
            S.update(camera=camera,stage='warm',at=time.monotonic());return
        if S['stage']=='warm' and now-S['at']>5:
            R.setdefault('camera_transforms',{})[views[S['index']][0]]=str(S['camera'].get_actor_transform())
            S['shot']=u.AutomationLibrary.take_high_res_screenshot(1600,1000,str(OUT/(views[S['index']][0]+'.png')),camera=S['camera'])
            S.update(stage='shot',at=now);return
        if S['stage']=='shot' and now-S['at']>3:
            L.eject_pilot_level_actor();A.destroy_actor(S['camera']);S['index']+=1
            if S['index']<len(views):S['stage']='view';return
            for a in S['bamboo']:a.static_mesh_component.set_forced_lod_model(0)
            L.editor_request_begin_play();S.update(stage='play',at=now);return
        if S['stage'] in ['warm','shot','view']:return
        world=u.EditorLevelLibrary.get_game_world()
        if not world or now-S['at']<5:return
        pawn=u.GameplayStatics.get_player_character(world,0)
        if S['stage']=='play':
            R['collision']=runpy.run_path(str(ROOT/'Tools/validate_old_village.py'),init_globals={'PLAY_WORLD':world})['report'];assert R['collision']['passed']
            for character in u.GameplayStatics.get_all_actors_of_class(world,u.CharacterBase):
                attack=character.get_component_by_class(u.AutoAttackComponent)
                if attack:attack.set_auto_attack_enabled(False)
                health=character.get_component_by_class(u.HealthComponent)
                if health:health.set_damage_enabled(False)
            p=S['bamboo_position'];pawn.set_actor_location(p+u.Vector(250,0,110),False,True)
            S.update(stage='game_bamboo',at=now);return
        if S['stage']=='game_bamboo':
            u.SystemLibrary.execute_console_command(world,'HighResShot 1600x1000 filename="'+str(OUT/'BambooGameplay.png').replace('\\','/')+'"')
            S.update(stage='done',at=now);return
        if S['stage']=='done':
            R['passed']=True;u.log('OLD_VILLAGE_BACKDROP_REVIEW_PASSED');finish()
    except Exception:R['error']=traceback.format_exc();u.log_error(R['error']);finish()
    finally:S['busy']=False
S['handle']=u.register_slate_post_tick_callback(tick)
