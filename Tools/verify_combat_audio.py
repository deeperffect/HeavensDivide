"""Read-only fresh-process verification of saved combat audio routing and graph pins."""
import unreal as u,json
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();base='/Game/HeavensDivide/Audio';rows=json.loads((root/'Tools/combat_audio_manifest.json').read_text())
ss=u.get_engine_subsystem(u.SubobjectDataSubsystem);report={'metasounds':[],'bindings':[],'upgrades':[]}
def comp(bp,cls):
 for h in ss.k2_gather_subobject_data_for_blueprint(bp):
  o=u.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(u.SubobjectDataBlueprintFunctionLibrary.get_data(h),bp)
  if isinstance(o,cls):return o
 raise AssertionError((bp,cls))
def sound(obj,field,event=None):
 a=obj.get_editor_property(field);assert isinstance(a,u.MetaSoundSource),(obj,field,a)
 assert a.get_path_name().startswith(base+'/MetaSounds/'),(obj,field,a)
 if event:assert a.get_name()=='MS_'+event,(field,a,event)
 report['bindings'].append({'object':obj.get_path_name() if isinstance(obj,u.Object) else str(type(obj)),'field':field,'sound':a.get_name()})
palette=u.load_asset(base+'/DA_CombatAudio');mapping=palette.get_editor_property('sounds');assert len(mapping)==len(rows)
for row in rows:
 a=u.load_asset(base+'/MetaSounds/MS_'+row['event']);assert isinstance(a,u.MetaSoundSource)
 assert len(a.get_editor_property('concurrency_set'))>=1
 assert abs(a.get_editor_property('volume')-row['volume'])<.001
 w=u.load_asset(base+'/Waves/W_'+row['event']);assert w and w.get_editor_property('num_channels')==1
 assert abs(w.get_editor_property('duration')-row['duration'])<.001
 report['metasounds'].append(a.get_path_name())
for hero in ['Samurai','Ninja']:
 bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_'+hero);c=u.get_default_object(bp.generated_class());p=c.get_editor_property('combo_ability')
 sound(p,'sound',hero+'Ability');sound(p,'pulse_sound',hero+'Pulse')
 if hero=='Samurai':sound(comp(bp,u.AutoAttackComponent),'impact_sound','SamuraiImpact')
 else:
  for field,event in [('kunai_throw_sound','KunaiThrow'),('shuriken_throw_sound','ShurikenThrow'),('shuriken_hit_sound','ShurikenImpact')]:sound(comp(bp,u.NinjaBuildComponent),field,event)
 for field,event in [('arrival_portal_sound','SwapArrival'),('departure_portal_sound','SwapDeparture')]:sound(comp(bp,u.SwapPresentationComponent),field,event)
for path in u.EditorAssetLibrary.list_assets('/Game/HeavensDivide/Blueprints/EnemyCharacters',True,False):
 bp=u.load_asset(path)
 if not isinstance(bp,u.Blueprint):continue
 c=u.get_default_object(bp.generated_class())
 if isinstance(c,u.EnemyBase) and bp.get_name()!='BP_TestDummy':sound(comp(bp,u.EnemyDeathComponent),'death_sound')
for path in u.EditorAssetLibrary.list_assets('/Game/HeavensDivide/Upgrades',True,False):
 a=u.load_asset(path)
 if isinstance(a,u.UpgradeDefinition):
  if a.get_editor_property('has_runtime_presentation'):sound(a.get_editor_property('presentation'),'sound')
  report['upgrades'].append(str(a.get_editor_property('upgrade_id')))
for name,event in [('BP_NinjaProjectile','KunaiImpact'),('BP_SamuraiWaveProjectile','BladeWaveHit')]:
 bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/'+name);sound(u.get_default_object(bp.generated_class()).get_editor_property('impact_feedback'),'hit_sound',event)
for hero,event in [('Ninja','KunaiThrow'),('Samurai','SamuraiSwing')]:
 a=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/'+hero+'/AM_AutoAttack'+hero)
 notifies=[e.get_editor_property('notify') for e in u.AnimationLibrary.get_animation_notify_events(a)]
 ns=[n for n in notifies if isinstance(n,u.AnimNotify_PlaySound)]
 assert ns,(hero,'Missing attack sound notify')
 assert any(n.get_editor_property('sound').get_name()=='MS_'+event for n in ns),(hero,[str(n.get_editor_property('sound')) for n in ns])
(root/'Saved/CombatAudio/verification.json').write_text(json.dumps(report,indent=2));u.log('COMBAT_AUDIO_VERIFIED '+str(len(report['bindings']))+' bindings, '+str(len(rows))+' sounds, '+str(len(report['upgrades']))+' cards')
