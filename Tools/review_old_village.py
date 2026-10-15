"""Render the new maps, audit supplied asset coverage, and smoke-test play mode."""
from pathlib import Path
import json,time,traceback,runpy
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/OldVillage/Review';OUT.mkdir(parents=True,exist_ok=True)
u.EditorPythonScripting.set_keep_python_script_alive(True)
L=u.get_editor_subsystem(u.LevelEditorSubsystem);A=u.get_editor_subsystem(u.EditorActorSubsystem)
views=[
 ('01_Overview','Lvl_OldVillage',(-13500,-1200,17500),(-500,0,0),55),
 ('02_ShrineCourt','Lvl_OldVillage',(1200,-2000,4200),(3700,600,100),65),
 ('03_Garden','Lvl_OldVillage',(-300,-7000,4200),(2600,-4300,0),65),
 ('04_Workshops','Lvl_OldVillage',(-4300,1200,4200),(-1400,4100,150),65),
 ('05_Waterfront','Lvl_OldVillage',(-7800,-6200,4300),(-3600,-3300,0),65),
 ('06_Blockout','Lvl_OldVillage_Blockout',(-13500,-1200,17500),(-500,0,0),55)]
scale=json.loads((ROOT/'Tools/old_village_layout.json').read_text()).get('layout_scale',1)
views=[(name,level,tuple(x*scale for x in pos),tuple(x*scale for x in target),fov) for name,level,pos,target,fov in views]
S={'index':0,'stage':'load','start':time.monotonic(),'at':time.monotonic()};report={}
def audit():
    actors=A.get_all_level_actors();seen={};errors=[]
    for a in actors:
        if isinstance(a,u.StaticMeshActor):
            c=a.static_mesh_component;m=c.static_mesh
            r=a.get_actor_rotation()
            if abs(r.pitch)>.01 or abs(r.roll)>.01:errors.append(a.get_actor_label()+' unexpected tilt')
            if not m:errors.append(a.get_actor_label()+' missing mesh');continue
            if m.get_name().startswith('SM_OV_'):
                seen[m.get_name()]=seen.get(m.get_name(),0)+1
                if not c.get_material(0):errors.append(a.get_actor_label()+' missing material')
    names=['SM_OV_'+x['name'] for x in json.loads((ROOT/'Art/Environment/OldVillage/SourceManifest.json').read_text())['assets']]
    errors += ['Unplaced supplied asset '+n for n in names if n not in seen]
    floor=next(a for a in actors if a.get_actor_label().startswith('Village ground'))
    center,extent=floor.get_actor_bounds(False)
    if not (extent.z<130 and extent.x>6400 and extent.y>6400):errors.append('Ground is not horizontal')
    report['asset_coverage']=seen;report['errors']=errors
    report['grounded_props']=sum(a.actor_has_tag('GroundedProp') for a in actors)
    report['floating_props']=[a.get_actor_label() for a in actors if a.actor_has_tag('GroundedProp') and abs(a.get_actor_bounds(False)[0].z-a.get_actor_bounds(False)[1].z)>1]
    report['ground_extent_cm']=[extent.x,extent.y]
    errors.extend('Floating prop '+name for name in report['floating_props'])
    report['gameplay_actors']={cls.__name__:sum(isinstance(a,cls) for a in actors) for cls in [u.RunObjectiveDirector,u.EncounterDirector,u.ObjectiveSpawnPoint,u.BossToriiGate,u.AmbientSound]}
    director=next(a for a in actors if isinstance(a,u.RunObjectiveDirector))
    report['saved_shrine_schedule']=[director.get_editor_property(k) for k in ['blood_shrine_spawn_time','second_blood_shrine_spawn_time']]
    if report['saved_shrine_schedule']!=[180.,300.]:errors.append('Saved objective timings changed')
    gate=next(a for a in actors if isinstance(a,u.BossToriiGate))
    gc,ge,gr=u.SystemLibrary.get_component_bounds(gate.get_editor_property('gate_visual'))
    report['gate_visual_bottom_cm']=gc.z-ge.z
    if abs(report['gate_visual_bottom_cm'])>1:errors.append('Gate visual is not grounded')
    report['api']={k:str(getattr(u.SystemLibrary,k,None).__doc__) for k in ['capsule_trace_single','line_trace_single']}
    report['play_api']={k:str(getattr(L,k,None).__doc__) for k in ['editor_request_begin_play','editor_request_end_play','is_in_play_in_editor']}
    (OUT/'Validation.json').write_text(json.dumps(report,indent=2))
    assert not errors,errors
