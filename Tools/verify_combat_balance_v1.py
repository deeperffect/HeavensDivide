"""Read-only saved-package verification for the fixed-HP pass."""
import unreal as u,json,math
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve()
t=json.loads((root/'Tools/combat_balance_v1.json').read_text())
checked=[]
def eq(label,got,want):
    assert math.isclose(float(got),float(want),rel_tol=1e-5,abs_tol=1e-5),(label,got,want)
    checked.append(label)
def cdo(p):return u.get_default_object(u.load_asset(p).generated_class())
pc=cdo('/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController')
eq('player hit grace',pc.get_editor_property('player_health_component').get_editor_property('damage_grace_period'),.25)
eq('swap cooldown',pc.get_editor_property('swap_cooldown'),3)
eq('working XP scale',pc.get_editor_property('experience_component').get_editor_property('xp_requirement_scale'),1)
up=pc.get_editor_property('player_upgrade_component')
# Native automation verifies legacy serialized mastery coefficients have no effect.
pool={str(c.get_editor_property('upgrade_id')):c for c in up.get_editor_property('upgrade_pool') if c}
assert set(pool)==set(t['reviewed_upgrade_ids'])
for name,v in t['player'].items():
    obj=cdo('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_'+name);a=obj.get_component_by_class(u.AutoAttackComponent)
    eq(name+' damage',a.get_editor_property('attack_damage'),v['damage'])
    eq(name+' interval',a.get_editor_property('attack_interval'),v['interval'])
    if 'secondary_damage' in v:eq(name+' secondary hit',a.get_editor_property('secondary_target_damage_multiplier'),v['secondary_damage'])
for uid,v in t['upgrades'].items():
    card=pool[uid];params={str(k):float(x) for k,x in card.get_editor_property('balance_parameters').items()}
    for key,want in v.get('parameters',{}).items():eq(uid+'.'+key,params[key],want)
    for i,want in enumerate(v.get('rarity',[])):eq(uid+'.rarity'+str(i),card.get_editor_property('rarity_magnitudes')[i].get_editor_property('magnitude'),want)
    for i,want in enumerate(v.get('modifier_values',[])):eq(uid+'.modifier'+str(i),card.get_editor_property('stat_modifiers')[i].get_editor_property('value_per_level'),want)
    if 'description' in v:
        assert str(card.get_editor_property('description'))==v['description'],uid
        assert '{' not in str(card.get_balance_description()),(uid,str(card.get_balance_description()))
for p,v in t['enemies'].items():
    e=cdo(p);eq(p+' HP',e.get_health_component().get_editor_property('max_health'),v['health'])
    eq(p+' XP',e.get_editor_property('xp_reward'),v['xp'])
    if isinstance(e,u.FinalBossBase):eq(p+' effective boss HP',e.get_editor_property('boss_max_health'),v['health'])
    if isinstance(e,u.TacticalEnemy):eq(p+' damage',e.get_editor_property('ability_damage'),v['damage'])
    elif isinstance(e,(u.MeleeEnemyBase,u.RangedEnemyBase)):eq(p+' damage',e.get_editor_property('attack_damage'),v['damage'])
    if isinstance(e,u.FinalBossBase):
        for key,want in {'boss_base_damage':20,'forward_attack_damage':20,'ao_e_damage':25,'dash_damage':28,'ground_circle_damage':16}.items():eq(p+' '+key,e.get_editor_property(key),want)
    if isinstance(e,u.CinderOracleBoss):eq(p+' spell damage',e.get_editor_property('spell_damage'),v['damage'])
    if isinstance(e,u.TankMeleeEnemyBase):
        eq(p+' slam multiplier',e.get_editor_property('slam_damage_multiplier'),1)
        eq(p+' contact',e.get_editor_property('contact_damage'),10 if 'Gorilla' in p else 8)
for p in u.EditorAssetLibrary.list_assets('/Game/HeavensDivide/Blueprints/Objectives',True,False):
    asset=u.load_asset(p)
    if not isinstance(asset,u.Blueprint):continue
    o=u.get_default_object(asset.generated_class())
    if isinstance(o,u.TwinSoulTrial):
        eq(p+' Crimson HP',o.get_editor_property('crimson_max_health'),360)
        eq(p+' Violet HP',o.get_editor_property('violet_max_health'),360)
    if isinstance(o,u.BloodShrine):
        eq(p+' challenge HP',o.get_editor_property('bloodbound_health_multiplier'),1.5)
        eq(p+' challenge damage',o.get_editor_property('bloodbound_damage_multiplier'),1.2)
def spawner(o):
    eq('aggregate threat cap',o.get_editor_property('max_timed_threats_alive'),6)
    eq('threat probability',o.get_editor_property('timed_threat_spawn_slot_chance'),.18)
    # Runtime tests cover fixed HP even when old entries retain nonzero growth.
spawner(cdo('/Game/HeavensDivide/Blueprints/EnemyCharacters/BP_EnemySpawner'))
level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for p in ['/Game/Maps/Lvl_B1_Lvl1','/Game/Maps/Lvl_B1_Arena','/Game/Maps/Lvl_BalanceLab']:
    assert level.load_level(p),p
    all_actors=actors.get_all_level_actors()
    for a in all_actors:
        if isinstance(a,u.EnemySpawner):spawner(a)
        if isinstance(a,u.RunObjectiveDirector):eq('placed Twin Soul timing',a.get_editor_property('twin_soul_trial_spawn_time'),360)
        if isinstance(a,u.BossToriiGate):eq('placed gate timing',a.get_editor_property('unlock_run_time_seconds'),t['run']['gate_seconds'])
        if isinstance(a,u.FinalBossBase):eq('placed boss HP',a.get_editor_property('boss_max_health'),6000)
    if p.endswith('BalanceLab'):
        assert len([a for a in all_actors if isinstance(a,u.BalanceLabDirector)])==1
        assert len([a for a in all_actors if isinstance(a,u.PlayerStart)])==1
        assert len([a for a in all_actors if isinstance(a,u.StaticMeshActor)])>=5
        assert not any(isinstance(a,(u.EnemySpawner,u.RunObjectiveDirector,u.EncounterDirector)) for a in all_actors)
        checked.append('isolated lab geometry, start and director')
out=root/'Saved/BalancePass/Verification.json'
out.write_text(json.dumps({'passed':True,'checks':checked,'count':len(checked)},indent=2))
u.log('COMBAT_BALANCE_VERIFY_PASS '+str(len(checked)))
