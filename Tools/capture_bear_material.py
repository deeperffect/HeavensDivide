"""Rendered verification of the saved bear material on its animated game mesh."""
from pathlib import Path
import json, time, traceback, runpy, unreal as u
root=Path(u.Paths.project_dir()).resolve();out=root/'Saved/BearMaterial'
u.EditorPythonScripting.set_keep_python_script_alive(True)
level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
state={'stage':'load','at':time.monotonic(),'start':time.monotonic(),'index':0}

def tick(dt):
    if state.get('busy'):return
    state['busy']=True
    try:
        if time.monotonic()-state['start']>240:raise RuntimeError('Bear capture timed out')
        if state['stage']=='load':
            runpy.run_path(str(root/'Tools/verify_enemy_palette.py'))
            assert level.load_level('/Game/Maps/Lvl_B1_Lvl1')
            bp=u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyBear')
            cdo=u.get_default_object(bp.generated_class())
            half=cdo.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_half_height()
            bear=actors.spawn_actor_from_class(bp.generated_class(),u.Vector(1413,0,half))
            mesh=bear.get_editor_property('mesh')
            assert mesh.get_material(0)==u.load_asset('/Game/Assets/EnemyCharacters/Bear/Enemy_Bear_Mat')
            mesh.set_update_animation_in_editor(True)
            mesh.play_animation(u.load_asset('/Game/Assets/EnemyCharacters/Bear/Walk'),False)
            mesh.set_position(.25,False);mesh.stop()
            assert u.SwapVFXSetupLibrary.refresh_preview_animation(mesh)
            center,extent,radius=u.SystemLibrary.get_component_bounds(mesh)
            span=max(extent.x,extent.y,extent.z,1)*2
            camera=actors.spawn_actor_from_class(u.CameraActor,center+u.Vector(span*1.8,-span*.65,span*.25))
            camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(camera.get_actor_location(),center),False)
            component=camera.get_component_by_class(u.CameraComponent)
            component.set_field_of_view(38);component.set_editor_property('post_process_blend_weight',0.)
            level.editor_set_viewport_realtime(True);level.editor_set_game_view(True);level.pilot_level_actor(camera)
            state.update(stage='warm',at=time.monotonic(),bear=bear,mesh=mesh,camera=camera,center=center,span=span)
        elif state['stage']=='warm' and time.monotonic()-state['at']>12:
            name='BearFront' if state['index']==0 else 'BearBack'
            state['task']=u.AutomationLibrary.take_high_res_screenshot(1280,900,str(out/(name+'.png')),camera=state['camera'])
            state.update(stage='capture',at=time.monotonic())
        elif state['stage']=='capture' and time.monotonic()-state['at']>3:
            name='BearFront' if state['index']==0 else 'BearBack'
            assert (out/(name+'.png')).exists()
            if state['index']==0:
                mesh=state['mesh'];mesh.set_position(.65,False)
                assert u.SwapVFXSetupLibrary.refresh_preview_animation(mesh)
                camera=state['camera'];center=state['center'];span=state['span']
                camera.set_actor_location(center+u.Vector(-span*1.8,span*.65,span*.25),False,False)
                camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(camera.get_actor_location(),center),False)
                state.update(index=1,stage='warm',at=time.monotonic())
            else:
                (out/'RenderChecks.json').write_text(json.dumps({'saved_material_assigned':True,'animated_poses':[.25,.65],'captures':['BearFront.png','BearBack.png']},indent=2))
                u.unregister_slate_post_tick_callback(state['handle'])
                u.log('BEAR_MATERIAL_RENDER_OK');u.SystemLibrary.quit_editor()
    except Exception:
        error=traceback.format_exc();u.log_error(error);(out/'RenderError.txt').write_text(error)
        u.unregister_slate_post_tick_callback(state['handle']);u.SystemLibrary.quit_editor()
    finally:state['busy']=False
state['handle']=u.register_slate_post_tick_callback(tick)
