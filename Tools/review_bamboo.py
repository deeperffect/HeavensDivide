"""Unsaved bamboo material/LOD comparison using the village lighting."""
from pathlib import Path
import json,time,traceback
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/OldVillage/BambooReview';OUT.mkdir(parents=True,exist_ok=True)
L=u.get_editor_subsystem(u.LevelEditorSubsystem);A=u.get_editor_subsystem(u.EditorActorSubsystem)
SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
u.EditorPythonScripting.set_keep_python_script_alive(True)
S={'stage':'load','index':0};R={}
def finish():
    (OUT/'Inspection.json').write_text(json.dumps(R,indent=2));u.unregister_slate_post_tick_callback(S['handle']);u.SystemLibrary.quit_editor()
def tick(dt):
    if S.get('busy'):return
    S['busy']=True
    try:
        now=time.monotonic()
        if S['stage']=='load':
            assert L.load_level('/Game/Maps/Lvl_OldVillage')
            src=next(a for a in A.get_all_level_actors() if a.get_actor_label()=='Compound bamboo screen')
            c=src.static_mesh_component;mesh=c.static_mesh
            R['mesh']=mesh.get_path_name();R['nanite']=str(SM.get_nanite_settings(mesh));R['lods']=[]
            for lod in range(SM.get_lod_count(mesh)):
                row={'lod':lod,'verts':SM.get_number_verts(mesh,lod),'sections':[]}
                for sec in range(mesh.get_num_sections(lod)):
                    row['sections'].append({'section':sec,'slot':SM.get_lod_material_slot(mesh,lod,sec)})
                R['lods'].append(row)
            R['materials']=[str(c.get_material(i)) for i in range(c.get_num_materials())]
            a=A.spawn_actor_from_class(u.StaticMeshActor,u.Vector(4242,1414,0));a.set_actor_scale3d(src.get_actor_scale3d());a.static_mesh_component.set_static_mesh(mesh)
            for i in range(c.get_num_materials()):a.static_mesh_component.set_material(i,c.get_material(i))
            a.static_mesh_component.set_editor_property('evaluate_world_position_offset',False)
            target=a.get_actor_location()+u.Vector(0,0,220);pos=target+u.Vector(-1100,-350,1000)
            camera=A.spawn_actor_from_class(u.CameraActor,pos,u.MathLibrary.find_look_at_rotation(pos,target))
            camera.get_component_by_class(u.CameraComponent).set_field_of_view(45)
            L.editor_set_viewport_realtime(True);L.editor_set_game_view(True);L.pilot_level_actor(camera)
            S.update(actor=a,camera=camera,original=c.get_material(0),stage='variant')
        if S['stage']=='variant':
            c=S['actor'].static_mesh_component;i=S['index']
            c.set_material(0,S['original'] if i in [0,3] else c.static_mesh.get_material(0) if i==1 else u.load_asset('/Engine/EngineMaterials/WorldGridMaterial'))
            c.set_forced_lod_model(1 if i==3 else 0)
            S.update(stage='warm',at=now);return
        if S['stage']=='warm' and now-S['at']>5:
            name=['Current','OriginalTrunk','FlatTrunk','ForcedLOD0'][S['index']]
            S['shot']=u.AutomationLibrary.take_high_res_screenshot(1400,1000,str(OUT/(name+'.png')),camera=S['camera'])
            S.update(stage='shot',at=now);return
        if S['stage']=='shot' and now-S['at']>3:
            S['index']+=1
            if S['index']<4:S['stage']='variant'
            else:R['complete']=True;finish()
    except Exception:R['error']=traceback.format_exc();u.log_error(R['error']);finish()
    finally:S['busy']=False
S['handle']=u.register_slate_post_tick_callback(tick)
