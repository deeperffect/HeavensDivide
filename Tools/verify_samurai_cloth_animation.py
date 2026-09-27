from pathlib import Path
import json,hashlib
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
base='/Game/Assets/PlayerCharacters/Samurai/'
new=u.load_asset(base+'fdsafdsa')
old=u.load_asset('/Game/__SamuraiRepairReference/SamuraiCharacterV4')
assert new.get_editor_property('skeleton')==old.get_editor_property('skeleton')
assert len(new.get_editor_property('materials'))==2
actors=[];components=[]
for mesh in [old,new]:
    actor=u.EditorLevelLibrary.spawn_actor_from_class(u.SkeletalMeshActor,u.Vector(0,0,5000))
    actors.append(actor)
    component=actor.get_component_by_class(u.SkeletalMeshComponent)
    component.set_skinned_asset_and_update(mesh)
    components.append(component)
report={}
bones=['Hips','Head','LeftHand','RightHand','LeftFoot','RightFoot']
for name in ['AS_Idle_Combat_Seq','AS_Run_Combat_Loop_F_0_Seq','AS_Parry_Counter_Attack_Seq']:
    anim=u.load_asset(base+'RetargetedAnimations/'+name)
    assert anim.get_editor_property('skeleton')==new.get_editor_property('skeleton')
    poses=[];errors=[]
    for time in [.05,.25,.45]:
        for component in components:
            component.set_animation_mode(u.AnimationMode.ANIMATION_BLUEPRINT)
            component.override_animation_data(anim,True,False,time,1.)
        sample=[]
        for bone in bones:
            a,b=[c.get_socket_transform(bone,u.RelativeTransformSpace.RTS_COMPONENT).translation for c in components]
            error=((a.x-b.x)**2+(a.y-b.y)**2+(a.z-b.z)**2)**.5
            errors.append(error)
            assert error<.05,(name,time,bone,error)
            sample.append([b.x,b.y,b.z])
        poses.append(sample)
    assert poses[0]!=poses[-1],name+' did not animate'
    report[name]={'max_bone_position_error_cm':max(errors),'animated':True}
for actor in actors: u.EditorLevelLibrary.destroy_actor(actor)
manifest=json.loads((root/'Saved/Backups/SamuraiClothRepair20260928/manifest.json').read_text())
for path,value in manifest['protected'].items():
    assert hashlib.sha256((root/path).read_bytes()).hexdigest()==value,path
(root/'Saved/SamuraiClothRepair/animation_verification.json').write_text(json.dumps(report,indent=2))
u.log('SAMURAI_CLOTH_ANIMATION_PASS '+json.dumps(report))
