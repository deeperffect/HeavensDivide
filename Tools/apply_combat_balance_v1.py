"""Apply the fixed-HP balance pass; back up every existing package before editing.
Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
The manifest owns only the properties edited here. Existing art, graphs and unlocks are preserved.
"""
import unreal as u, json, shutil
from pathlib import Path
from datetime import datetime, timezone
root=Path(u.Paths.project_dir()).resolve()
tuning=json.loads((root/'Tools/combat_balance_v1.json').read_text())
backup_root=root/'Saved/BalancePass/AssetBackups'/datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S')
report={'assets':[], 'levels':[], 'lab':None}
reg=u.AssetRegistryHelpers.get_asset_registry(); reg.search_all_assets(True)
def backup(p,ext='.uasset'):
    src=root/'Content'/(p.split('.')[0].removeprefix('/Game/')+ext)
    if src.exists():
        dst=backup_root/src.relative_to(root/'Content'); dst.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(src,dst)
def save(asset):
    assert u.EditorAssetLibrary.save_loaded_asset(asset,False),asset.get_path_name()
    report['assets'].append(asset.get_path_name())
def load_bp(p):
    backup(p); a=u.load_asset(p); assert isinstance(a,u.Blueprint),p
    u.BlueprintEditorLibrary.compile_blueprint(a)
    return a,u.get_default_object(a.generated_class())
def setp(o,**kwargs):
    for k,v in kwargs.items(): o.set_editor_property(k,v)
for name,values in tuning['player'].items():
    bp,cdo=load_bp('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_'+name)
    attack=cdo.get_component_by_class(u.AutoAttackComponent)
    setp(attack,attack_damage=values['damage'],attack_interval=values['interval'])
    if 'secondary_damage' in values: attack.set_editor_property('secondary_target_damage_multiplier',values['secondary_damage'])
    save(bp)
bp,pc=load_bp('/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController')
setp(pc,swap_cooldown=3.)
setp(pc.get_editor_property('player_health_component'),damage_grace_period=.25)
setp(pc.get_editor_property('experience_component'),xp_requirement_scale=1.)
# Legacy mastery power is intentionally inaccessible; native accessors always return 1.0.
save(bp)
pool={str(c.get_editor_property('upgrade_id')):c for c in pc.get_editor_property('player_upgrade_component').get_editor_property('upgrade_pool') if c}
assert set(tuning['reviewed_upgrade_ids'])==set(pool), 'Pool changed; review new cards before applying'
for uid,change in tuning['upgrades'].items():
    card=pool[uid]; backup(card.get_path_name())
    if 'parameters' in change:
        params={str(k):v for k,v in card.get_editor_property('balance_parameters').items()}
        params.update(change['parameters']);card.set_editor_property('balance_parameters',params)
    if 'rarity' in change:
        variants=list(card.get_editor_property('rarity_magnitudes')); assert len(variants)==3,uid
        for variant,value in zip(variants,change['rarity']): setp(variant,magnitude=value,description_override='')
        card.set_editor_property('rarity_magnitudes',variants)
        fmt=change.get('description_format','')
        if fmt:
            card.set_editor_property('rolled_description_format',fmt)
            card.set_editor_property('description',fmt.replace('{Percent}',str(round(change['rarity'][0]*100))).replace('{Magnitude}',str(change['rarity'][0])))
    if 'modifier_values' in change:
        mods=list(card.get_editor_property('stat_modifiers')); assert len(mods)==len(change['modifier_values'])
        for mod,value in zip(mods,change['modifier_values']): mod.set_editor_property('value_per_level',value)
        card.set_editor_property('stat_modifiers',mods)
    if 'description' in change: card.set_editor_property('description',change['description'])
    save(card)
for p,values in tuning['enemies'].items():
    bp,cdo=load_bp(p)
    cdo.get_health_component().set_editor_property('max_health',values['health'])
    cdo.set_editor_property('xp_reward',values['xp'])
    if isinstance(cdo,(u.MeleeEnemyBase,u.RangedEnemyBase)): cdo.set_editor_property('attack_damage',values['damage'])
    if isinstance(cdo,u.TacticalEnemy): cdo.set_editor_property('ability_damage',values['damage'])
    if isinstance(cdo,u.TankMeleeEnemyBase):
        setp(cdo,slam_damage_multiplier=1.,contact_damage=10. if 'Gorilla' in p else 8.)
    if isinstance(cdo,u.FinalBossBase):
        setp(cdo,boss_max_health=values['health'],boss_base_damage=20.,forward_attack_damage=20.,ao_e_damage=25.,dash_damage=28.,ground_circle_damage=16.)
    if isinstance(cdo,u.CinderOracleBoss): cdo.set_editor_property('spell_damage',values['damage'])
    if p.endswith('BP_GraveCantor'): setp(cdo,heal_fraction=.02,support_targets=4)
    save(bp)
