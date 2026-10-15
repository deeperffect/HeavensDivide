"""Close-up scenery review, actual PIE portal and wall-before-text validation."""
from pathlib import Path
import json,time,traceback,runpy
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/OldVillage/PolishReview';OUT.mkdir(parents=True,exist_ok=True)
L=u.get_editor_subsystem(u.LevelEditorSubsystem);A=u.get_editor_subsystem(u.EditorActorSubsystem)
u.EditorPythonScripting.set_keep_python_script_alive(True)
S={'stage':'load','start':time.monotonic(),'index':0};R={'passed':False}

def spawn(world,cls,position,rotation):
    library=u.get_default_object(u.GameplayStatics);transform=u.Transform(location=position,rotation=rotation)
    a=library.call_method('BeginDeferredActorSpawnFromClass',args=(world,cls,transform,u.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,u.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    library.call_method('FinishSpawningActor',args=(a,transform,u.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));return a

def finish():
    (OUT/'Validation.json').write_text(json.dumps(R,indent=2));L.editor_request_end_play()
    u.unregister_slate_post_tick_callback(S['handle']);u.SystemLibrary.quit_editor()

def tick(dt):
    if S.get('busy'):return
    S['busy']=True
    try:
        assert time.monotonic()-S['start']<360,'Polish review timeout'
        now=time.monotonic()
        if S['stage']=='load':
            assert L.load_level('/Game/Maps/Lvl_OldVillage')
            actors=A.get_all_level_actors()
            S['views']=[('Bamboo',next(a for a in actors if a.get_actor_label()=='Compound bamboo screen')),
                        ('Bushes',next(a for a in actors if a.get_actor_label()=='Planted shrub'))]
            gate=next(a for a in actors if isinstance(a,u.BossToriiGate))
            gate.set_editor_property('unlock_run_time_seconds',0.) # Unsaved test instance.
            S['stage']='view'
        if S['stage']=='view':
            name,actor=S['views'][S['index']];c,e=actor.get_actor_bounds(False)
            target=actor.get_actor_location()+u.Vector(0,0,240) if name=='Bamboo' else u.Vector(c.x,c.y,max(100,c.z))
            pos=target+u.Vector(-850,-850,650)
            camera=A.spawn_actor_from_class(u.CameraActor,pos,u.MathLibrary.find_look_at_rotation(pos,target))
            camera.get_component_by_class(u.CameraComponent).set_field_of_view(55)
            L.editor_set_viewport_realtime(True);L.editor_set_game_view(True);L.pilot_level_actor(camera)
            u.AutomationLibrary.finish_loading_before_screenshot()
            S.update(camera=camera,stage='warm',at=now);return
        if S['stage']=='warm' and now-S['at']>5:
            S['shot']=u.AutomationLibrary.take_high_res_screenshot(1600,1000,str(OUT/(S['views'][S['index']][0]+'.png')),camera=S['camera'])
            S.update(stage='shot',at=now);return
        if S['stage']=='shot' and now-S['at']>3:
            L.eject_pilot_level_actor();A.destroy_actor(S['camera']);S['index']+=1
            if S['index']<len(S['views']):S['stage']='view';return
            L.editor_request_begin_play();S.update(stage='play',at=now);return
        if S['stage'] in ['view','warm','shot']:return
        world=u.EditorLevelLibrary.get_game_world()
        if not world or now-S['at']<3:return
        pc=u.GameplayStatics.get_player_controller(world,0);pawn=u.GameplayStatics.get_player_character(world,0)
        if S['stage']=='play':
            if u.GameplayStatics.get_time_seconds(world)<1.5:return # Let spawn emergence finish even during shader hitches.
            for character in u.GameplayStatics.get_all_actors_of_class(world,u.CharacterBase):
                attack=character.get_component_by_class(u.AutoAttackComponent)
                if attack:attack.set_auto_attack_enabled(False)
                health=character.get_component_by_class(u.HealthComponent)
                if health:health.set_damage_enabled(False)
            for enemy in u.GameplayStatics.get_all_actors_of_class(world,u.EnemyBase):enemy.destroy_actor()
            actors=u.GameplayStatics.get_all_actors_of_class(world,u.StaticMeshActor)
            R['solid_prop_traces']={}
            for label in ['Household cart','Workshop storage barrel','Compound entrance lantern','Household garden bench']:
                actor=next(a for a in actors if a.get_actor_label()==label)
                c,e=actor.get_actor_bounds(False)
                hit=u.SystemLibrary.line_trace_single_by_profile(world,c+u.Vector(0,0,e.z+25),c,'Pawn',False,[],u.DrawDebugTrace.NONE,False)
                R['solid_prop_traces'][label]=bool(hit) and any(isinstance(v,u.Actor) and v==actor for v in u.get_default_object(u.GameplayStatics).call_method('BreakHitResult',args=(hit,)))
            assert all(R['solid_prop_traces'].values()),R['solid_prop_traces']
            assert all(a.static_mesh_component.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION or a.static_mesh_component.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN)!=u.CollisionResponseType.ECR_BLOCK for a in actors if a.get_actor_label()=='Storage pottery')
            R['routes']=runpy.run_path(str(ROOT/'Tools/validate_old_village.py'),init_globals={'PLAY_WORLD':world})['report']
            assert R['routes']['passed'],'Main route clearance failed'
            gate=u.GameplayStatics.get_all_actors_of_class(world,u.BossToriiGate)[0]
            visual=gate.get_editor_property('gate_visual');vfx=gate.get_editor_property('unlock_vfx')
            c,e,r=u.SystemLibrary.get_component_bounds(visual);p=vfx.get_world_location()
            R['portal']={'center':[p.x,p.y,p.z],'gate_bottom':c.z-e.z,'gate_top':c.z+e.z,'active':vfx.is_active()}
            assert abs(p.z-(c.z-e.z+e.z*2*.38))<1 and abs(p.x-c.x)<1 and abs(p.y-c.y)<1 and vfx.is_active(),R['portal']
            bounds=visual.static_mesh.get_bounds()
            normal=gate.get_actor_forward_vector() if bounds.box_extent.x<bounds.box_extent.y else gate.get_actor_right_vector()
            position=p+normal*1500+u.Vector(0,0,130)
            camera=spawn(world,u.CameraActor,position,u.MathLibrary.find_look_at_rotation(position,p))
            camera.get_component_by_class(u.CameraComponent).set_field_of_view(55)
            S['original_camera']=pc.get_view_target();pc.set_view_target_with_blend(camera,0.)
            S.update(runtime_camera=camera,stage='portal',at=now);return
        if S['stage']=='portal':
            u.SystemLibrary.execute_console_command(world,'HighResShot 1600x1000 filename="'+str(OUT/'Portal.png').replace('\\','/')+'"')
            S.update(stage='wall_start',at=now);return
        if S['stage']=='wall_start':
            pc.set_view_target_with_blend(S['original_camera'],0.);S['runtime_camera'].destroy_actor()
            pawn.set_actor_location(u.Vector(4242.64,1414.21,110),False,True)
            encounter=u.GameplayStatics.get_all_actors_of_class(world,u.EncounterDirector)[0]
            assert encounter.spawn_wall_trap(),'Wall did not fit the clear court'
            assert not u.GameplayStatics.is_game_paused(world),'Intro paused before the ring rose'
            S.update(stage='wall_rise',at=now);return
        if S['stage']=='wall_rise':
            walls=u.GameplayStatics.get_all_actors_of_class(world,u.EncounterWall)
            if u.GameplayStatics.is_game_paused(world):
                R['wall']={'segments':len(walls),'visible':sum(a.get_editor_property('stone').is_visible() for a in walls),
                           'blocking_enemies':sum(a.get_editor_property('barrier').get_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY)==u.CollisionResponseType.ECR_BLOCK for a in walls)}
                assert len(walls)>0 and R['wall']['visible']==len(walls) and R['wall']['blocking_enemies']==len(walls)
                u.SystemLibrary.execute_console_command(world,'HighResShot 1600x1000 filename="'+str(OUT/'WallIntro.png').replace('\\','/')+'"')
                S.update(stage='done',at=now)
            elif now-S['at']>8:raise RuntimeError('Wall intro never appeared after reveal')
            return
        if S['stage']=='done':
            R['passed']=True;u.log('OLD_VILLAGE_POLISH_REVIEW_PASSED');finish()
    except Exception:
        R['error']=traceback.format_exc();u.log_error(R['error']);finish()
    finally:S['busy']=False
S['handle']=u.register_slate_post_tick_callback(tick)
