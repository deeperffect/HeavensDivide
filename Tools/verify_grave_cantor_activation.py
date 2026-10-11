"""Cold-load the live enemy Blueprint and verify its new art and retained tuning."""
import unreal as u,json
from pathlib import Path
out=Path(u.Paths.project_saved_dir()).resolve()/'GraveCantorReplacement'
original=json.loads((out/'OriginalGameplay.json').read_text())
paths=json.loads((out/'ImportedAssets.json').read_text())['GraveCantor']
bp=u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Tactical/BP_GraveCantor')
cdo=u.get_default_object(bp.generated_class());mesh=cdo.get_editor_property('mesh')
for name in ['move_speed','ability_damage','ability_cooldown','cast_range','windup','xp_reward','tactical_role',
             'support_radius','support_targets','heal_fraction','status_stack_font_size']:
    assert str(cdo.get_editor_property(name))==original[name],name
assert mesh.get_editor_property('skeletal_mesh_asset').get_path_name()==paths['mesh']
abp=u.load_asset(paths['anim_blueprint'])
assert mesh.get_editor_property('anim_class')==abp.generated_class()
assert cdo.get_editor_property('cast_montage').get_path_name()==paths['montage']
assert mesh.get_editor_property('animation_mode')==u.AnimationMode.ANIMATION_BLUEPRINT
assert not mesh.get_editor_property('override_materials')
assert cdo.get_editor_property('health_bar_draw_size')==u.Vector2D(120,18)
assert cdo.get_editor_property('status_indicator_draw_size')==u.Vector2D(42,42)
skeletal=mesh.get_editor_property('skeletal_mesh_asset')
assert u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem).get_lod_count(skeletal)==3
assert len(skeletal.get_editor_property('materials'))==1
result={'live_enemy':bp.get_path_name(),'new_model':paths['mesh'],'animation_blueprint':paths['anim_blueprint'],
        'cast_montage':paths['montage'],'combat_and_healing_preserved':True,'updated_status_ui_preserved':True,
        'lods':3,'material_sections':1}
(out/'ActivationVerified.json').write_text(json.dumps(result,indent=2))
u.log('GRAVE_CANTOR_ACTIVATION_VERIFIED '+json.dumps(result))
