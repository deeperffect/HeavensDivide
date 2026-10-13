"""Apply the verified scorpion appearance to the existing boss and saved arena."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/ScorpionBoss'
BP='/Game/HeavensDivide/Blueprints/EnemyCharacters/Bosses/CinderOracle/BP_CinderOracle';MAP='/Game/Maps/Lvl_B1_Arena'
assets=json.loads((OUT/'ImportedAssets.json').read_text());visual=json.loads((OUT/'UnrealVisualValidation.json').read_text())
assert visual['montages']['Cast']['plays'] and visual['montages']['Death']['plays']
BACKUP=OUT/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S');BACKUP.mkdir(parents=True,exist_ok=True)
for relative in ['Content/HeavensDivide/Blueprints/EnemyCharacters/Bosses/CinderOracle/BP_CinderOracle.uasset','Content/Maps/Lvl_B1_Arena.umap']:
    destination=BACKUP/relative;destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/relative,destination)
level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
protected=list(json.loads((ROOT/'Tools/boss_pressure_tuning.json').read_text()))+['boss_max_health','boss_move_speed','spell_damage','boss_base_damage','forward_attack_damage','AoEDamage','dash_damage','ground_circle_damage','preferred_distance','cast_recovery_seconds','guardian_class','guardian_limit','guardians_per_cast','xp_reward']
def snapshot(a):
    return {key:(value.get_path_name() if isinstance(value,u.Object) else value) for key in protected for value in [a.get_editor_property(key)]}
mesh=u.load_asset(assets['mesh']);abp=u.load_asset(assets['anim_blueprint']);cast=u.load_asset(assets['montages']['Cast']);death=u.load_asset(assets['montages']['Death'])
def appearance(a):
    cap=a.get_component_by_class(u.CapsuleComponent);comp=a.get_component_by_class(u.SkeletalMeshComponent)
    cap.set_capsule_size(300,400,False)
    comp.set_skeletal_mesh_asset(mesh);comp.set_anim_instance_class(abp.generated_class())
    comp.set_editor_property('animation_mode',u.AnimationMode.ANIMATION_BLUEPRINT)
    comp.set_editor_property('relative_location',u.Vector(0,0,-400));comp.set_editor_property('relative_scale3d',u.Vector(1.30,1.30,1.30))
    comp.set_editor_property('relative_rotation',u.Rotator(yaw=assets['component_yaw']))
    comp.set_editor_property('override_materials',[]);comp.set_editor_property('receives_decals',False)
    comp.set_overlay_material(None)
    comp.set_editor_property('component_use_fixed_skel_bounds',False)
    a.set_editor_property('boss_display_name','VESHRA - THE PALE STINGER')
    a.set_editor_property('oracle_cast_montage',cast);a.set_editor_property('death_montage',death)
    # EditDefaultsOnly is hidden on instances, but old serialized overrides must
    # also be cleared on the placed actor before the arena is saved.
    assert u.SwapVFXSetupLibrary.set_property_text(a,'PushbackMultiplier','0.0')
assert level.load_level(MAP)
placed=[a for a in actors.get_all_level_actors() if isinstance(a,u.CinderOracleBoss)];assert len(placed)==1
before_arena=snapshot(placed[0]);old_scale=placed[0].get_actor_scale3d()
bp=u.load_asset(BP);before_cdo=snapshot(u.get_default_object(bp.generated_class()))
appearance(u.get_default_object(bp.generated_class()));u.BlueprintEditorLibrary.compile_blueprint(bp)
assert 'ERROR' not in str(bp.get_editor_property('status'));assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
cdo=u.get_default_object(bp.generated_class());assert snapshot(cdo)==before_cdo
placed=[a for a in actors.get_all_level_actors() if isinstance(a,u.CinderOracleBoss)];assert len(placed)==1
boss=placed[0];boss.modify(True);appearance(boss);boss.set_actor_label('Veshra - The Pale Stinger')
assert snapshot(boss)==before_arena
floor=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='Plane')
assert floor.get_component_by_class(u.StaticMeshComponent).get_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY_GROUND)==u.CollisionResponseType.ECR_BLOCK
location=boss.get_actor_location();location.z=floor.get_actor_location().z+boss.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_half_height()
boss.set_actor_location(location,False,False)
assert boss.get_actor_scale3d()==old_scale;assert level.save_current_level()
assert level.load_level('/Game/Maps/Lvl_B1_Lvl1');assert level.load_level(MAP)
boss=next(a for a in actors.get_all_level_actors() if isinstance(a,u.CinderOracleBoss));component=boss.get_component_by_class(u.SkeletalMeshComponent)
assert component.get_editor_property('skeletal_mesh_asset')==mesh
assert component.get_editor_property('anim_class')==abp.generated_class()
assert boss.get_editor_property('oracle_cast_montage')==cast and boss.get_editor_property('death_montage')==death
assert str(boss.get_editor_property('boss_display_name'))=='VESHRA - THE PALE STINGER'
assert component.get_overlay_material() is None
assert boss.get_editor_property('pushback_multiplier')==0
assert component.get_editor_property('relative_scale3d').distance(u.Vector(1.30,1.30,1.30))<.001
assert abs(boss.get_component_by_class(u.CapsuleComponent).get_unscaled_capsule_radius()-300)<.001
assert abs(boss.get_component_by_class(u.CapsuleComponent).get_unscaled_capsule_half_height()-400)<.001
assert snapshot(boss)==before_arena
floor=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='Plane')
foot_z=boss.get_actor_location().z-boss.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_half_height()
assert abs(foot_z-floor.get_actor_location().z)<.01
report=dict(backup=str(BACKUP),blueprint=BP,arena=MAP,name=str(boss.get_editor_property('boss_display_name')),mesh=mesh.get_path_name(),
    unchanged_combat=before_arena,mesh_location=str(component.get_editor_property('relative_location')),component_yaw=assets['component_yaw'],
    actor_location=str(boss.get_actor_location()),actor_scale=str(old_scale),ground_z=floor.get_actor_location().z,reload_verified=True,
    mesh_scale=str(component.get_editor_property('relative_scale3d')),overlay_removed=True,pushback_multiplier=0,size_multiplier_since_initial_rig=2,
    arena_sha256=hashlib.sha256((ROOT/'Content/Maps/Lvl_B1_Arena.umap').read_bytes()).hexdigest())
(OUT/'ActivationReport.json').write_text(json.dumps(report,indent=2));u.log('SCORPION_BOSS_ACTIVATED '+json.dumps(report))
