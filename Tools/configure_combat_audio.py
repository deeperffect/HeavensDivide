"""Wire generated combat audio. Back up existing assets before changing references."""
import unreal as u,json,shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(u.Paths.project_dir()).resolve();BASE='/Game/HeavensDivide/Audio';rows=json.loads((ROOT/'Tools/combat_audio_manifest.json').read_text())
backup=ROOT/'Saved/Backups/CombatAudio'/datetime.now().strftime('%Y%m%d_%H%M%S');changes=[];protected=set()
lib=u.EditorAssetLibrary;ss=u.get_engine_subsystem(u.SubobjectDataSubsystem);at=u.AssetToolsHelpers.get_asset_tools()
sounds={r['event']:u.load_asset(BASE+'/MetaSounds/MS_'+r['event']) for r in rows};assert all(sounds.values())

def protect(a):
 path=a.get_path_name().split('.')[0]
 if path in protected:return
 protected.add(path);rel=Path(path.removeprefix('/Game/')+'.uasset');src=ROOT/'Content'/rel
 if src.exists():dest=backup/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
def save(a):assert lib.save_loaded_asset(a,False)
def components(bp):
 result=[]
 for h in ss.k2_gather_subobject_data_for_blueprint(bp):
  obj=u.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(u.SubobjectDataBlueprintFunctionLibrary.get_data(h),bp)
  if obj and isinstance(obj,u.ActorComponent) and obj not in result:result.append(obj)
 return result
def assign(a,obj,field,event):
 old=obj.get_editor_property(field);new=sounds[event]
 if old==new:return
 protect(a);obj.set_editor_property(field,new);changes.append({'asset':a.get_path_name(),'object':obj.get_name(),'field':field,'old':str(old),'event':event})
def feedback(a,obj,event):
 p=obj.get_editor_property('impact_feedback');protect(a);p.set_editor_property('hit_sound',sounds[event]);obj.set_editor_property('impact_feedback',p);changes.append({'asset':a.get_path_name(),'field':'impact_feedback.hit_sound','event':event})
factory=u.DataAssetFactory();factory.set_editor_property('data_asset_class',u.CombatAudioPalette)
palette=u.load_asset(BASE+'/DA_CombatAudio') if lib.does_asset_exist(BASE+'/DA_CombatAudio') else at.create_asset('DA_CombatAudio',BASE,u.CombatAudioPalette,factory)
palette.set_editor_property('sounds',sounds);save(palette)
upgrade_events={'BladeWave':'BladeWave','SplinterWave':'SplinterWave','CrossingBlades':'CrossingBlades','ReturningBlade':'ReturningBlade','OverkillBurst':'OverkillBurst','BloodTransfer':'BloodTransfer','BleedingEdge':'BleedingEdge','VenomousKunai':'Poison','EmbeddedBlades':'EmbeddedScatter','GrandEntrance':'GrandEntrance','TagTeam':'TagTeam'}
coverage=[]
for path in lib.list_assets('/Game/HeavensDivide/Upgrades',True,False):
 a=u.load_asset(path)
 if not isinstance(a,u.UpgradeDefinition):continue
 uid=str(a.get_editor_property('upgrade_id'));event=upgrade_events.get(uid)
 if event:
  protect(a);p=a.get_editor_property('presentation');p.set_editor_property('sound',sounds[event]);p.set_editor_property('sound_volume',1.);group=next(r['group'] for r in rows if r['event']==event);p.set_editor_property('sound_concurrency',None);a.set_editor_property('presentation',p);save(a)
 coverage.append({'upgrade':uid,'runtime_event':event or ('DoubleCut' if uid=='DoubleCut' else 'inherits weapon/stance/status/clone/swap event'),'selection_event':'UpgradeSelect'})
