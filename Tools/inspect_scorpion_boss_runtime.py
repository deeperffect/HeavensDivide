"""Read-only inventory of the current boss and animation graph."""
import json
from pathlib import Path
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/ScorpionBoss';OUT.mkdir(parents=True,exist_ok=True)
BP='/Game/HeavensDivide/Blueprints/EnemyCharacters/Bosses/CinderOracle/BP_CinderOracle'
bp=u.load_asset(BP);cdo=u.get_default_object(bp.generated_class())
def vec(v):return [v.x,v.y,v.z]
def path(v):return v.get_path_name() if v else None
def capture(actor):
    mesh=actor.get_component_by_class(u.SkeletalMeshComponent);cap=actor.get_component_by_class(u.CapsuleComponent)
    return dict(name=str(actor.get_editor_property('boss_display_name')),scale=vec(actor.get_actor_scale3d()),
        capsule=[cap.get_editor_property('capsule_radius'),cap.get_editor_property('capsule_half_height')],
        mesh=path(mesh.get_editor_property('skeletal_mesh_asset')),anim_class=path(mesh.get_editor_property('anim_class')),
        mesh_location=vec(mesh.get_editor_property('relative_location')),mesh_scale=vec(mesh.get_editor_property('relative_scale3d')),
        mesh_rotation=str(mesh.get_editor_property('relative_rotation')),death=path(actor.get_editor_property('death_montage')),
        cast=path(actor.get_editor_property('oracle_cast_montage')),
        balance={key:actor.get_editor_property(key) for key in ['boss_max_health','boss_move_speed','spell_damage','phase_one_interval','phase_two_interval','preferred_distance','cast_recovery_seconds']})
report={'cdo':capture(cdo)}
level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert level.load_level('/Game/Maps/Lvl_B1_Arena')
report['placed']=[capture(a) for a in actors.get_all_level_actors() if isinstance(a,u.CinderOracleBoss)]
abp=u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/ABP_EnemyDevilRanged')
task=u.AssetExportTask();task.object=abp;task.filename=str(OUT/'OriginalAnimationGraph.copy');task.automated=True;task.prompt=False
assert u.Exporter.run_asset_export_task(task)
(OUT/'RuntimeBefore.json').write_text(json.dumps(report,indent=2))
u.log('SCORPION_RUNTIME_INSPECTION '+json.dumps(report))
