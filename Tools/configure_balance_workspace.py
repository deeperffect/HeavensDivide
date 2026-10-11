"""Run inside Unreal. Seed ONLY absent runtime keys; preserve all authored balance values."""
import unreal,json,re,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
catalog=json.loads((root/'Tools/balance_parameter_catalog.json').read_text())['values']
report={'added':{},'preserved':{},'missing_definitions':[]}
backup=root/'Saved/BalanceWorkspace/InstallBackup'
def protect(asset):
 rel=asset.get_path_name().split('.')[0].removeprefix('/Game/')+'.uasset'
 src=root/'Content'/rel;dest=backup/rel
 if src.exists() and not dest.exists():dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
path='/Game/HeavensDivide/Balance/DA_GlobalBalance'
global_asset=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if not global_asset:
 factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.BloodshiftBalanceData)
 global_asset=unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_GlobalBalance','/Game/HeavensDivide/Balance',unreal.BloodshiftBalanceData,factory)
 skills=list(global_asset.get_editor_property('skills'))
 for skill in skills:
  desc=skill.get_editor_property('description');value=skill.get_editor_property('per_rank')
  # Defaults are integral percentages; keep wording and only replace matching numerical bonuses.
  pct=str(round(value*100))+'%'
  if pct in desc:desc=desc.replace(pct,'{Percent}%')
  elif str(skill.get_editor_property('effect')) in ['Dash','Pierce','Projectile','Reroll','Banish']:
   desc=re.sub(r'\b'+str(round(value))+r'\b','{Magnitude}',desc,count=1)
   if value==1:desc=re.sub(r'\bone\b','{Magnitude}',desc,count=1)
  skill.set_editor_property('description',desc)
 global_asset.set_editor_property('skills',skills)
 assert unreal.EditorAssetLibrary.save_loaded_asset(global_asset,False)
bp=unreal.load_asset('/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController')
controller=unreal.get_default_object(bp.generated_class())
upgrades=controller.get_editor_property('player_upgrade_component')
# Native-only clones have no source Blueprint to edit. Preserve the configured class's
# defaults when exposing those values through the Shadow Step upgrade.
clone_class=controller.get_editor_property('shadow_clone_class') or unreal.load_class(None,'/Script/HeavensDivide.ShadowClone')
clone=unreal.get_default_object(clone_class)
for key,prop in [('InitialAttackDelay','initial_attack_delay'),('BaseAttackInterval','base_attack_interval'),('TargetRetryInterval','target_retry_interval'),('SafetyLifetime','safety_lifetime'),('PostAttackLingerDuration','post_attack_linger_duration'),('AttackRange','attack_range')]:
 catalog.setdefault('ShadowStep',{})[key]=float(clone.get_editor_property(prop))
seen=set()
templates={
 'SynergySwapRestoresDashCharge':'Swapping heroes restores {ChargesRestored} dash charge(s).',
 'ShadowStep':'Dashing as Ninja leaves a Shadow Clone that attacks {BaseAttacks} time(s) with your current weapon.',
 'BloodRush':'Bleeding kills grant +{MoveSpeedBonusPercent}% movement speed for {Duration}s. Kills refresh it; swapping ends it.',
 'IaijutsuDash':'Gain +{DashDistanceBonusPercent}% dash distance. Dashing charges an Iaijutsu slash along the path travelled.',
 'IaijutsuDashPact':'Marked kills restore {RechargeSecondsPerKill}s of dash recharge. Marks no longer amplify damage; Exposed Weakness becomes Iaijutsu Damage.',
 'Handoff':'After swapping, the incoming hero gains {HandoffAttackSpeedBonusPercent}% attack speed for {HandoffDuration} seconds.',
 'DoubleCut':'Every {Frequency}th attack strikes in a full circle.',
 'BloodCritical':'Attacks have a {ChancePercent}% chance to deal {DamageMultiplier}x damage.',
 'BloodEcho':'Attacks have a {ChancePercent}% chance to repeat as an echo dealing {DamageFractionPercent}% damage.',
 'BloodDetonation':'At maximum Bleed stacks, consume all stacks and detonate for {DamageMultiplier}x their remaining damage in a {Radius} cm radius.',
 'FangTwin':'Every {Frequency}th launch sends {ExtraBlades} additional spectral Fangs, seeking different enemies when possible. Spectral Fangs return once and cannot summon more Fangs.',
 'BloodAssist':'Attacks have a {ChancePercent}% chance to trigger a Ninja assist.',
 'IaijutsuPowerPact':'Deal {DamageMultiplier}x Iaijutsu damage. Charge time becomes {ChargeMultiplier}x as long.',
 'IaijutsuMarkPact':'Marks grant {MarkMultiplier}x their damage bonus. Slash width becomes {WidthMultiplierPercent}% of normal.',
 'CrescentPowerPact':'Crescent waves deal {DamageMultiplier}x damage and travel at {SpeedMultiplierPercent}% speed.',
 'CrescentFieldPact':'Lingering fields deal {FieldDamageMultiplier}x damage. Wave hit damage becomes {WaveDamageMultiplierPercent}% of normal.',
}
for asset in upgrades.get_editor_property('upgrade_pool'):
 if not asset:continue
 uid=str(asset.get_editor_property('upgrade_id'));seen.add(uid)
 old=dict(asset.get_editor_property('balance_parameters'));current={str(k):float(v) for k,v in old.items()}
 report['preserved'][uid]=dict(current)
 added={k:v for k,v in catalog.get(uid,{}).items() if k not in current}
 if added or uid in templates:
  protect(asset);current.update(added)
  asset.set_editor_property('balance_parameters',current)
  if current:unreal.SystemLibrary.execute_console_command(None,'setnopec '+asset.get_path_name()+' bHasRuntimeBalance True')
  if uid in templates:asset.set_editor_property('description',templates[uid])
  assert unreal.EditorAssetLibrary.save_loaded_asset(asset,False),uid
  if added:report['added'][uid]=added
 # A migration never silently changes an existing tuning value.
 actual={str(k):float(v) for k,v in dict(asset.get_editor_property('balance_parameters')).items()}
 assert all(abs(actual[k]-v)<0.0001 for k,v in report['preserved'][uid].items()),uid
report['missing_definitions']=sorted(set(catalog)-seen)
out=root/'Saved/BalanceWorkspace';out.mkdir(parents=True,exist_ok=True)
(out/'Installation.json').write_text(json.dumps(report,indent=2))
unreal.log('BALANCE_WORKSPACE_INSTALLED '+str(sum(map(len,report['added'].values())))+' new editable parameters; all existing numbers preserved.')