for path in lib.list_assets('/Game/HeavensDivide/Blueprints',True,False):
 name=path.rsplit('/',1)[-1].split('.')[0]
 if not name.startswith('BP_'):continue
 if not any(k in name for k in ['Samurai','Ninja','Enemy','Projectile','Pickup','Chest','Trial','Shrine','Obelisk','Rift']):continue
 bp=u.load_asset(path)
 if not isinstance(bp,u.Blueprint):continue
 c=u.get_default_object(bp.generated_class());comps=components(bp);before=len(changes)
 if name in ('BP_Samurai','BP_Ninja'):
  hero=name.removeprefix('BP_');protect(bp);p=c.get_editor_property('combo_ability');p.set_editor_property('sound',sounds[hero+'Ability']);p.set_editor_property('pulse_sound',sounds[hero+'Pulse']);c.set_editor_property('combo_ability',p);changes.append({'asset':path,'field':'combo_ability.sound + pulse_sound','event':hero+'Ability / '+hero+'Pulse'})
  for o in comps:
   if isinstance(o,u.AutoAttackComponent) and hero=='Samurai':assign(bp,o,'impact_sound','SamuraiImpact')
   if isinstance(o,u.NinjaBuildComponent):
    for field,event in [('kunai_throw_sound','KunaiThrow'),('shuriken_throw_sound','ShurikenThrow'),('shuriken_hit_sound','ShurikenImpact')]:assign(bp,o,field,event)
   if isinstance(o,u.SwapPresentationComponent):
    assign(bp,o,'arrival_portal_sound','SwapArrival');assign(bp,o,'departure_portal_sound','SwapDeparture')
 if name in ('BP_NinjaProjectile','BP_SamuraiWaveProjectile','BP_FireballProjectile'):feedback(bp,c,{'BP_NinjaProjectile':'KunaiImpact','BP_SamuraiWaveProjectile':'BladeWaveHit','BP_FireballProjectile':'FireballImpact'}[name])
 for o in comps:
  if isinstance(o,u.EnemyDeathComponent) and name!='BP_TestDummy':
   event='DeathBoss' if 'Boss' in name else 'DeathElite' if any(k in name for k in ['Gorilla','Ogre']) else 'DeathSkeleton' if 'Skeleton' in name else 'DeathCreature' if any(k in name for k in ['Bear','Creature','Devil']) else 'DeathSmall'
   assign(bp,o,'death_sound',event);protect(bp);o.set_editor_property('death_sound_concurrency',None)
  if isinstance(o,u.ObjectiveInteractionComponent):assign(bp,o,'spawn_sound','ObjectiveSpawn')
 if isinstance(c,u.ExperiencePickup):assign(bp,c,'pickup_sound','XPPickup')
 if isinstance(c,u.HealingPickup):assign(bp,c,'healing_sound','HealPickup')
 if isinstance(c,u.EliteRewardChest):assign(bp,c,'opening_sound','ChestOpen')
 if len(changes)>before:u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
# Replace only weapon sound notifies; retain original foley and all audio source assets.
old_map={}
for path in lib.list_assets('/Game/Assets/Sounds',True,False):
 n=path.rsplit('/',1)[-1].split('.')[0];event=None
 if '/Samurai/' in path and (n.startswith('Swing') or 'Swing01' in n or 'MS_Samurai_Impact' in n):event='SamuraiImpact' if 'Hit' in n or 'Impact' in n else 'SamuraiSwing'
 elif '/Ninja/' in path:
  if 'KunaiThrow' in n or 'knife-swish' in n:event='KunaiThrow'
  elif 'MetalMotion' in n:event='ShurikenThrow'
  elif 'MetalWoosh' in n:event='ShurikenImpact'
  elif 'MS_Ninja_Impact' in n:event='KunaiImpact'
 if event:old_map[path.split('.')[0]]=event
ar=u.AssetRegistryHelpers.get_asset_registry();opts=u.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True)
refs=set()
for path in old_map:refs.update(str(x) for x in ar.get_referencers(path,opts))
for path in sorted(refs):
 a=u.load_asset(path)
 if not isinstance(a,u.AnimSequenceBase):continue
 dirty=False
 for e in u.AnimationLibrary.get_animation_notify_events(a):
  n=e.get_editor_property('notify')
  if not isinstance(n,u.AnimNotify_PlaySound):continue
  sound=n.get_editor_property('sound')
  event=old_map.get(sound.get_path_name().split('.')[0]) if sound else None
  if event:assign(a,n,'sound',event);dirty=True
 if dirty:save(a)
# Use the animation's role, not its old sound name (the Ninja throw used an impact sound).
for folder,name,event in [('Ninja','AM_AutoAttackNinja','KunaiThrow'),('Samurai','AM_DoubleCutBloodStance','DoubleCut')]:
 a=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/'+folder+'/'+name)
 for e in u.AnimationLibrary.get_animation_notify_events(a):
  n=e.get_editor_property('notify')
  if isinstance(n,u.AnimNotify_PlaySound):assign(a,n,'sound',event)
 save(a)
backup.mkdir(parents=True,exist_ok=True);(backup/'assignments.json').write_text(json.dumps(changes,indent=2))
(ROOT/'Saved/CombatAudio/assignments.json').write_text(json.dumps(changes,indent=2));(ROOT/'Saved/CombatAudio/upgrade_coverage.json').write_text(json.dumps(coverage,indent=2))
u.log('COMBAT_AUDIO_WIRED '+str(len(changes))+' assignments; '+str(len(coverage))+' upgrades reviewed')

