"""Read saved enemy/encounter settings without modifying any game assets."""
import json
from pathlib import Path
import unreal as u

out=Path(u.Paths.project_saved_dir())/'EnemyRosterAudit';out.mkdir(parents=True,exist_ok=True)
def value(v):
    if v is None or isinstance(v,(str,int,float,bool)):return v
    if isinstance(v,u.Object):return v.get_path_name()
    return str(v)
def props(obj,names):
    return {n:value(obj.get_editor_property(n)) for n in names.split()}
def enemy(obj):
    r=props(obj,'move_speed drop_category xp_reward')
    r['health']=obj.get_health_component().get_editor_property('max_health')
    mesh=obj.get_component_by_class(u.SkeletalMeshComponent)
    r['mesh']=value(mesh.get_editor_property('skeletal_mesh_asset'));r['animation']=value(mesh.get_editor_property('anim_class'))
    r['root_scale']=value(obj.get_component_by_class(u.CapsuleComponent).get_editor_property('relative_scale3d'))
    if isinstance(obj,u.TacticalEnemy):
        r.update(props(obj,'tactical_role enemy_name ability_damage ability_cooldown cast_range windup cast_montage role_color minimum_windup cast_recovery_seconds retreat_distance charge_speed charge_distance charge_width charge_recovery_seconds followup_cooldown support_targets support_radius heal_fraction haste_duration haste_multiplier seer_radius mire_radius mire_duration frost_count frost_spacing frost_radius frost_stagger sniper_length sniper_width sniper_active_time warden_outer_radius warden_inner_radius storm_length storm_width storm_cross_angle burst_active_time'))
    if isinstance(obj,u.CinderOracleBoss):
        r.update(props(obj,'boss_max_health boss_move_speed spell_damage guardian_class oracle_cast_montage initial_cast_delay enrage_health_fraction phase_one_interval phase_two_interval preferred_distance cast_recovery_seconds phase_one_warning phase_two_warning ember_count ember_spacing ember_radius ember_stagger lance_side_angle lance_length lance_width nova_radius nova_safe_radius nova_warning ground_count ground_spread_radius ground_radius ground_stagger ground_duration guardians_per_cast guardian_limit spiral_count spiral_start_radius spiral_radius_step spiral_angle_step spiral_blast_radius spiral_stagger eclipse_outer_radius eclipse_inner_radius eclipse_warning eclipse_lane_length eclipse_lane_width'))
    return r
def spawner(obj):
    r=props(obj,'absolute_hard_alive_cap timed_threat_spawn_slot_chance')
    r['entries']=[props(e,'enemy_class minimum_run_time maximum_run_time enabled health_scaling_per_minute pressure_spawn_mode min_respawn_delay_after_death max_respawn_delay_after_death') for e in obj.get_editor_property('enemy_spawn_entries')]
    r['phases']=[]
    for p in obj.get_editor_property('pressure_phases'):
        row=props(p,'start_time_seconds end_time_seconds global_max_alive')
        row['population']=[props(e,'enemy_class desired_population max_population') for e in p.get_editor_property('enemy_population_entries')]
        r['phases'].append(row)
    return r
u.AssetRegistryHelpers.get_asset_registry().search_all_assets(True)
report={'enemies':{},'levels':{}}
for name in ['AshSeer','HexSniper','MireWeaver','HornLancer','FangStalker','GraveCantor','WarDrummer','OgreWarden','StormGorilla','FrostOracle','AshenMarcher']:
    bp=u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Tactical/BP_'+name)
    report['enemies'][name]=enemy(u.get_default_object(bp.generated_class()))
report['enemies']['CinderOracle']=enemy(u.get_default_object(u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Bosses/CinderOracle/BP_CinderOracle').generated_class()))
report['spawner_defaults']=spawner(u.get_default_object(u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/BP_EnemySpawner').generated_class()))
level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for path in ['/Game/Maps/Lvl_B1_Lvl1','/Game/Maps/Lvl_B1_Arena']:
    assert level.load_level(path)
    rows=[]
    for a in actors.get_all_level_actors():
        if isinstance(a,u.EnemySpawner): rows.append({'kind':'spawner','label':a.get_actor_label(),'settings':spawner(a)})
        elif isinstance(a,u.EncounterDirector):rows.append({'kind':'director','settings':props(a,'marching_enemy_class first_march_seconds march_interval_min march_interval_max max_march_enemies')})
        elif isinstance(a,u.CinderOracleBoss):rows.append({'kind':'boss','settings':enemy(a)})
    report['levels'][path]=rows
(out/'Current.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
u.log('ENEMY_ROSTER_AUDIT_COMPLETE')