def tune_spawner(o):
    entries=list(o.get_editor_property('enemy_spawn_entries'))
    # Serialized legacy HP growth is hidden and ignored by native spawning.
    setp(o,enemy_spawn_entries=entries,timed_threat_spawn_slot_chance=tuning['run']['timed_threat_chance'],max_timed_threats_alive=tuning['run']['max_timed_threats'])
bp,spawner=load_bp('/Game/HeavensDivide/Blueprints/EnemyCharacters/BP_EnemySpawner');tune_spawner(spawner);save(bp)
for p in u.EditorAssetLibrary.list_assets('/Game/HeavensDivide/Blueprints/Objectives',True,False):
    asset=u.load_asset(p)
    if not isinstance(asset,u.Blueprint):continue
    cdo=u.get_default_object(asset.generated_class())
    fields={}
    if isinstance(cdo,u.BloodShrine): fields={'bloodbound_health_multiplier':tuning['run']['bloodbound_hp'],'bloodbound_damage_multiplier':tuning['run']['bloodbound_damage']}
    if isinstance(cdo,u.BossToriiGate): fields={'unlock_run_time_seconds':tuning['run']['gate_seconds']}
    if isinstance(cdo,u.RunObjectiveDirector): fields={'twin_soul_trial_spawn_time':tuning['run']['twin_soul_seconds']}
    if isinstance(cdo,u.TwinSoulTrial): fields={'crimson_max_health':360.,'violet_max_health':360.}
    if fields: backup(p);setp(cdo,**fields);save(asset)
level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for p in ['/Game/Maps/Lvl_B1_Lvl1','/Game/Maps/Lvl_B1_Arena']:
    backup(p,'.umap'); assert level.load_level(p)
    for a in actors.get_all_level_actors():
        if isinstance(a,u.EnemySpawner):tune_spawner(a)
        if isinstance(a,u.RunObjectiveDirector):setp(a,twin_soul_trial_spawn_time=tuning['run']['twin_soul_seconds'])
        if isinstance(a,u.BossToriiGate):setp(a,unlock_run_time_seconds=tuning['run']['gate_seconds'])
        if isinstance(a,u.FinalBossBase):setp(a,boss_max_health=6000.)
        if isinstance(a,u.BloodShrine):setp(a,bloodbound_health_multiplier=tuning['run']['bloodbound_hp'],bloodbound_damage_multiplier=tuning['run']['bloodbound_damage'])
        if isinstance(a,u.TwinSoulTrial):setp(a,crimson_max_health=360.,violet_max_health=360.)
    assert level.save_current_level();report['levels'].append(p)
lab='/Game/Maps/Lvl_BalanceLab'
if not u.EditorAssetLibrary.does_asset_exist(lab):
    assert level.new_level(lab)
    cube=u.load_asset('/Engine/BasicShapes/Cube.Cube')
    def box(label,loc,scale):
        a=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(*loc));a.set_actor_label(label)
        m=a.get_component_by_class(u.StaticMeshComponent);m.set_static_mesh(cube);m.set_collision_profile_name('BlockAll')
        a.set_actor_scale3d(u.Vector(*scale));return a
    box('Balance Lab - 60m floor',(0,0,-25),(60,60,.5))
    for x,y,sx,sy in [(3000,0,.3,60),(-3000,0,.3,60),(0,3000,60,.3),(0,-3000,60,.3)]:box('Arena edge',(x,y,150),(sx,sy,3))
    start=actors.spawn_actor_from_class(u.PlayerStart,u.Vector(0,0,100));start.set_actor_label('Balance Lab Player Start')
    sun=actors.spawn_actor_from_class(u.DirectionalLight,u.Vector(0,0,1000),u.Rotator(-55,-35,0));sun.set_actor_label('Lab Key Light')
    sun.get_component_by_class(u.DirectionalLightComponent).set_editor_property('intensity',4.)
    sky=actors.spawn_actor_from_class(u.SkyLight,u.Vector(0,0,500));sky.get_component_by_class(u.SkyLightComponent).set_editor_property('intensity',1.)
    director=actors.spawn_actor_from_class(u.BalanceLabDirector,u.Vector(0,0,50));director.set_actor_label('Balance Lab - Runtime Controls and DPS Meter')
    for text,pos,size in [('BALANCE LAB',(1000,0,2),110),('6 STANCES / FIXED HP / FRESH OR FULL STATS',(-1200,0,2),45),('Use console: BalanceLab help',(-1600,0,2),50)]:
        a=actors.spawn_actor_from_class(u.TextRenderActor,u.Vector(*pos),u.Rotator(90,0,0));a.set_actor_label(text)
        t=a.get_component_by_class(u.TextRenderComponent);t.set_text(text);t.set_world_size(size);t.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
        t.set_text_render_color(u.Color(25,40,65,255))
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode',u.load_asset('/Game/HeavensDivide/Blueprints/BP_SurvivorGameMode').generated_class())
    assert level.save_current_level();report['lab']='created'
else: report['lab']='existing map preserved'
(root/'Saved/BalancePass/Applied.json').write_text(json.dumps(report,indent=2))
u.log('COMBAT_BALANCE_V1_APPLIED')
