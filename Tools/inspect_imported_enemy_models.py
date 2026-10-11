import unreal as u,json
from pathlib import Path
OUT=Path(u.Paths.project_dir()).resolve()/'Saved/EnemyModelReplacement'
assets=json.loads((OUT/'ImportedAssets.json').read_text());report={}
for name,paths in assets.items():
    mesh=u.load_asset(paths['mesh']);m=u.SkeletonModifier();assert m.set_skeletal_mesh(mesh)
    report[name]={'bounds':str(mesh.get_bounds()),'root':str(m.get_bone_transform('Hips',True)),
       'frames':{k:{'duration':u.load_asset(a).get_play_length(),'hips':str(u.AnimationLibrary.get_bone_pose_for_time(u.load_asset(a),'Hips',0,False))} for k,a in paths['animations'].items()},
       'abp_skeleton':str(u.load_asset(paths['anim_blueprint']).get_editor_property('target_skeleton'))}
(OUT/'UnrealRigInspection.json').write_text(json.dumps(report,indent=2));u.log('UNREAL_ENEMY_INSPECTION '+json.dumps(report))
