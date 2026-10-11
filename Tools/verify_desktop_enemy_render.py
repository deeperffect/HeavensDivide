"""Render imported mesh poses and verify skin alignment/dissolve before activation."""
import unreal as u,json,os
from pathlib import Path
OUT=Path(u.Paths.project_dir()).resolve()/'Saved'/os.environ.get('BLOODSHIFT_ENEMY_WORKSPACE','EnemyModelReplacement')
assets=json.loads((OUT/'ImportedAssets.json').read_text());report={}
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
u.SystemLibrary.execute_console_command(None,'r.PSOPrecaching 0')
u.SystemLibrary.execute_console_command(None,'r.SkipDrawOnPSOPrecaching 0')
for name,paths in assets.items():
    # A commandlet has no gameplay streaming viewpoint. Keep just this preview's
    # base-color texture resident so captures don't use coarse fallback mips.
    texture=u.load_asset('/Game/Assets/EnemyCharacters/'+name+'/Textures/T_'+name+'_BaseColor')
    texture.set_editor_property('never_stream',True)
    mesh=u.load_asset(paths['mesh']);mod=u.SkeletonModifier();assert mod.set_skeletal_mesh(mesh)
    bones=mod.get_all_bone_names();idle=u.load_asset(paths['animations']['Idle'])
    # Idle begins in the authored bind pose. Every local transform should agree.
    errors=[]
    for bone in bones:
        actual=u.AnimationLibrary.get_bone_pose_for_time(idle,bone,0,False);ref=mod.get_bone_transform(bone,False)
        distance=actual.translation.distance(ref.translation)
        q=actual.rotation;r=ref.rotation;dot=abs(q.x*r.x+q.y*r.y+q.z*r.z+q.w*r.w)
        if distance>.1 or dot<.999:errors.append((str(bone),distance,dot))
    assert not errors,(name,'Bind animation mismatch',errors)
    actor=actors.spawn_actor_from_class(u.SkeletalMeshActor,u.Vector(),u.Rotator(yaw=180))
    comp=actor.get_component_by_class(u.SkeletalMeshComponent);comp.set_skeletal_mesh_asset(mesh)
    comp.set_forced_lod(1)
    comp.set_update_animation_in_editor(True)
    material=mesh.get_editor_property('materials')[0].material_interface
    assert not u.MaterialEditingLibrary.recompile_material(material)
    camera=actors.spawn_actor_from_class(u.SceneCapture2D,u.Vector(260,-80,70),u.Rotator())
    camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(camera.get_actor_location(),u.Vector(0,0,50)),False)
    cap=camera.get_component_by_class(u.SceneCaptureComponent2D)
    cap.set_editor_property('projection_type',u.CameraProjectionMode.ORTHOGRAPHIC);cap.set_editor_property('ortho_width',135)
    cap.set_editor_property('capture_source',u.SceneCaptureSource.SCS_BASE_COLOR)
    cap.set_editor_property('capture_every_frame',False);cap.set_editor_property('capture_on_movement',False)
    cap.set_editor_property('primitive_render_mode',u.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
    cap.show_only_actor_components(actor)
    rt=u.RenderingLibrary.create_render_target2d(actor,512,512,u.TextureRenderTargetFormat.RTF_RGBA8)
    cap.set_editor_property('texture_target',rt)
    stats={};hands={}
    for label,t in [('Idle',0),('Walk',.23),('Cast',.9)]:
        animation=u.load_asset(paths['animations'][label])
        comp.play_animation(animation,False);comp.set_position(t,False);comp.stop()
        assert u.SwapVFXSetupLibrary.refresh_preview_animation(comp)
        # Recreate the proxy between captures made in a single commandlet frame,
        # so GPU skinning does not reuse the preceding pose's frame cache.
        comp.set_cast_shadow(False);comp.set_cast_shadow(True)
        assert abs(comp.get_position()-t)<.001,(name,label,comp.get_position())
        assert 35<comp.get_socket_location('Hips').z<(65 if name=='GraveCantor' else 55),(name,'Incorrect skeletal orientation')
        cap.capture_scene()
        pixels=u.RenderingLibrary.read_render_target(actor,rt,False)
        coverage=sum(bool(p.r or p.g or p.b) for p in pixels)
        assert coverage>15000,(name,label,'Mesh not visible',coverage)
        u.RenderingLibrary.export_render_target(actor,rt,str(OUT),name+'_UE_'+label+'.png')
        hands[label]=comp.get_socket_location('LeftHand')
        stats[label]={'coverage':coverage,'hips':str(comp.get_socket_location('Hips')),'hand':str(comp.get_socket_location('LeftHand')),'position':comp.get_position()}
    assert hands['Idle'].distance(hands['Cast'])>5,(name,'Attack pose did not change')
    comp.set_anim_instance_class(u.load_asset(paths['anim_blueprint']).generated_class())
    assert u.SwapVFXSetupLibrary.refresh_preview_animation(comp)
    instance=comp.get_anim_instance();montage=u.load_asset(paths['montage'])
    assert instance.montage_play(montage)>0
    assert u.SwapVFXSetupLibrary.refresh_preview_animation(comp,.9)
    assert instance.montage_is_playing(montage)
    assert comp.get_socket_location('LeftHand').distance(hands['Idle'])>5,(name,'Animation Blueprint did not output the montage')
    stats['animation_blueprint_montage_playback']=True
    mid=comp.create_dynamic_material_instance(0);coverage=[]
    for dissolve in [0,.5,1]:
        mid.set_scalar_parameter_value('DissolveAmount',dissolve);cap.capture_scene()
        pixels=u.RenderingLibrary.read_render_target(actor,rt,False)
        coverage.append(sum(bool(p.r or p.g or p.b) for p in pixels))
    assert coverage[0]>coverage[1]>coverage[2] and coverage[2]==0,(name,'Dissolve failure',coverage)
    stats['dissolve_coverage']=coverage;report[name]=stats
    (OUT/'UnrealVisualValidation.json').write_text(json.dumps(report,indent=2))
    u.log('ENEMY_RENDER_PASS '+name+' '+json.dumps(stats))
    actors.destroy_actor(camera);actors.destroy_actor(actor)
u.log('ALL_ENEMY_RENDER_PASS')