def tick(dt):
    if S.get('busy'):return
    S['busy']=True
    try:
        if time.monotonic()-S['start']>600:raise RuntimeError('Review timeout')
        i=S['index']
        if i>=len(views):
            if S['stage']!='play':
                assert L.load_level('/Game/Maps/Lvl_OldVillage');audit()
                u.SystemLibrary.execute_console_command(u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world(),'r.ViewDistanceScale 1')
                L.editor_request_begin_play();S.update(stage='play',at=time.monotonic());return
            if time.monotonic()-S['at']<12:return
            world=u.EditorLevelLibrary.get_game_world()
            chars=u.GameplayStatics.get_all_actors_of_class(world,u.Character) if world else []
            pawn=u.GameplayStatics.get_player_character(world,0) if world else None
            if pawn and not S.get('motion_start'):
                S.update(motion_start=time.monotonic(),motion_position=pawn.get_actor_location());return
            if pawn and time.monotonic()-S['motion_start']<1.5:
                pawn.add_movement_input(u.Vector(0,1,0),1.,True);return
            report['play']={'world':str(world),'characters':[{'class':a.get_class().get_name(),'location':[a.get_actor_location().x,a.get_actor_location().y,a.get_actor_location().z]} for a in chars]}
            report['play']['movement_cm']=pawn.get_actor_location().distance(S['motion_position']) if pawn else 0
            report['play']['passed']=bool(chars) and all(a.get_actor_location().z>-50 for a in chars) and report['play']['movement_cm']>50
            # Commandlet editor worlds do not initialize a physics scene; probe PIE.
            probe=runpy.run_path(str(ROOT/'Tools/validate_old_village.py'),init_globals={'PLAY_WORLD':world})
            report['collision']=probe['report']
            (OUT/'Validation.json').write_text(json.dumps(report,indent=2))
            L.editor_request_end_play()
            assert report['play']['passed'] and report['collision']['passed'],'Play/collision validation failed; see Validation.json'
            u.unregister_slate_post_tick_callback(S['handle']);u.log('OLD_VILLAGE_REVIEW_COMPLETE')
            if globals().get('RUN_GAMEPLAY_CHECKS',False):
                def continue_checks(dt):
                    if L.is_in_play_in_editor():return
                    u.unregister_slate_post_tick_callback(S['next_handle'])
                    runpy.run_path(str(ROOT/'Tools/test_old_village_navigation_camera.py'),init_globals={'RUN_SPAWNING':True})
                S['next_handle']=u.register_slate_post_tick_callback(continue_checks)
            else:u.SystemLibrary.quit_editor()
            return
        name,mapname,pos,target,fov=views[i]
        if S['stage']=='load':
            if i==0 or views[i-1][1]!=mapname:assert L.load_level('/Game/Maps/'+mapname)
            u.SystemLibrary.execute_console_command(u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world(),'r.ViewDistanceScale 10')
            camera=A.spawn_actor_from_class(u.CameraActor,u.Vector(*pos),u.MathLibrary.find_look_at_rotation(u.Vector(*pos),u.Vector(*target)))
            camera.set_actor_label('TEMP Old Village review')
            c=camera.get_component_by_class(u.CameraComponent);c.set_field_of_view(fov);c.set_editor_property('post_process_blend_weight',0.)
            L.editor_set_viewport_realtime(True);L.editor_set_game_view(True);L.pilot_level_actor(camera)
            u.AutomationLibrary.finish_loading_before_screenshot()
            S.update(camera=camera,stage='warm',at=time.monotonic())
        elif S['stage']=='warm' and time.monotonic()-S['at']>6:
            file=OUT/(name+'.png')
            if file.exists():file.unlink()
            S['task']=u.AutomationLibrary.take_high_res_screenshot(1920,1200,str(file),camera=S['camera']);S.update(stage='capture',at=time.monotonic())
        elif S['stage']=='capture' and time.monotonic()-S['at']>2 and (OUT/(name+'.png')).exists():
            L.eject_pilot_level_actor();A.destroy_actor(S['camera']);S.update(index=i+1,stage='load')
    except Exception:
        error=traceback.format_exc();u.log_error(error);(OUT/'Error.txt').write_text(error)
        u.unregister_slate_post_tick_callback(S['handle']);u.SystemLibrary.quit_editor()
    finally:S['busy']=False
S['handle']=u.register_slate_post_tick_callback(tick)
