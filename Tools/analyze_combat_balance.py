"""Reproducible analytic budgets, not a replacement for measured gameplay DPS.
python Tools/analyze_combat_balance.py
Writes Saved/BalancePass/MathReport.json; no Unreal dependency.
"""
import json, math
from pathlib import Path
root=Path(__file__).resolve().parents[1]
data=json.loads((root/'Tools/combat_balance_v1.json').read_text())
cards={r['id']:r for r in json.loads((root/'Tools/combat_balance_reference.json').read_text())}
def value(uid,key):return cards[uid]['parameters'][key]
def rank(uid,n):
    c=cards[uid]; n=min(n,c['max_rank'])
    return n*(c['rarity'][0] if c['rarity'] else c['parameters'].get('PerRank',0))
def budgets(n=0):
    s=data['player']['Samurai']['damage']; k=data['player']['Ninja']['damage']; period=1.2
    blood=s*(1+rank('SamuraiHeavyBlade',n));blood_rate=1.3*(1+rank('SamuraiTempo',n))/period
    crescent=s*value('BladeWave','WaveDamageMultiplier')*(1+rank('CrescentDamage',n))
    iai=s*value('Iaijutsu','DamageMultiplier')*(1+rank('IaijutsuDamage',n))
    iai_period=value('Iaijutsu','ChargeDuration')/(1+rank('IaijutsuChargeSpeed',n))+value('Iaijutsu','Cooldown')
    fang=k*(1+rank('FangDamage',n));speed=1800*value('ReturningFang','FlightSpeedMultiplier')*(1+rank('FangSpeed',n))
    barrage=k*value('BarrageStance','ProjectileDamageMultiplier')*(1+rank('BarrageDamage',n));barrage_rate=(1+value('BarrageStance','AttackSpeedBonus'))*(1+rank('BarrageSpeed',n))/period
    wheel=k*1.5*(1+rank('ShurikenDamage',n));wheel_period=period/(.5*(1+rank('ShurikenSpeed',n)))
    return {
      'Blood':{'direct_dps':blood*blood_rate,'full_stack_bleed_dps':blood*value('BattleStance','BleedHitFraction')*value('BattleStance','BleedStackCap')/value('BattleStance','BleedDuration')},
      'Crescent':{'per_target_dps':crescent/period,'hit':crescent,'note':'Wave Speed improves travel/coverage, not firing cadence.'},
      'Iaijutsu':{'unmarked_dps':iai/iai_period,'marked_dps':iai*(1+value('Iaijutsu','MarkBonus'))/iai_period,'hit':iai,'period':iai_period,'note':'Mark requires another hit before its expiration; ordinary attack speed scales recovery only.'},
      'Fang':{'dps_at_180cm':fang/min_period(speed,180),'dps_at_600cm':fang/min_period(speed,600),'dps_at_850cm':fang/min_period(speed,850),'note':'Point-target round trips; steering, animation, target radius and misses change measured rate. Return deals no damage.'},
      'Barrage':{'one_projectile_dps':barrage*barrage_rate,'three_target_direct_dps':barrage*barrage_rate*3,'full_stack_poison_dps':barrage*value('BarrageStance','PoisonHitFraction')*value('BarrageStance','PoisonCap')/value('BarrageStance','PoisonDuration'),'note':'Three projectiles are coverage, not guaranteed triple boss damage.'},
      'Shuriken':{'one_contact_dps':wheel/wheel_period,'two_contact_dps':2*wheel/wheel_period,'six_contact_stationary_upper_bound_dps':6*wheel/wheel_period,'note':'Count actual contacts; lifetime is not guaranteed uptime on a moving target.'}}
def min_period(speed,distance):return max(1/value('ReturningFang','MaxLaunchesPerSecond'),2*distance/speed)
upgrade_math=[]
for c in cards.values():
    p=c['parameters'];r={'id':c['id'],'max_rank':c['max_rank'],'parameters':p,'rarity':c['rarity'],'modifiers':c['modifiers']}
    if 'PerRank' in p:r['max_rank_added_amount']=p['PerRank']*c['max_rank']
    if c['rarity']:r['all_common_total']=c['rarity'][0]*c['max_rank'];r['all_epic_total']=c['rarity'][-1]*c['max_rank']
    if 'Chance' in p and 'DamageFraction' in p:r['expected_extra_hit_fraction']=p['Chance']*p['DamageFraction']
    if 'Chance' in p and 'DamageMultiplier' in p:r['expected_critical_multiplier']=1+p['Chance']*(p['DamageMultiplier']-1)
    if 'Frequency' in p:r['base_extra_attack_rate_per_normal_attack']=p.get('ExtraBlades',1)/p['Frequency']
    upgrade_math.append(r)
enemies=[]
for p,e in data['enemies'].items():
    enemies.append({'enemy':p.rsplit('/',1)[1],'hp':e['health'],'xp':e['xp'],'samurai_base_hits':math.ceil(e['health']/8),'ninja_base_hits':math.ceil(e['health']/10),'seconds_at_30_effective_dps':e['health']/30,'seconds_at_60_effective_dps':e['health']/60,'xp_per_100_hp':100*e['xp']/e['health']})
report={'assumptions':['No mastery damage; no time-based HP growth.','Actual DPS includes misses, range, animations, overkill and avoidance.','DoT columns are capped steady-state rates, not immediate starting damage.','Proc/area totals cannot be added as guaranteed single-target DPS.','Equal-rank presets are comparison fixtures, not guaranteed level-up builds.'],
 'presets':{str(n):budgets(n) for n in [0,1,3,5]},'upgrades':upgrade_math,'enemies':enemies,
 'combo':{'Samurai_total_base_damage':5*16,'Ninja_total_base_damage':4.5*18,'note':'Pulses include time zero. Must remain in range; character/shared damage modifiers apply.'},
 'meta':{'samurai_direct_damage':1.17*1.24,'samurai_direct_cadence':1.14*1.17,'samurai_regular_direct_dps':1.17*1.24*1.14*1.17,'ninja_direct_damage':1.17*1.24,'ninja_direct_cadence':1.14*1.09,'note':'Fang is travel/cap limited; Iaijutsu charge does not scale with ordinary attack speed. Projectile, status and defensive bonuses are separate.'}}
assert not data['health_time_scaling']
assert len(cards)==len(data['reviewed_upgrade_ids']) and len(enemies)==26
assert math.isclose(budgets()['Crescent']['per_target_dps'],10)
assert math.isclose(budgets()['Iaijutsu']['unmarked_dps'],8)
assert math.isclose(budgets()['Fang']['dps_at_180cm'],20)
for n in range(1,6):
    assert budgets(n)['Blood']['direct_dps'] > budgets(n-1)['Blood']['direct_dps']
    assert budgets(n)['Iaijutsu']['marked_dps'] > budgets(n-1)['Iaijutsu']['marked_dps']
assert max(e['health'] for e in data['enemies'].values())==6000
out=root/'Saved/BalancePass/MathReport.json';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,indent=2))
print('BALANCE_MATH_PASS: %d definitions, %d enemies, 4 rank profiles; %s'%(len(cards),len(enemies),out))
