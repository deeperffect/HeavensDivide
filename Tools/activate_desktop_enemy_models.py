"""Replace only the three tactical enemy visual assets; preserve combat tuning."""
import unreal as u,json,os
from pathlib import Path
OUT=Path(u.Paths.project_dir()).resolve()/'Saved'/os.environ.get('BLOODSHIFT_ENEMY_WORKSPACE','EnemyModelReplacement')
assets=json.loads((OUT/'ImportedAssets.json').read_text())
validation=json.loads((OUT/'UnrealVisualValidation.json').read_text())
assert set(validation)==set(assets)
assert all(v.get('animation_blueprint_montage_playback') for v in validation.values())
report={}
def capsule_values(capsule):
    scale=capsule.get_editor_property('relative_scale3d')
    return {'relative_scale3d':[scale.x,scale.y,scale.z],
        'capsule_half_height':capsule.get_editor_property('capsule_half_height'),
        'capsule_radius':capsule.get_editor_property('capsule_radius')}
for name,paths in assets.items():
    path='/Game/HeavensDivide/Blueprints/EnemyCharacters/Tactical/BP_'+name
    backup=OUT/'Backup/HeavensDivide/Blueprints/EnemyCharacters/Tactical'/('BP_'+name+'.uasset')
    assert backup.is_file(),backup
    bp=u.load_asset(path);cdo=u.get_default_object(bp.generated_class());mesh=cdo.get_editor_property('mesh')
    capsule=cdo.get_component_by_class(u.CapsuleComponent)
    properties=['move_speed','ability_damage','ability_cooldown','cast_range','windup','xp_reward','tactical_role']
    before={k:str(cdo.get_editor_property(k)) for k in properties}
    capbefore=capsule_values(capsule)
    skeletal=u.load_asset(paths['mesh']);abp=u.load_asset(paths['anim_blueprint']);montage=u.load_asset(paths['montage'])
    mesh.set_editor_property('skeletal_mesh_asset',skeletal)
    mesh.set_editor_property('anim_class',abp.generated_class())
    mesh.set_editor_property('animation_mode',u.AnimationMode.ANIMATION_BLUEPRINT)
    mesh.set_editor_property('override_materials',[])
    mesh.set_editor_property('relative_rotation',u.Rotator(yaw=180))
    location=mesh.get_editor_property('relative_location')
    mesh.set_editor_property('relative_location',u.Vector(location.x,location.y,-capsule.get_editor_property('capsule_half_height')))
    cdo.set_editor_property('cast_montage',montage)
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert 'ERROR' not in str(bp.get_editor_property('status'))
    assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
    cdo=u.get_default_object(bp.generated_class());mesh=cdo.get_editor_property('mesh');capsule=cdo.get_component_by_class(u.CapsuleComponent)
    assert before=={k:str(cdo.get_editor_property(k)) for k in properties}
    assert capbefore==capsule_values(capsule)
    assert mesh.get_editor_property('skeletal_mesh_asset')==skeletal
    assert mesh.get_editor_property('anim_class')==abp.generated_class()
    assert cdo.get_editor_property('cast_montage')==montage
    report[name]={'blueprint':path,'mesh':skeletal.get_path_name(),'animation_class':abp.generated_class().get_path_name(),
        'cast_montage':montage.get_path_name(),'mesh_rotation':str(mesh.get_editor_property('relative_rotation')),
        'mesh_location':str(mesh.get_editor_property('relative_location')),'mesh_scale':str(mesh.get_editor_property('relative_scale3d')),
        'unchanged_combat':before,'unchanged_capsule':capbefore}
    (OUT/'ActivatedModels.json').write_text(json.dumps(report,indent=2))
    u.log('ENEMY_MODEL_ACTIVATED '+name)
u.log('ALL_ENEMY_MODELS_ACTIVATED')
