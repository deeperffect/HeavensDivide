"""Unsaved before/after upper-half dissolve and calibrated Blood slash previews."""
from pathlib import Path
import json,time,traceback
import unreal as u
root=Path(u.Paths.project_dir()).resolve();out=root/'Saved/RunFixes/Previews';out.mkdir(parents=True,exist_ok=True)
L=u.get_editor_subsystem(u.LevelEditorSubsystem);A=u.get_editor_subsystem(u.EditorActorSubsystem)
assert L.load_level('/Game/Maps/Lvl_OldVillage')
houses=[a for a in A.get_all_level_actors() if a.get_actor_label().startswith('ThatchedCottage')]
house=min(houses,key=lambda a:a.get_actor_location().length_squared())
mesh=house.get_component_by_class(u.StaticMeshComponent)
(out/'Occluder.json').write_text(json.dumps({'actor':house.get_path_name(),'material':str(mesh.get_material(0)),
    'defaults':str(mesh.get_editor_property('custom_primitive_data')),
    'fade_index':mesh.get_custom_primitive_data_index_for_scalar_parameter('HD_CameraOcclusion')},indent=2))
mesh.set_custom_primitive_data_float(31,0.)
center,extent,_=u.SystemLibrary.get_component_bounds(mesh)
distance=max(extent.x,extent.y)*3.5
pos=center+u.Vector(distance,-distance,extent.z*.4)
cam=A.spawn_actor_from_class(u.CameraActor,pos,u.MathLibrary.find_look_at_rotation(pos,center))
cam.get_component_by_class(u.CameraComponent).set_field_of_view(50)
L.editor_set_viewport_realtime(True);L.editor_set_game_view(True);L.pilot_level_actor(cam)
S={'i':0,'at':time.monotonic(),'stage':'warm','camera':cam,'fx':None}
views=['OccluderWhole','OccluderUpperHalf','BloodWarmup','BloodUpgraded','BloodBase']
calibration=json.loads((root/'Saved/RunFixes/BloodCalibration.json').read_text())
def tick(dt):
    if S.get('busy'):return
    S['busy']=True
    try:
        now=time.monotonic();name=views[S['i']]
        if S['stage']=='warm' and now-S['at']>12:
            if S['fx']:
                S['fx'].set_paused(False)
                S['fx'].reinitialize_system()
                S['fx'].activate(True)
                S['fx'].advance_simulation(12,.01)
                S['fx'].set_paused(True)
            S.update(stage='ready',at=now);return
        if S['stage']=='ready' and now-S['at']>1:
            S['shot']=u.AutomationLibrary.take_high_res_screenshot(1440,1000,str(out/(name+'.png')),camera=S['camera']);S.update(stage='shot',at=now);return
        if S['stage']=='shot' and now-S['at']>3 and S['shot'].is_task_done():
            S['i']+=1
            if S['i']==len(views):
                u.unregister_slate_post_tick_callback(S['handle']);u.SystemLibrary.quit_editor();return
            if S['i']==1:mesh.set_custom_primitive_data_float(31,1.)
            else:
                if S['fx']:A.destroy_actor(S['effect'])
                if S['i']==2:
                    p=next(a for a in A.get_all_level_actors() if isinstance(a,u.PlayerStart)).get_actor_location()
                    S['origin']=u.Vector(p.x+120,p.y,p.z)
                    top=S['origin']+u.Vector(0,0,2200)
                    S['camera'].set_actor_location_and_rotation(top,u.MathLibrary.find_look_at_rotation(top,S['origin']),False,True)
                    S['camera'].get_component_by_class(u.CameraComponent).set_field_of_view(55)
                effect=A.spawn_actor_from_class(u.NiagaraActor,S['origin'])
                radius=240.*(2.5 if S['i']==3 else 1)
                effect.set_actor_scale3d(u.Vector(radius/calibration['radius'],radius/calibration['radius'],radius/calibration['radius']))
                fx=effect.get_component_by_class(u.NiagaraComponent)
                fx.set_asset(u.load_asset('/Game/Assets/VFX/SlashesV1/Particles/NiagaraSystems/NS_Slash_BloodStance'))
                fx.set_force_solo(True)
                fx.set_age_update_mode(u.NiagaraAgeUpdateMode.TICK_DELTA_TIME)
                world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
                u.SystemLibrary.flush_persistent_debug_lines(world)
                u.SystemLibrary.draw_debug_circle(world,S['origin'],radius,96,u.LinearColor(0,1,0,1),60.,3.,u.Vector(1,0,0),u.Vector(0,1,0),False)
                S.update(fx=fx,effect=effect)
            S.update(stage='warm',at=now)
    except Exception:
        (out/'Error.txt').write_text(traceback.format_exc());u.log_error(traceback.format_exc());u.unregister_slate_post_tick_callback(S['handle']);u.SystemLibrary.quit_editor()
    finally:S['busy']=False
u.EditorPythonScripting.set_keep_python_script_alive(True);S['handle']=u.register_slate_post_tick_callback(tick)
