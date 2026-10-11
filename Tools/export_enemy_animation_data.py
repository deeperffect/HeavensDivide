import unreal as u,json
from pathlib import Path
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/EnemyModelReplacement'
donors=json.loads((OUT/'DonorRigs.json').read_text())
paths={
 'AshSeer':{'Idle':'/Game/Assets/EnemyCharacters/DevilRanged/Idle','Walk':'/Game/Assets/EnemyCharacters/DevilRanged/Standing_Walk_Forward','Cast':'/Game/Assets/EnemyCharacters/DevilRanged/AttackFireball'},
 'OgreWarden':{'Idle':'/Game/Assets/EnemyCharacters/Ogre/Animations/Breathing_Idle','Walk':'/Game/Assets/EnemyCharacters/Ogre/Animations/Walking','Cast':'/Game/Assets/EnemyCharacters/Ogre/Animations/Attack'}}
def tf(t):return {'t':[t.translation.x,t.translation.y,t.translation.z],'q':[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],'s':[t.scale3d.x,t.scale3d.y,t.scale3d.z]}
out={}
for name,anims in paths.items():
    mesh=u.load_asset(donors[name]['mesh']);bones=[b['name'] for b in donors[name]['bones']]
    out[name]={}
    for label,path in anims.items():
        anim=u.load_asset(path);duration=anim.get_play_length();count=max(2,round(duration*30))
        frames=[]
        for i in range(count+1):
            poses=u.AnimationLibrary.get_bone_poses_for_time(anim,bones,duration*i/count,False,mesh)
            frames.append([tf(t) for t in poses])
        out[name][label]={'path':path,'duration':duration,'frames':frames,'bones':bones}
    bp=u.load_asset(donors[name]['animation_class'].split('.')[0])
    task=u.AssetExportTask();task.object=bp;task.filename=str(OUT/(name+'_AnimBlueprint.copy'));task.automated=True;task.prompt=False;task.replace_identical=True
    assert u.Exporter.run_asset_export_task(task)
(OUT/'AnimationData.json').write_text(json.dumps(out,separators=(',',':')))
u.log('ENEMY_ANIMATION_EXPORT_COMPLETE')
