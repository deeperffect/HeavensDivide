"""Editor viewport captures: -EnvBefore writes baseline images, otherwise After.
Run via -ExecutePythonScript (full editor), with -RenderOffscreen.
"""
from pathlib import Path
import json,time,unreal as u
u.EditorPythonScripting.set_keep_python_script_alive(True)
root=Path(u.Paths.project_dir()).resolve()
out=root/'Saved/EnvironmentPolish'/('Before' if '-EnvBefore' in u.SystemLibrary.get_command_line() else 'After');out.mkdir(parents=True,exist_ok=True)
level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
views=[('Lvl1_Overview','/Game/Maps/Lvl_B1_Lvl1',(7200,-1400,11500),(0,0,0),60),('Lvl1_Start','/Game/Maps/Lvl_B1_Lvl1',(3300,0,2950),(1413,0,0),60),('Lvl1_Gate','/Game/Maps/Lvl_B1_Lvl1',(450,2300,2900),(-2400,0,100),60),('Arena','/Game/Maps/Lvl_B1_Arena',(-440,1700,3200),(-2600,450,0),65)]
state={'index':0,'stage':'load','camera':None,'at':time.monotonic(),'start':time.monotonic()}
def tick(dt):
    global out
    if state.get("busy"): return
    state["busy"]=True
    try:
        if time.monotonic()-state['start']>480:raise RuntimeError('Capture timeout')
        if '-EnvApplyFirst' in u.SystemLibrary.get_command_line() and not state.get('applied'):
            exec(compile((root/'Tools/apply_environment_polish.py').read_text(), 'apply_environment_polish.py', 'exec'), {'__name__':'__main__'})
            state['applied']=True
        i=state['index']
        if i>=len(views):
            control=root/'Saved/EnvironmentPolish/ReviewCommand.json'
            keep=root/'Saved/EnvironmentPolish/KeepReviewOpen.txt'
            if keep.exists():
                if not state.get('review_ready'):
                    (root/'Saved/EnvironmentPolish/ReviewReady.json').write_text(json.dumps({'folder':str(out),'ready':True}))
                    state['review_ready']=True
                if not control.exists():return
                command=json.loads(control.read_text());control.unlink()
                if command['action']=='quit':keep.unlink()
                elif command['action']=='verify':
                    exec(compile((root/'Tools/verify_environment_polish.py').read_text(), 'verify_environment_polish.py', 'exec'), {'__name__':'__main__'});return
                elif command['action']=='apply':
                    exec(compile((root/'Tools/apply_environment_polish.py').read_text(), 'apply_environment_polish.py', 'exec'), {'__name__':'__main__'})
                    out=root/'Saved/EnvironmentPolish'/command.get('folder','After');out.mkdir(parents=True,exist_ok=True)
                    state.update(index=0,stage='load',start=time.monotonic(),review_ready=False);return
            if '-EnvApply' in u.SystemLibrary.get_command_line() and not state.get('applied'):
                exec(compile((root/'Tools/apply_environment_polish.py').read_text(), 'apply_environment_polish.py', 'exec'), {'__name__':'__main__'})
                out=root/'Saved/EnvironmentPolish/After';out.mkdir(parents=True,exist_ok=True)
                state.update(index=0,stage='load',applied=True,start=time.monotonic());return
            u.unregister_slate_post_tick_callback(state['handle']);u.log('ENVIRONMENT_CAPTURE_OK '+str(out));u.SystemLibrary.quit_editor();return
        name,map_path,pos,target,fov=views[i]
        if state['stage']=='load':
            if i==0 or views[i-1][1]!=map_path:assert level.load_level(map_path)
            camera=actors.spawn_actor_from_class(u.CameraActor,u.Vector(*pos),u.MathLibrary.find_look_at_rotation(u.Vector(*pos),u.Vector(*target)))
            camera.set_actor_label('TEMP Environment Review Camera')
            c=camera.get_component_by_class(u.CameraComponent);c.set_field_of_view(fov);c.set_editor_property('post_process_blend_weight',0.0)
            level.editor_set_viewport_realtime(True);level.editor_set_game_view(True);level.pilot_level_actor(camera)
            state.update(camera=camera,stage='warm',at=time.monotonic())
        elif state['stage']=='warm' and time.monotonic()-state['at']>7:
            state['task']=u.AutomationLibrary.take_high_res_screenshot(1600,1000,str(out/(name+'.png')),camera=state['camera'])
            state.update(stage='capture',at=time.monotonic())
        elif state['stage']=='capture' and time.monotonic()-state['at']>3 and (out/(name+'.png')).exists():
            level.eject_pilot_level_actor();actors.destroy_actor(state['camera']);state.update(index=i+1,stage='load')
    except Exception as e:
        import traceback
        u.log_error(traceback.format_exc());(out/'Error.txt').write_text(traceback.format_exc())
        if (root/'Saved/EnvironmentPolish/KeepReviewOpen.txt').exists():
            state.update(index=len(views),applied=True,review_ready=False,start=time.monotonic())
        else:
            u.unregister_slate_post_tick_callback(state['handle']);u.SystemLibrary.quit_editor()
    finally:
        state['busy']=False
state['handle']=u.register_slate_post_tick_callback(tick)
